#[compute]

#version 450

/*
    COPYRIGHT (c) 2026 Jari
    Licensed under the MIT license. Refer to the license file provided within the README for details.
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

ivec3 CHUNK_SIZE = ivec3(dCHUNK_SIZE);
ivec3 VOXELS_PER_CHUNK = ivec3(dVOXELS_PER_CHUNK);

float MAT_ID = 0.0;
float SIZE_OFFSET = 1.0;

bool INDEX_PASS = true;

#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/DualContouring_Math.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/MortonCurve.glsl"


int FlattenCoordinates(ivec3 Coordinates)
{
    uint pGridSize = GridSize;
    if(Coordinates.x < 0 || Coordinates.y < 0 || Coordinates.z < 0) return -1; 
    if(Coordinates.x >= GridSize || Coordinates.y >= GridSize || Coordinates.z >= GridSize) return -1;

    int flat_coords = int(Coordinates.x + (Coordinates.y * pGridSize) + (Coordinates.z * pGridSize * pGridSize));
    
    return flat_coords; 
}

float GetCornerDensities(vec3 f_point)
{
    ivec3 point = ivec3(round(f_point));

    int flat_coords = FlattenCoordinates(point);
    if(flat_coords == -1) return 1e-6;
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
    ivec2 flat_coords_xy = GetFlattenedCoordinateIndex(index);
    if(flat_coords_xy.x == -1)
        return vec4(-1.0, -1.0, -1.0, -1.0);
    vec4 Vertex = (get_from_buffer_b == true) ? imageLoad(VertexTexture_B, flat_coords_xy) : imageLoad(VertexTexture, flat_coords_xy);
    if(isnan(Vertex.x) || isinf(Vertex.x))
        return vec4(-1.0, -1.0, -1.0, -1.0);
    return Vertex;
}

void SetCellVertex(ivec3 index, vec4 pos, bool set_to_buffer_b)
{
    ivec2 flat_coords_xy = GetFlattenedCoordinateIndex(index);
    if(flat_coords_xy.x == -1)
        return;
     
    if(set_to_buffer_b == true) imageStore(VertexTexture_B, flat_coords_xy, pos); else imageStore(VertexTexture, flat_coords_xy, pos);
}


vec3 GetCellNormal(ivec3 index)
{
    ivec2 flat_coords_xy = GetFlattenedCoordinateIndex(index);
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

/*
void store_index(uint flat_idx, uint vertex_val) {
    int x = int(flat_idx % uint(dVOXELS_PER_CHUNK.w));
    int y = int(flat_idx / uint(dVOXELS_PER_CHUNK.w));
    imageStore(IndexTexture, ivec2(x, y), vec4(float(vertex_val), 0, 0, 0));
}
*/

void StoreIndices(int V0, int V1, int V2, int V3)
{
    if(V0 < 0 || V1 < 0 || V2 < 0 || V3 < 0){
        //atomicAdd(AtomicCounter, 6);
        return;
    }
    
    uint AtomicIndex = atomicAdd(AtomicCounter2, 6);
    
    store_index(AtomicIndex + 0u, V0);
    store_index(AtomicIndex + 1u, V1);
    store_index(AtomicIndex + 2u, V2);

    store_index(AtomicIndex + 3u, V0);
    store_index(AtomicIndex + 4u, V2);
    store_index(AtomicIndex + 5u, V3);
}

float CalculateCentralDifference(float center_density, float f_point_axis, float point_axis, int di0, int di1)
{
    float n;

    float d0 = (di0 != 1) ? VoxelData[di0].density : 0.0;
    float d1 = (di1 != 1) ? VoxelData[di1].density : 0.0;

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
        //Normal /= vec3(2.0, 2.0, 2.0);
        return Normal;
    }
    return vec3(center_density, center_density, center_density);
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
    
    /*
    float d000 = VoxelData[FlattenCoordinates(base_p + ivec3(0,0,0))].density;
    float d100 = VoxelData[FlattenCoordinates(base_p + ivec3(1,0,0))].density;
    float d010 = VoxelData[FlattenCoordinates(base_p + ivec3(0,1,0))].density;
    float d110 = VoxelData[FlattenCoordinates(base_p + ivec3(1,1,0))].density;
    float d001 = VoxelData[FlattenCoordinates(base_p + ivec3(0,0,1))].density;
    float d101 = VoxelData[FlattenCoordinates(base_p + ivec3(1,0,1))].density;
    float d011 = VoxelData[FlattenCoordinates(base_p + ivec3(0,1,1))].density;
    float d111 = VoxelData[FlattenCoordinates(base_p + ivec3(1,1,1))].density;
    */
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
    float SizeOffset = 1;
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

/*
carbon-copy of the sparse solver in some parts, but it's like different in such tiny ways, 
that separating parts of it into its own compute shader will take more work than just copy pasting parts after modifying it
*/

void main() 
{
    uint TotalNodes = Dense_TotalNodes - 1;
    { uvec3 Index = gl_GlobalInvocationID.xyz; if(Index.x > GridSize || Index.y > GridSize || Index.z > GridSize) return; }
    
    float SizeOffset = 1;
    SIZE_OFFSET = SizeOffset;
    float CellSize   = 1;

    if(PassOffset == 0 || PassOffset == BOUNDARY_GENERATION_PASS)
    {
        INDEX_PASS = false;
        int i_Index = int(FlattenCoordinates(ivec3(gl_GlobalInvocationID.xyz)));
        //if(i_Index == -1) return;
        uint Index = uint(i_Index);

        uvec3 InvocationID = gl_GlobalInvocationID.xyz;

        Node_VertexIndex[Index] = -1;
        Node_EdgeMask[Index]    = 0;
        
        //if(Index > TotalNodes) return;

        vec3 NodeMin = vec3(InvocationID) * SizeOffset;
        vec3 NodeMax =  vec3(InvocationID + 1) * SizeOffset;

        ivec3 ivec3_NodeMin = ivec3(round(NodeMin));

        // edge configuration
        vec3 Corners[8];

        //if(PassNum == 2)
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
        //else
        {
        //Corners[0] = ivec3_NodeMin + vec3(-SizeOffset, -SizeOffset, -SizeOffset);
        //Corners[1] = ivec3_NodeMin + vec3(0,          -SizeOffset, -SizeOffset);
        //Corners[2] = ivec3_NodeMin + vec3(-SizeOffset, 0,           -SizeOffset);
       // Corners[3] = ivec3_NodeMin + vec3(0,          0,           -SizeOffset);
        //Corners[4] = ivec3_NodeMin + vec3(-SizeOffset, -SizeOffset, 0         );
        //Corners[5] = ivec3_NodeMin + vec3(0,          -SizeOffset, 0         );
       //Corners[6] = ivec3_NodeMin + vec3(-SizeOffset, 0,           0         );
       // Corners[7] = ivec3_NodeMin;
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
        vec3 IntersectionSum = vec3(0.0);
        vec3 IntersectionNormals[12];
        vec3 IntersectionPoints[12];

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
                //if (abs(denom) < 1e-6) continue;
                float t = -d0 / denom;
                //if(abs(t) < 0.00001) continue;
                clamp(t, 0.00001, 1 - 0.00001);

                vec3 n;
                vec3 p;

                vec3 CornerA = Corners[i0];
                vec3 CornerB = Corners[i1];

                //bool CornerA_OutOfBounds = OutOfBoundsCheck(CornerA);
                //bool CornerB_OutOfBounds = OutOfBoundsCheck(CornerB);
                
                // normals
                /*
                if(!CornerA_OutOfBounds && !CornerB_OutOfBounds)
                {
                    n = mix(CalculateNormals(CornerA), CalculateNormals(CornerB), t);
                }
                else if(!CornerA_OutOfBounds && CornerB_OutOfBounds)
                {
                    n = CalculateNormals(CornerA);
                }
                else if(CornerA_OutOfBounds && !CornerB_OutOfBounds)
                    n = CalculateNormals(CornerB);
                else continue;*/


                p = mix(CornerA, CornerB, t);


                //vec3 p = mix(Corners[i0], Corners[i1], t);
                n = mix(CalculateNormals(Corners[i0]), CalculateNormals(Corners[i1]), t);
                //vec3 n = CalculateNormalGradient(p);
                
                Normals += n; 

                IntersectionSum += p;
                IntersectionCount++;
                IntersectionNormals[k] = n;
                IntersectionPoints[k] = p;

                EdgeMask |= (1u << k);
                if(d0 > d1)
                    EdgeMask |= (1u << (k + 12));

                DEBUG_intersect_centroid = uvec3(t * 255.0, 0, 0);

                //if(t < 0.01 || t > 0.99) {
                //    DEBUG_intersect_centroid = uvec3(1, 0, 0);
                //}
            }
        }
        
        vec3 Centroid = vec3(0.0, 0.0, 0.0); // or as an alias, position

        // reminder: NodeMin = vec3(di_x, di_y, di_z) * SizeOffset;
        if(IntersectionCount > 0)
        {
            Centroid = IntersectionSum / float(IntersectionCount);
            Normals /= IntersectionCount;
            
            if(PassOffset != BOUNDARY_GENERATION_PASS){
            const uint MAX_ITERATIONS = PassNum;

            mat3 ATA = mat3(0.0);
            vec3 ATb = vec3(0.0);

            for(int e = 0; e < 12; e++) {
                if((EdgeMask & (1u << e)) == 0u) continue;
                vec3 n = IntersectionNormals[e];
                vec3 p = IntersectionPoints[e];
                
                ATA[0][0] += n.x * n.x;
                ATA[0][1] += n.x * n.y;
                ATA[0][2] += n.x * n.z;

                ATA[1][0] += n.y * n.x;
                ATA[1][1] += n.y * n.y;
                ATA[1][2] += n.y * n.z;

                ATA[2][0] += n.z * n.x;
                ATA[2][1] += n.z * n.y;
                ATA[2][2] += n.z * n.z;
                
                ATb += n * dot(n, p);
            }

            vec3 Bias = Centroid;//(NodeMin + NodeMax) * 0.5;
            float Lambda = 0.06;
            ATA[0][0] += Lambda;
            ATA[1][1] += Lambda;
            ATA[2][2] += Lambda;
            ATb += Bias * Lambda;  // bias toward centroid

            float det = determinant(ATA);
            if(abs(det) > 1e-6) {
                vec3 result;
                result.x = determinant(mat3(ATb, ATA[1], ATA[2])) / det;
                result.y = determinant(mat3(ATA[0], ATb, ATA[2])) / det;
                result.z = determinant(mat3(ATA[0], ATA[1], ATb)) / det;
                Centroid = result;
            }}

            //Centroid = clamp(Centroid, NodeMin, NodeMax);

            //uint DEBUG_color_packed = 0; DEBUG_color_packed |= ((DEBUG_intersect_centroid.x << 16u) | (DEBUG_intersect_centroid.y << 8u) | DEBUG_intersect_centroid.z);

            vec4 Normal = vec4(Normals, 1.0);
            MAT_ID = VoxelData[Index].matID;
            vec4 Vertex = vec4(Centroid, MAT_ID);
            uint VertexIndex = store_vertices_and_normals(Vertex, Normal);

            Node_VertexIndex[Index] = int(VertexIndex);
            Node_EdgeMask[Index]    = EdgeMask;
        }

        return;
    }
    int i_Index = int(FlattenCoordinates(ivec3(gl_GlobalInvocationID.xyz)));
    if(i_Index == -1) return;
    uint Index = FlattenCoordinates(ivec3(gl_GlobalInvocationID.xyz));
    /*
    if(PassOffset < 2147483647)
    {
        if(PassOffset == 3)
        {
            vec4 Vertex = GetCellVertex(ivec3(gl_GlobalInvocationID.xyz), true);
            if(Vertex.w == -1) return;
            SetCellVertex(ivec3(gl_GlobalInvocationID.xyz), Vertex, true);
            return;
        }
        
        float SmoothFactor = 0;

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
        return;
    }
    */
    //if(Index > TotalNodes) return;

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

        if((EdgeMask & (1u << (0 + 12))) != 0)
            StoreIndices(V0, V1, V2, V3);
        else
            StoreIndices(V0, V3, V2, V1);
    }
    V1 = -1, V2 = -1, V3 = -1;
    
    if ((EdgeMask & (1u << 3)) != 0) // Y
    {
        V1 = GetCellIndex(MC_x - 0, MC_y - 0, MC_z - 1);

        if(V1 > -1)
            V2 = GetCellIndex(MC_x - 1, MC_y + 0, MC_z - 1);

        if(V2 > -1)
            V3 = GetCellIndex(MC_x - 1, MC_y + 0, MC_z + 0);

        if((EdgeMask & (1u << (3 + 12))) != 0)
            StoreIndices(V0, V3, V2, V1);
        else
            StoreIndices(V0, V1, V2, V3);
    }
    V1 = -1, V2 = -1, V3 = -1;
    
    if((EdgeMask & (1u << 8)) != 0) // Z
    {
        V1 = GetCellIndex(MC_x - 1, MC_y - 0, MC_z - 0);

        if(V1 > -1)
            V2 = GetCellIndex(MC_x - 1, MC_y - 1, MC_z - 0);

        if(V2 > -1)
            V3 = GetCellIndex(MC_x - 0, MC_y - 1, MC_z - 0);
            
        if((EdgeMask & (1u << (8 + 12))) != 0)
            StoreIndices(V0, V1, V2, V3);
        else
            StoreIndices(V0, V3, V2, V1);
    }
}