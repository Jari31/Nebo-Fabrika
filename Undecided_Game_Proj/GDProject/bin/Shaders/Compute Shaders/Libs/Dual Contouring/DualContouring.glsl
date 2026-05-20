#[compute]

#version 450

/*
    COPYRIGHT (c) 2026 Jari
*/

#ifndef WORKGROUP_SIZE_X
#define WORKGROUP_SIZE_X 8
#endif
#ifndef WORKGROUP_SIZE_Y
#define WORKGROUP_SIZE_Y 8
#endif
#ifndef WORKGROUP_SIZE_Z
#define WORKGROUP_SIZE_Z 8
#endif

layout(local_size_x = WORKGROUP_SIZE_X, local_size_y = WORKGROUP_SIZE_Y, local_size_z = WORKGROUP_SIZE_Z) in;
#extension GL_GOOGLE_include_directive : require

#include "res://bin/Shaders/Compute Shaders/Libs/General/PCG_GENERATION_Set1.glsl"

#include "res://bin/Shaders/Compute Shaders/Libs/General/PCG_GENERATION_PushConstant.glsl"

#include "res://bin/Shaders/Compute Shaders/Libs/Dual Contouring/DUAL_CONTOURING_Set2.glsl"


#define BOUNDARY_GENERATION_PASS 4206967
bool GENERATE_BOUNDARIES = true;

ivec3 CHUNK_SIZE = ivec3(dCHUNK_SIZE);
ivec3 VOXELS_PER_CHUNK = ivec3(dVOXELS_PER_CHUNK);

vec3 NodeMin = vec3(0);
vec3 NodeMax = vec3(0);

float MAT_ID = 0.0;
float SIZE_OFFSET = 1.0;

bool INDEX_PASS = true;

uint StorageOffset = 0;

uint WindingOrder = 0;

#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/DualContouring_Math.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/MortonCurve.glsl"

#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/OctahedralMapping.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/CompressFloat.glsl"

int FlattenCoordinates(ivec3 Coordinates)
{
    uint pGridSize = GridSize;
    if(Coordinates.x < 0 || Coordinates.y < 0 || Coordinates.z < 0) return -1; 
    if(Coordinates.x >= GridSize || Coordinates.y >= GridSize || Coordinates.z >= GridSize) return -1;
    //else if(GENERATE_BOUNDARIES) if(Coordinates.x >= GridSize - 1 || Coordinates.y >= GridSize - 1 || Coordinates.z >= GridSize - 1) return  -1;

    int flat_coords = int(Coordinates.x + (Coordinates.y * pGridSize) + (Coordinates.z * pGridSize * pGridSize));
    
    return flat_coords; 
}

float GetCornerDensities(vec3 f_point)
{
    ivec3 point = ivec3(round(f_point));

    int flat_coords = FlattenCoordinates(point);
    if(flat_coords == -1 && GENERATE_BOUNDARIES) return 0;
    //else if(flat_coords == -1) return 1e-6;

    return VoxelData[flat_coords].density;   
}

int GetCellIndex(int x, int y, int z)
{
    ivec3 xyz = ivec3(x, y, z);
    int flat_coords = FlattenCoordinates(xyz);
    
    if(flat_coords == -1)
        return -1;

    int Index = Node_VertexIndex[flat_coords];
    return Index;
}

ivec2 GetFlattenedCoordinateIndex(ivec3 index)
{
    int flat_coords = GetCellIndex(index.x, index.y, index.z);
    
    if(flat_coords == -1)
        return ivec2(-1, -1);

    ivec2 flat_coords_xy;
    flat_coords_xy.x = flat_coords % dCHUNK_SIZE.w;
    flat_coords_xy.y = flat_coords / dCHUNK_SIZE.w;

    return flat_coords_xy;
}

vec4 GetCellVertex(ivec3 index, bool get_from_buffer_b)
{
    ivec2 flat_coords_xy = GetFlattenedCoordinateIndex(index) + ivec2(StorageOffset, StorageOffset);
    if(flat_coords_xy.x == -1)
        return vec4(-1.0, -1.0, -1.0, -1.0);
    vec4 Vertex = (get_from_buffer_b == true) ? imageLoad(VertexTexture_B, flat_coords_xy) : imageLoad(VertexTexture, flat_coords_xy);
    if(isnan(Vertex.x) || isinf(Vertex.x))
        return vec4(-1.0, -1.0, -1.0, -1.0);
    return Vertex;
}

void SetCellVertex(ivec3 index, vec4 pos, bool set_to_buffer_b)
{
    ivec2 flat_coords_xy = GetFlattenedCoordinateIndex(index) + ivec2(StorageOffset, StorageOffset);
    if(flat_coords_xy.x == -1)
        return;
     
    if(set_to_buffer_b == true) imageStore(VertexTexture_B, flat_coords_xy, pos); else imageStore(VertexTexture, flat_coords_xy, pos);
}


vec3 GetCellNormal(ivec3 index)
{
    ivec2 flat_coords_xy = GetFlattenedCoordinateIndex(index) + ivec2(StorageOffset, StorageOffset);
    if(flat_coords_xy.x == -1)
        return vec3(-2.0, -2.0, -2.0);

    vec4 Normal = imageLoad(NormalTexture, flat_coords_xy);
    return Normal.xyz;
}


vec3 GetAverageNeighborPos(ivec3 base_pos, bool get_from_buffer_b)
{
    int vert_sum_count = 0;
    vec3 neighbor_pos_sum = vec3(0);

    ivec3 PositionOffsets[6] = {
        ivec3(1, 0, 0), ivec3(0, 1, 0), ivec3(0, 0, 1),
        ivec3(-1, 0, 0), ivec3(0, -1, 0), ivec3(0, 0, -1)
    };

    for(int i = 0; i < 6; i++)
    {
        ivec3 neighbor_pos = base_pos + PositionOffsets[i];

        vec4 neighbor_vertex_pos = GetCellVertex(neighbor_pos, get_from_buffer_b);
        if(neighbor_vertex_pos.w != -1.0f)
        {
            neighbor_pos_sum += neighbor_vertex_pos.xyz;
            vert_sum_count++;
        }
    }

    return (vert_sum_count > 0) ? neighbor_pos_sum / vert_sum_count : (GetCellVertex(base_pos, get_from_buffer_b).xyz);
}

uvec3 CalculateTriEdgeBudget(vec4 P0, vec4 P1, vec4 P2, uint MaxEdgeThreadBudget)
{
    Triangle tri;
    
    tri.EdgeBudget[0] = clamp(distance(P0, P1) / VertexIntervalOnEdge, 1, MaxEdgeThreadBudget);
    
    tri.EdgeBudget[1] = clamp(distance(P1, P2) / VertexIntervalOnEdge, 1, MaxEdgeThreadBudget);

    tri.EdgeBudget[2] = clamp(distance(P2, P0) / VertexIntervalOnEdge, 1, MaxEdgeThreadBudget);

    return uvec3(tri.EdgeBudget[0], tri.EdgeBudget[1], tri.EdgeBudget[2]);
}

void StoreIndices_Tri(uint V0, uint V1, uint V2)
{
    uint AtomicIndex = atomicAdd(AtomicCounter2, 3);

    store_index(AtomicIndex + 0u, V0);
    store_index(AtomicIndex + 1u, V1);
    store_index(AtomicIndex + 2u, V2);
}

void StoreIndices(uint V0, uint V1, uint V2, uint V3)
{
    if(V0 < 0 || V1 < 0 || V2 < 0 || V3 < 0){
        //atomicAdd(AtomicCounter, 6);
        return;
    }
    
    if(WriteToTexturesInFirstPass != 0){
    uint AtomicIndex = atomicAdd(AtomicCounter2, 6);

    store_index(AtomicIndex + 0u, V0);
    store_index(AtomicIndex + 1u, V1);
    store_index(AtomicIndex + 2u, V2);

    store_index(AtomicIndex + 3u, V0);
    store_index(AtomicIndex + 4u, V2);
    store_index(AtomicIndex + 5u, V3);
    return;
    }

    uint AtomicIndex = atomicAdd(AtomicCounter, 2);

    uint MaxEdgeThreadBudget = uint(round(float(ThreadAllocationPerTriangle) / float(VertexAllocationForEdges))); // so huge triangles don't get all the threads allocated to them
    // VertexAllocationForEdges MUST be 4. 4 as in quads. if not, the index generation pass will get 2x complicated. e.g., 256 / 4 = 64
    // 64 x 3 (3 edges) = 192 (interior/surface vertices) = perfect subdivision

    vec4 P0 = VertexBuffer[V0];
    vec4 P1 = VertexBuffer[V1];
    vec4 P2 = VertexBuffer[V2];
    vec4 P3 = VertexBuffer[V3];

    Triangle tri;
    tri.VIndex[0] = V0;
    tri.VIndex[1] = V1;
    tri.VIndex[2] = V2;

    uvec3 TempTri = CalculateTriEdgeBudget(P0, P1, P2, MaxEdgeThreadBudget);

    tri.EdgeBudget[0] = TempTri.x;
    tri.EdgeBudget[1] = TempTri.y;
    tri.EdgeBudget[2] = TempTri.z;

    vec3 U = P1.xyz - P0.xyz;
    vec3 V = P2.xyz - P0.xyz;

    tri.OriginNormal = vec4(cross(U, V), 0);

    TriangleBuffer[AtomicIndex] = tri;

    tri.VIndex[1] = V2;
    tri.VIndex[2] = V3; 

    TempTri = CalculateTriEdgeBudget(P0, P2, P3, MaxEdgeThreadBudget);

    tri.EdgeBudget[0] = TempTri.x; // p2, p0
    tri.EdgeBudget[1] = TempTri.y;
    tri.EdgeBudget[2] = TempTri.z;

    U = P0.xyz - P2.xyz;
    V = P3.xyz - P2.xyz;

    tri.OriginNormal = vec4(cross(U, V), 0);

    TriangleBuffer[AtomicIndex + 1u] = tri;
}

vec3 CalculateSobelNormals(vec3 f_point)
{
    ivec3 point = ivec3(round(f_point));
    vec3 Normals = vec3(0);

    for(int x = -1; x <= 1; x++){
        for(int y = -1; y <= 1; y++){
            for(int z = -1; z <= 1; z++){
                float d = GetCornerDensities(point + ivec3(x, y, z)); 

                int weight = (x == 0 ? 2 : 1) * (y == 0 ? 2 : 1) * (z == 0 ? 2 : 1);

                Normals.x += float(x) * float(weight) * d;
                Normals.y += float(y) * float(weight) * d;
                Normals.z += float(z) * float(weight) * d;
            }
        }
    }

    return normalize(Normals);
}

float CalculateCentralDifference(float center_density, float f_point_axis, float point_axis, int di0, int di1)
{
    float n;

    float d0 = (di0 != -1) ? VoxelData[di0].density : 0.0;
    float d1 = (di1 != -1) ? VoxelData[di1].density : 0.0;

    if(di0 != -1 && di1 != -1)
    {
        return n = (d0 - d1) * 0.5;
    }
    else if(di0 != -1)
    {
        return n = d0 - center_density;
    }
    else if(di1 != -1)
    {
        return n = center_density - d1;
    }

    return 0.0f;
}

vec3 CalculateNormals(vec3 f_point)
{
    ivec3 point = ivec3(round(f_point));
    vec3 Normal = vec3(0.0, 0.0, 0.0);
    
    float center_density = GetCornerDensities(point);

    int di0 = FlattenCoordinates(point + ivec3(1, 0, 0));
    int di1 = FlattenCoordinates(point + ivec3(-1, 0, 0)); // di = density index

    Normal.x = CalculateCentralDifference(center_density, f_point.x, point.x, di0, di1);

    di0 = FlattenCoordinates(point + ivec3(0, 1, 0));
    di1 = FlattenCoordinates(point + ivec3(0, -1, 0));

    Normal.y = CalculateCentralDifference(center_density, f_point.y, point.y, di0, di1);

    di0 = FlattenCoordinates(point + ivec3(0, 0, 1));
    di1 = FlattenCoordinates(point + ivec3(0, 0, -1));

    Normal.z = CalculateCentralDifference(center_density, f_point.z, point.z, di0, di1);

    float Length = length(Normal); 
    if(Length > 0.001f){
        return Normal;
    }
    return vec3(0, 0, 0);
}

vec3 GetOctNormals(vec3 f_point)
{
    ivec3 point = ivec3(round(f_point));

    vec2 packed_normals = VoxelData[FlattenCoordinates(point)].normals_packed_oct;

    //vec2 unpacked_normals;
    //unpacked_normals.x = decompress_float_normalized_uint16(packed_normals & 0xFFFF);
    //unpacked_normals.y = decompress_float_normalized_uint16((packed_normals >> 16) & 0xFFFF);

    return unpack_normal_oct(packed_normals);
}

float CalculateTrilinearNormals(vec3 point)
{
    ivec3 base_p = ivec3(round(point));
    vec3 frac_pos = point - base_p;
    
    float d000 = GetCornerDensities(base_p + ivec3(0,0,0));
    float d100 = GetCornerDensities(base_p + ivec3(1,0,0));
    float d010 = GetCornerDensities(base_p + ivec3(0,1,0));
    float d110 = GetCornerDensities(base_p + ivec3(1,1,0));
    float d001 = GetCornerDensities(base_p + ivec3(0,0,1));
    float d101 = GetCornerDensities(base_p + ivec3(1,0,1));
    float d011 = GetCornerDensities(base_p + ivec3(0,1,1));
    float d111 = GetCornerDensities(base_p + ivec3(1,1,1));

    float ip_x0 = mix(d000, d100, frac_pos.x); // ip_x = interpolated point x
    float ip_x1 = mix(d010, d110, frac_pos.x);
    float ip_x2 = mix(d001, d101, frac_pos.x);
    float ip_x3 = mix(d011, d111, frac_pos.x);
    float ip_y0 = mix(ip_x0, ip_x1, frac_pos.y);
    float ip_y1 = mix(ip_x2, ip_x3, frac_pos.y);
    return mix(ip_y0, ip_y1, frac_pos.z);
}

vec3 CalculateNormalGradient(vec3 point)
{
    float SizeOffset = VoxelSize;
    vec3 gradient;

    gradient.x = (CalculateTrilinearNormals(point + vec3(SizeOffset,0,0)) - CalculateTrilinearNormals(point - vec3(SizeOffset,0,0))) / (2.0*SizeOffset);
    gradient.y = (CalculateTrilinearNormals(point + vec3(0, SizeOffset, 0)) - CalculateTrilinearNormals(point - vec3(0,SizeOffset,0))) / (2.0*SizeOffset);
    gradient.z = (CalculateTrilinearNormals(point + vec3(0, 0, SizeOffset)) - CalculateTrilinearNormals(point - vec3(0,0,SizeOffset))) / (2.0*SizeOffset);

    return normalize(gradient);
}

bool OutOfBoundsCheck(vec3 point)
{
    if(point.x >= GridSize || point.y >= GridSize || point.z >= GridSize) return true;
    if(point.x <  0        || point.y <  0        || point.z <  0       ) return true;
    return false;
}

vec3 SolveCholeskyQEF(vec3 IntersectionNormals[12], vec3 IntersectionPoints[12], 
                vec3 Centroid, uint EdgeMask, vec3 AverageNormals)
{
    mat3 ATA = mat3(0.0);
    vec3 ATb = vec3(0.0);

    float p_count = 0;

    float n_divergence = 0;

    for(int i = 0; i < 12; i++)
    {
        if((EdgeMask & (1u << i)) == 0u) continue;

        n_divergence += (1.0 - dot(AverageNormals, IntersectionNormals[i]));

        p_count++;
    }

    if(p_count < 2) return Centroid;

    for(int i = 0; i < 12; i++)
    {
        if((EdgeMask & (1u << i)) == 0u) continue;
        
        vec3 n = IntersectionNormals[i];
        vec3 p = IntersectionPoints[i];
        
        n_divergence += (1.0 - dot(AverageNormals, n));
        p_count++;
        
        ATA[0][0] += n.x * n.x;
        ATA[0][1] += n.x * n.y;
        ATA[0][2] += n.x * n.z;
        
        ATA[1][1] += n.y * n.y;
        ATA[1][2] += n.y * n.z;
        
        ATA[2][2] += n.z * n.z;
        
        ATb += n * dot(n, p);
    }

    ATA[1][0] = ATA[0][1];
    ATA[2][0] = ATA[0][2];
    ATA[2][1] = ATA[1][2];

    float Lambda = 0.0001;

    vec3 Bias = Centroid;
    ATA[0][0] += Lambda;
    ATA[1][1] += Lambda;
    ATA[2][2] += Lambda;
    ATb += Bias * Lambda;
    
    if(determinant(ATA) < 1e-8) return Centroid;

    vec3 result;
    float L11 = sqrt(ATA[0][0]);
    float L21 = ATA[1][0] / L11;
    float L31 = ATA[2][0] / L11;

    //if(L11 < 0 || L21 < 0 || L31 < 0) return Centroid;

    float L22 = sqrt(ATA[1][1] - L21 * L21);
    float L32 = (ATA[2][1] - L31 * L21)/L22;
    
    float L33 = sqrt(ATA[2][2] - (L31 * L31 + L32 * L32));

    vec3 forward_subs;
    forward_subs.x = ATb.x / L11;
    forward_subs.y = (ATb.y - (L21 * forward_subs.x)) / L22;
    forward_subs.z = (ATb.z - (L31 * forward_subs.x + L32 * forward_subs.y)) / L33;
    
    // backward substitution
    result.z = forward_subs.z / L33;
    result.y = (forward_subs.y - (L32 * result.z)) / L22;
    result.x = (forward_subs.x - (L21 * result.y + L31 * result.z)) / L11;

    if(length(result - Centroid) > 2)
        return Centroid;

    //if(isinf(result.x) || isinf(result.y) || isinf(result.z)) return Centroid;

    float StabilityThreshold = 0.2;
    float t = clamp(n_divergence / StabilityThreshold, 0.0, 1.0);

    return result;
}

void SmoothVertexPositions()
{
    if(PassOffset <= 3){
        if(PassOffset == 3)
        {
            vec4 Vertex = GetCellVertex(ivec3(gl_GlobalInvocationID.xyz), true);
            if(Vertex.w == -1) return;
            SetCellVertex(ivec3(gl_GlobalInvocationID.xyz), Vertex, true);
            return;
        }
        
        float SmoothFactor = 0.5;

        bool GetFromBufferB = (PassOffset == 1) ? false : true;
        bool TransferToBufferB = (GetFromBufferB) ? true : false;
        
        vec4 ParentVertexPos = GetCellVertex(ivec3(gl_GlobalInvocationID.xyz), GetFromBufferB);
        if(ParentVertexPos.w == -1.0f)
            return;
        
        vec3 ParentVertexNormal = GetCellNormal(ivec3(gl_GlobalInvocationID.xyz));
        if(ParentVertexNormal.x == -2.0f)
            return;
        
        vec3 AverageNeighborPos = GetAverageNeighborPos(ivec3(gl_GlobalInvocationID.xyz), GetFromBufferB);
        if(AverageNeighborPos.x == -1.0f)
            return;
        vec3 AverageDirection = AverageNeighborPos - (ParentVertexPos.xyz);

        float Projection = dot(AverageDirection, ParentVertexNormal);

        SmoothFactor = min(0.1, max(SmoothFactor, abs(Projection) * 2.0));
        
        ParentVertexPos.xyz += ParentVertexNormal * Projection * SmoothFactor;
        SetCellVertex(ivec3(gl_GlobalInvocationID.xyz), ParentVertexPos, TransferToBufferB);
    }
}

void main() 
{
    uint TotalNodes = Dense_TotalNodes - 1;
    //{ uvec3 Index = gl_GlobalInvocationID.xyz; if(Index.x > GridSize || Index.y > GridSize || Index.z > GridSize) return; }
    
    float SizeOffset = 1;
    SIZE_OFFSET = SizeOffset;
    float CellSize   = 1;

    StorageOffset = (VertexOffsetLoD.w > 0) ? VertexOffsets[VertexOffsetLoD.w] : 0;

    if(PassOffset == 0)
    {
        INDEX_PASS = false;
        int i_Index = int(FlattenCoordinates(ivec3(gl_GlobalInvocationID.xyz)));
        //if(i_Index == -1) return;
        uint Index = uint(i_Index);

        uvec3 InvocationID = gl_GlobalInvocationID.xyz;

        Node_VertexIndex[Index] = -1; 
        Node_EdgeMask[Index]    = 0;

        NodeMin = vec3(InvocationID) * SizeOffset;
        NodeMax =  vec3(InvocationID + 1) * SizeOffset;

        ivec3 ivec3_NodeMin = ivec3(round(NodeMin));

        // edge configuration
        vec3 Corners[8];

        /*if(PassNum > 1)
        {
        Corners[0] = ivec3_NodeMin + vec3(0, 0, 0);
        Corners[1] = ivec3_NodeMin + vec3(1, 0, 0);
        Corners[2] = ivec3_NodeMin + vec3(0, 1, 0);
        Corners[3] = ivec3_NodeMin + vec3(1, 1, 0);
        Corners[4] = ivec3_NodeMin + vec3(0, 0, 1);
        Corners[5] = ivec3_NodeMin + vec3(1, 0, 1);
        Corners[6] = ivec3_NodeMin + vec3(0, 1, 1);
        Corners[7] = ivec3_NodeMin + vec3(1, 1, 1);
        }
        else*/
        {
        Corners[0] = ivec3_NodeMin + vec3(-SizeOffset, -SizeOffset, -SizeOffset);
        Corners[1] = ivec3_NodeMin + vec3(0,          -SizeOffset, -SizeOffset);
        Corners[2] = ivec3_NodeMin + vec3(-SizeOffset, 0,           -SizeOffset);
        Corners[3] = ivec3_NodeMin + vec3(0,          0,           -SizeOffset);
        Corners[4] = ivec3_NodeMin + vec3(-SizeOffset, -SizeOffset, 0         );
        Corners[5] = ivec3_NodeMin + vec3(0,          -SizeOffset, 0         );
        Corners[6] = ivec3_NodeMin + vec3(-SizeOffset, 0,           0         );
        Corners[7] = ivec3_NodeMin;
        }

        float CornerDensities[8];
        CornerDensities[0] = GetCornerDensities(Corners[0]);
        CornerDensities[1] = GetCornerDensities(Corners[1]);
        CornerDensities[2] = GetCornerDensities(Corners[2]);
        CornerDensities[3] = GetCornerDensities(Corners[3]);
        CornerDensities[4] = GetCornerDensities(Corners[4]);
        CornerDensities[5] = GetCornerDensities(Corners[5]);
        CornerDensities[6] = GetCornerDensities(Corners[6]);
        CornerDensities[7] = GetCornerDensities(Corners[7]);

        uint Empty_Full = 0;
        for(int i = 0; i < 8; i++) 
        {
            if(CornerDensities[i] > 0.0f)
                Empty_Full |= 1u << i;
            else if(CornerDensities[i] < 0.0f)
                Empty_Full |= 1u << (i + 8);
            else if(CornerDensities[i] == 0.0f)
                Empty_Full |= 1u << (i + 16);
        }

        if((Empty_Full & 0xFF) == 255u || (Empty_Full & 0xFF00) == 255u || (Empty_Full & 0xFF0000) == 255u) return;
    
        const int EdgeCorners[12][2] = {
        // Edges on the bottom face (Z = -1)
        {0, 1}, {1, 3}, {3, 2}, {2, 0}, 
        // Edges on the top face (Z = 0)
        {4, 5}, {5, 7}, {7, 6}, {6, 4},
        // Vertical pillars connecting bottom to top
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
        };

        vec3 Normals = vec3(0.0);
        
        int IntersectionCount = 0;
        float IntersectionWeight = 0.0f;
        vec3 IntersectionSum = vec3(0.0);
        vec3 IntersectionNormals[12];
        vec3 IntersectionPoints[12];
        float IntersectionSigns[12];

        uvec3 DEBUG_intersect_centroid = uvec3(0.0f);

        uint EdgeMask = 0;
        for(int k = 0; k < 12; k++)
        {
            int i0   = EdgeCorners[k][0]; // i = index
            int i1   = EdgeCorners[k][1];
            float d0 = CornerDensities[i0]; // d = density
            float d1 = CornerDensities[i1];
        
            if(d0 * d1 < 0.0)
            {
                float denom = d1 - d0;
                float t = 0;
                if(denom > 0.001)
                t = -d0 / denom;

                vec3 n = vec3(0.0);
                vec3 p;

                vec3 CornerA = Corners[i0];
                vec3 CornerB = Corners[i1];

                p = mix(CornerA, CornerB, t);

                //vec3 CornerA_n = CalculateNormals(CornerA);
                //vec3 CornerB_n = CalculateNormals(CornerB);
                
                n = GetOctNormals(p);//CalculateNormals(p);

                if(isnan(n.x) || isnan(n.y) || isnan(n.z) || isinf(n.x) || isinf(n.y) || isinf(n.z))
                    continue;

                Normals += n;

                float weight = dot(n, n); // magnitude weight

                IntersectionSum += p * weight;
                IntersectionCount++;
                IntersectionWeight += weight;

                IntersectionNormals[k] = n;
                IntersectionPoints[k] = p;

                EdgeMask |= (1u << k);
                if(d0 > d1)
                    EdgeMask |= (1u << (k + 12));

                //DEBUG_intersect_centroid = uvec3(t * 255.0, 0, 0);
            }
        }
        
        vec3 Centroid = vec3(0.0, 0.0, 0.0); // or as an alias, position

        if(IntersectionCount > 0)
        {
            if(IntersectionWeight > 0.0f)
            Centroid = IntersectionSum / float(IntersectionWeight);
            else
            Centroid = (NodeMin + NodeMax) / 2;

            Normals /= IntersectionCount;
            if(isnan(Normals.x) || isnan(Normals.y) || isnan(Normals.z))
                return;
            Normals = normalize(Normals);
            
            const uint MAX_ITERATIONS = PassNum;

            //if(GridSize > 32)
            Centroid = SolveCholeskyQEF(IntersectionNormals, IntersectionPoints, Centroid, EdgeMask, Normals);
            
            //uint DEBUG_color_packed = 0; DEBUG_color_packed |= ((DEBUG_intersect_centroid.x << 16u) | (DEBUG_intersect_centroid.y << 8u) | DEBUG_intersect_centroid.z);

            vec4 Normal = vec4(Normals, 1.0);
            MAT_ID = VoxelData[Index].matID;
            
            vec4 Vertex = vec4(Centroid * VoxelSize, MAT_ID) + vec4(VertexOffsetLoD.xyz, 0);
            
            uint VertexIndex = store_vertices_and_normals(Vertex, Normal);

            Node_VertexIndex[Index] = int(VertexIndex);
            Node_EdgeMask[Index]    = EdgeMask;
        }

        return;
    }
    int i_Index = int(FlattenCoordinates(ivec3(gl_GlobalInvocationID.xyz)));
    //if(i_Index == -1) return;
    uint Index = uint(i_Index);
    
    // post processing
    if(PassOffset < 2147483647)
    {
        switch (PassOffset)
        {

        default:
            SmoothVertexPositions();
        break;

        case 4:
            if(Index != 0)
                return;

            indirect_dispatch_params.TriangleCount = AtomicCounter;

            float ThreadsPerTriangle = ceil(float(AtomicCounter) * float(ThreadAllocationPerTriangle));
            ThreadsPerTriangle /= TrianglesProcessedPerThread;
            uint i = uint(ceil((ThreadsPerTriangle / 512.0f)));
            VertexCounter = 0;
            
            if(i <= 0 || isnan(i))
                i = 1;

            indirect_dispatch_params.x = i; indirect_dispatch_params.y = i; indirect_dispatch_params.z = i;
        break;
        
        case 5:
            //atomicAdd(VertexCounter, indirect_dispatch_params.x);
            Index = (TrianglesProcessedPerThread > 1 && Index > 0) ? Index + TrianglesProcessedPerThread : Index;
            uint PrevIndex = Index + TrianglesProcessedPerThread;
            for(Index = Index; Index < PrevIndex; Index++){
            if(Index >= indirect_dispatch_params.TriangleCount) return;

            Triangle OriginTriangle = TriangleBuffer[(Index > 0) ? Index / ThreadAllocationPerTriangle : 0];
            uint StartingLocalIndex = Index % ThreadAllocationPerTriangle;
            uint LocalIndex = (StartingLocalIndex != 0) ? StartingLocalIndex + VerticesPerThread : 0;

            vec4 P0 = VertexBuffer[OriginTriangle.VIndex[0]];
            vec4 P1 = VertexBuffer[OriginTriangle.VIndex[1]];
            vec4 P2 = VertexBuffer[OriginTriangle.VIndex[2]];

            vec4 N0 = NormalBuffer[OriginTriangle.VIndex[0]];
            vec4 N1 = NormalBuffer[OriginTriangle.VIndex[1]];
            vec4 N2 = NormalBuffer[OriginTriangle.VIndex[2]];

            //vec3 EdgeSmooth_01 = P0 + (project_on_plane(P1 - P0, N0) / 3.0);
            //vec3 EdgeSmooth_10 = P1 - (project_on_plane(P1 - P0, N1) / 3.0);

           // vec3 EdgeSmooth_02 = P1 +  

            int TotalVertices = int(VerticesPerThread);
            TotalVertices = (isnan(TotalVertices) || TotalVertices <= 0) ? 3 : TotalVertices;
            float Segments = ceil((float(sqrt(1 + 8 * TotalVertices) - 3) / 2.0));

            uint VertexIndices[1024];

            for(uint row = 0; row <= uint(Segments); row++)
            {
                for(uint column = 0; column <= row; column++)
                {
                LocalIndex++;
                //uint row = uint((sqrt(1.0 + 8.0 * float(LocalIndex)) - 1.0) * 0.5);
                //uint column = uint(LocalIndex - (row * (row + 1))/2);

                float progress = (row > 0) ? float(column) / float(row) : 0.0;

                vec3 Vertex = vec3(0,0,0);
                
                float _v = float(row) / Segments;
                float _w = _v * progress;
                
                float u = 1.0 - _v;
                float v = _v - _w;
                float w = _w;/*
                
                if(max(max(u, v), w) >= 0.8)
                {
                    // exterior vertices 
                    vec4 LocalVertexArray[] = vec4[](P0, P1, P2);
                    
                    uint OwnerEdge = 0;
                    if(LocalIndex > OriginTriangle.EdgeBudget[0]){
                        if(LocalIndex > OriginTriangle.EdgeBudget[1])
                            OwnerEdge = 2;
                        else
                            OwnerEdge = 1;
                    }

                    float LocalDistribution = OriginTriangle.EdgeBudget[OwnerEdge];

                    float Step = float(LocalIndex % uint(round(LocalDistribution)));
                
                    vec3 StartingPoint = LocalVertexArray[OwnerEdge].xyz;
                    vec3 EndPoint = LocalVertexArray[(OwnerEdge == 2) ? OriginTriangle.VIndex[0] : OriginTriangle.VIndex[OwnerEdge + 1]].xyz;

                    float Length = distance(StartingPoint, EndPoint);

                    float t = Step / Length;

                    Vertex = StartingPoint + t * (EndPoint - StartingPoint);
                }*/
                //else
                {
                    Vertex = P0.xyz * u + P1.xyz * v + P2.xyz * w;
                    // interior
                }
                VertexIndices[LocalIndex] = store_vertices_and_normals(vec4(Vertex, 1), vec4(1, 1, 1, 1));
                }
            }
            
            uint counter = 0;
            for(uint row = 0; row < uint(Segments); row++)
            {
                uint CurrentRowStart = (row * (row + 1)) / 2;
                uint NextRowStart = ((row + 1) * (row + 2)) / 2;
                
                for(uint column = 0; column <= row; column++)
                {
                    uint TopLeft_Grid = CurrentRowStart + column;
                    uint TopRight_Grid = TopLeft_Grid + 1;
                    uint BottomLeft_Grid = NextRowStart + column;
                    uint BottomRight_Grid = BottomLeft_Grid + 1;

                    uint TopLeft = VertexIndices[TopLeft_Grid];
                    uint TopRight = VertexIndices[TopRight_Grid];
                    uint BottomLeft = VertexIndices[BottomLeft_Grid];
                    uint BottomRight = VertexIndices[BottomRight_Grid];

                    if(counter >= LocalIndex || isnan(TopRight) || isnan(BottomRight) || isnan(TopLeft) || isnan(BottomLeft)) return;

                    //if(column < row)
                       StoreIndices(TopLeft, TopRight, BottomRight, BottomLeft);
                    //else
                        //StoreIndices_Tri(TopLeft, BottomRight, BottomLeft);

                    counter++;
                }
            };

            }
        break;
        
        case 666:
            //
        break;
        }

        return;
    }
    
    // triangle emission 
    int MC_x = int(gl_GlobalInvocationID.x); // mc = minimum corner
    int MC_y = int(gl_GlobalInvocationID.y);
    int MC_z = int(gl_GlobalInvocationID.z);
    ivec3 MC_xyz = ivec3(MC_x, MC_y, MC_z);

    int f = -1;

    const ivec3 EdgeOffsets[3][4] =
    {
        {ivec3(0, 0, 0), ivec3(0, f, 0), ivec3(0, 0, f), ivec3(0, f, f)}, // X
        {ivec3(0, 0, 0), ivec3(f, 0, 0), ivec3(0, 0, f), ivec3(f, 0, f)}, // Y
        {ivec3(0, 0, 0), ivec3(f, 0, 0), ivec3(0, f, 0), ivec3(f, f, 0)} // Z
    };

    // {} x -> y -> z
    uint EdgeMask = uint(Node_EdgeMask[Index]);
    if(EdgeMask == 0) return;

    //Vn = Vertex_num
    int V0 = -1, V1 = -1, V2 = -1, V3 = -1;

    V0 = Node_VertexIndex[Index];
    if(V0 == -1) return;

    if((EdgeMask & 1u) != 0) // X
    {
        V1 = GetCellIndex(MC_x - 0, MC_y - 1, MC_z - 0);
        if(V1 > -1)
            V2 = GetCellIndex(MC_x - 0, MC_y - 1, MC_z - 1);
        if(V2 > -1)
            V3 = GetCellIndex(MC_x - 0, MC_y + 0, MC_z - 1);

        if((EdgeMask & (1u << (0 + 12))) != 0){
            WindingOrder = 0;
            StoreIndices(V0, V1, V2, V3);
        }
        else{
            WindingOrder = 1;
            StoreIndices(V0, V3, V2, V1);
        }
    }
    V1 = -1, V2 = -1, V3 = -1;
    
    if ((EdgeMask & (1u << 3)) != 0) // Y
    {
        V1 = GetCellIndex(MC_x - 0, MC_y - 0, MC_z - 1);

        if(V1 > -1)
            V2 = GetCellIndex(MC_x - 1, MC_y + 0, MC_z - 1);

        if(V2 > -1)
            V3 = GetCellIndex(MC_x - 1, MC_y + 0, MC_z + 0);

        if((EdgeMask & (1u << (3 + 12))) != 0){
            WindingOrder = 1;
            StoreIndices(V0, V3, V2, V1);
        }
        else{
            WindingOrder = 0;
            StoreIndices(V0, V1, V2, V3);
        }
    }
    V1 = -1, V2 = -1, V3 = -1;
    
    if((EdgeMask & (1u << 8)) != 0) // Z
    {
        V1 = GetCellIndex(MC_x - 1, MC_y - 0, MC_z - 0);

        if(V1 > -1)
            V2 = GetCellIndex(MC_x - 1, MC_y - 1, MC_z - 0);

        if(V2 > -1)
            V3 = GetCellIndex(MC_x - 0, MC_y - 1, MC_z - 0);
            
        if((EdgeMask & (1u << (8 + 12))) != 0){
            WindingOrder = 0;
            StoreIndices(V0, V1, V2, V3);
        }
        else{
            WindingOrder = 1;
            StoreIndices(V0, V3, V2, V1);
        }
    }
}