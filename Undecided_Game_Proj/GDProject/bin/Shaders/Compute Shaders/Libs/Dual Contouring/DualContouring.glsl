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

// --------------------------------------------------- set 1

// --------------------------------------------------- struct
struct SVO_NodeArray
{
    uint ChildPointer;
    uint ChildMask;

    uint Data;
    uint MortonAddress;

    uint densities_lower4;
    uint densities_higher4;
};

struct VoxelDataArray 
{
    float matID;
    float density;
};

// --------------------------------------------------- buff

layout(std430, set = 0, binding = 0) buffer voxelData {
    VoxelDataArray VoxelData[];
};

layout(std430, set = 0, binding = 1) buffer SVONodeBuffer
{
    SVO_NodeArray SVO_Node[]; //Buffer A
};

layout(std430, set = 0, binding = 2) buffer SVOAuxNodeBuffer
{
    SVO_NodeArray SVO_AuxNode[]; //Buffer B
};

layout(std430, set = 0, binding = 3) buffer atomicCounter
{
    uint AtomicCounter;
    uint AtomicCounter2;

    uint VertexCounter;
};

layout(std430, set = 0, binding = 4) buffer HistogramBuffer
{
    uint Histogram[6][16];
};

layout(std430, set = 0, binding = 5) buffer OffsetBuffer 
{
    uint Offsets[6][16];
};

//---------------------------------------------------------- set 2
layout(std430, set = 1, binding = 0) buffer vertexBuffer 
{
    vec3 Vertices[];    
};

layout(std430, set = 1, binding = 1) buffer DC_NormalBuffer
{
    vec3 Normals[];
};

layout(std430, set = 1, binding = 2) buffer DC_UVBuffer
{
    vec2 UV[];
};

layout(std430, set = 1, binding = 3) buffer indexBuffer
{
    uint Indices[];
};


layout(std430, set = 1, binding = 4) buffer nodeVertexBuffer
{
    int Node_VertexIndex[];
};


layout(std430, set = 1, binding = 5) buffer nodeEdgeMaskBuffer
{
    uint Node_EdgeMask[];
};

//-------------------------------------------------------- push const // i hate bit alignment
layout(push_constant) uniform PushConstants 
{
    uint  PassNum;
    uint  PassOffset;
    uint  PassStage;
    uint  Dense_SaveAsMortonCode;

    uint  Dense_TotalNodes;
    uint  SEED;
    float SVO_VoxelSize;
    uint SVO_BufferSize;

    uint HASH_SIZE;
    uint pad1;
    uint pad2;
    uint pad3;
    
    ivec4 dCHUNK_SIZE;
    ivec4 dVOXELS_PER_CHUNK;
};

ivec3 CHUNK_SIZE = ivec3(dCHUNK_SIZE);
ivec3 VOXELS_PER_CHUNK = ivec3(dVOXELS_PER_CHUNK);

#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/DualContouring_Math.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/MortonCurve.glsl"

//const uint TOTAL_SIZE = CHUNK_SIZE.x * VOXELS_PER_CHUNK; total nodes. remember to - 1 on the CPU 

uint FlattenCoordinates(ivec3 Coordinates)
{
    return uint(Coordinates.x + (Coordinates.y * Dense_TotalNodes) + (Coordinates.z * Dense_TotalNodes * Dense_TotalNodes));
}

/*
carbon-copy of the sparse solver in some parts, but it's like different in such tiny ways, 
that separating parts of it into its own compute shader will take more work than just copy pasting parts after modifying it
*/

void main() 
{
    uint TotalNodes = Dense_TotalNodes - 1; // need to add a padding ghost layer so I don't want to kill myself by writing complex logic

    { uvec3 Index = gl_GlobalInvocationID.xyz; if(Index.x > TotalNodes || Index.y > TotalNodes || Index.z > TotalNodes) return; }

    uint Index = FlattenCoordinates(ivec3(gl_GlobalInvocationID.xyz)); 
    
    float SizeOffset = 1;
    float CellSize   = 1;

    /*
    uint MortonAddress = Node.MortonAddress;
    float di_x = float(DeInterleave(MortonAddress)); // di = de-interleaved
    float di_y = float(DeInterleave(MortonAddress >> 1));
    float di_z = float(DeInterleave(MortonAddress >> 2));
    vec3 NodeMin = vec3(Index, Index, Index) * SizeOffset;
    */

    if(PassOffset > 0)
    {
        vec3 NodeMin = vec3(gl_GlobalInvocationID.xyz) * SizeOffset;


        // edge configuration
        vec3 Corners[8];
        Corners[0] = NodeMin;                                                     
        Corners[1] = NodeMin + vec3(SizeOffset, 0,          0         ); 
        Corners[2] = NodeMin + vec3(0,          SizeOffset, 0         ); 
        Corners[3] = NodeMin + vec3(SizeOffset, SizeOffset, 0         ); 
        Corners[4] = NodeMin + vec3(0,          0,          SizeOffset);
        Corners[5] = NodeMin + vec3(SizeOffset, 0,          SizeOffset);
        Corners[6] = NodeMin + vec3(0,          SizeOffset, SizeOffset);
        Corners[7] = NodeMin + vec3(SizeOffset, SizeOffset, SizeOffset);

        float CornerDensities[8];
        CornerDensities[0] = VoxelData[FlattenCoordinates(ivec3(NodeMin + ivec3(0, 0, 0)))].density;
        CornerDensities[1] = VoxelData[FlattenCoordinates(ivec3(NodeMin + ivec3(1, 0, 0)))].density;
        CornerDensities[2] = VoxelData[FlattenCoordinates(ivec3(NodeMin + ivec3(0, 1, 0)))].density;
        CornerDensities[3] = VoxelData[FlattenCoordinates(ivec3(NodeMin + ivec3(1, 1, 0)))].density;
        CornerDensities[4] = VoxelData[FlattenCoordinates(ivec3(NodeMin + ivec3(0, 0, 1)))].density;
        CornerDensities[5] = VoxelData[FlattenCoordinates(ivec3(NodeMin + ivec3(1, 0, 1)))].density;
        CornerDensities[6] = VoxelData[FlattenCoordinates(ivec3(NodeMin + ivec3(0, 1, 1)))].density;
        CornerDensities[7] = VoxelData[FlattenCoordinates(ivec3(NodeMin + ivec3(1, 1, 1)))].density;

        uint Empty_Full = 0;
        for(int i = 0; i < 8; i++) 
        {
            if(CornerDensities[i] >= 0)
                Empty_Full |= 1u << i;
            else if(CornerDensities[i] <= 0)
                Empty_Full |= 1u << (i + 8);
        }

        if((Empty_Full & 0xFF) == 255u || (Empty_Full & 0xFF00) == 255u) return;

        const int EdgeCorners[12][2] = {
            {0,1}, {1,3}, {3,2}, {2,0}, // bottom face (z=0) edges
            {4,5}, {5,7}, {7,6}, {6,4}, // top face (z=1) edges
            {0,4}, {1,5}, {3,7}, {2,6}  // vertical edges
        };

        vec3 IntersectionSum = vec3(0.0);
        int IntersectionCount = 0;

        uint EdgeMask = 0;
        for(int k = 0; k < 12; k++)
        {
            int i0   = EdgeCorners[k][0];
            int i1   = EdgeCorners[k][1];
            float d0 = CornerDensities[i0];
            float d1 = CornerDensities[i1];
        
            if(d0 * d1 < 0.0 || abs(d0) < 1e-6 || abs(d1) < 1e-6)
            {
                float t = -d0 / (d1 - d0);
                t = clamp(t, 0.0, 1.0);

                vec3 p = mix(Corners[i0], Corners[i1], t);
                IntersectionSum += p;
                IntersectionCount++;

                EdgeMask |= (1u << k);
            }
        }

        vec3 Centroid = vec3(0.0, 0.0, 0.0); // or as an alias, position

        // reminder: NodeMin = vec3(di_x, di_y, di_z) * SizeOffset;
        if(IntersectionCount > 0)
        {
            Centroid = IntersectionSum / float(IntersectionCount);
            const uint MAX_ITERATIONS = PassNum;
            const float STEP_SCALE = 0.5; 
            const float CONVERGE_EPSILON = 1e-4;

            for(int Iteration = 0; Iteration < MAX_ITERATIONS; Iteration++)
            {
                float fDensity = TrilinearSample(Centroid, NodeMin, CellSize, CornerDensities);
                if(abs(fDensity) < CONVERGE_EPSILON) 
                    break;
                
                vec3 Gradient = TrilinearGradient(Centroid, NodeMin, CellSize, CornerDensities);

                float GradientLen2 = dot(Gradient, Gradient); // faster than doing length calcs
                if(GradientLen2 < 1e-8) 
                    break;

                float Step = STEP_SCALE * fDensity / GradientLen2; // vec/magnitude normalizes it
                Centroid -= Step * Gradient;

                Centroid = clamp(Centroid, NodeMin, NodeMin + CellSize);
            }

            uint VertexIndex = atomicAdd(VertexCounter, 1);
            Vertices[VertexIndex] = vec3(Centroid) + vec3(gl_GlobalInvocationID.xyz);

            Node_VertexIndex[Index] = int(VertexIndex);
            Node_EdgeMask[Index]    = EdgeMask;
        }
        else
        {
            Node_VertexIndex[Index] = -1;
            Node_EdgeMask[Index]    = 0;
        }

        return;
    }

    if(Node_VertexIndex[Index] < 0)
        return;

    
    // boilerplate edge offsets generated by AI; I ain't writing allat
    const ivec3 EdgeNeighbors[12][4] = {
        // Edge 0: (0,0,0)-(1,0,0) bottom front X
        { ivec3(0, 0, 0), ivec3(0,-1, 0), ivec3(0,-1,-1), ivec3(0, 0,-1) },
        // Edge 1: (1,0,0)-(1,1,0) bottom right Y
        { ivec3(0, 0, 0), ivec3(1, 0, 0), ivec3(1, 0,-1), ivec3(0, 0,-1) },
        // Edge 2: (1,1,0)-(0,1,0) bottom back X (reverse direction)
        { ivec3(0, 0, 0), ivec3(0, 1, 0), ivec3(0, 1,-1), ivec3(0, 0,-1) },
        // Edge 3: (0,1,0)-(0,0,0) bottom left Y
        { ivec3(0, 0, 0), ivec3(0, 0,-1), ivec3(-1, 0,-1), ivec3(-1, 0, 0) },

        // Edge 4: (0,0,1)-(1,0,1) top front X
        { ivec3(0, 0, 0), ivec3(0,-1, 0), ivec3(0,-1, 1), ivec3(0, 0, 1) },
        // Edge 5: (1,0,1)-(1,1,1) top right Y
        { ivec3(0, 0, 0), ivec3(1, 0, 0), ivec3(1, 0, 1), ivec3(0, 0, 1) },
        // Edge 6: (1,1,1)-(0,1,1) top back X
        { ivec3(0, 0, 0), ivec3(0, 1, 0), ivec3(0, 1, 1), ivec3(0, 0, 1) },
        // Edge 7: (0,1,1)-(0,0,1) top left Y
        { ivec3(0, 0, 0), ivec3(0, 0, 1), ivec3(-1, 0, 1), ivec3(-1, 0, 0) },

        // Edge 8: (0,0,0)-(0,0,1) front left Z
        { ivec3(0, 0, 0), ivec3(-1, 0, 0), ivec3(-1,-1, 0), ivec3(0,-1, 0) },
        // Edge 9: (1,0,0)-(1,0,1) front right Z
        { ivec3(0, 0, 0), ivec3(1, 0, 0), ivec3(1,-1, 0), ivec3(0,-1, 0) },
        // Edge 10: (1,1,0)-(1,1,1) back right Z
        { ivec3(0, 0, 0), ivec3(1, 0, 0), ivec3(1, 1, 0), ivec3(0, 1, 0) },
        // Edge 11: (0,1,0)-(0,1,1) back left Z
        { ivec3(0, 0, 0), ivec3(-1, 0, 0), ivec3(-1, 1, 0), ivec3(0, 1, 0) }
    };

    int MC_x = int(gl_GlobalInvocationID.x); // mc = minimum corner
    int MC_y = int(gl_GlobalInvocationID.y);
    int MC_z = int(gl_GlobalInvocationID.z);
    ivec3 MC_xyz = ivec3(MC_x, MC_y, MC_z);

    uint EdgeMask = uint(Node_EdgeMask[Index]);
    if(EdgeMask == 0) return;
    
    {
    ivec3 NeighborPosition[4];
    uint NeighborNode_FlattenedIndex[4];

    
    for(int i = 0; i < 12; i++)
    {
        uint Node_Cell = Index;
        uint Node_Owner_Cell = Node_Cell;
        if((EdgeMask & (1u << i)) == 0)
            continue;
        bool Exists = true;
        
        for(int j = 0; j < 4; j++)
        {
            NeighborPosition[j] = MC_xyz + EdgeNeighbors[i][j];

            if(NeighborPosition[j].x < 0 || NeighborPosition[j].y < 0 || NeighborPosition[j].z < 0)
            {
                Node_Cell = 0;
                break;
            }

            NeighborNode_FlattenedIndex[j] = FlattenCoordinates(NeighborPosition[j]);
            if(NeighborNode_FlattenedIndex[j] < Node_Cell) Node_Cell = NeighborNode_FlattenedIndex[j];
        }

        if(Node_Cell != Node_Owner_Cell) continue;

        int NodeIndices[4];
        for(int j = 0; j < 4; j++)
        {
            uint Node_Index = NeighborNode_FlattenedIndex[j];
            if(Node_Index < 0 || Node_VertexIndex[Node_Index] < 0)
            {
                Exists = false;
                break;
            }
            
            NodeIndices[j] = int(Node_Index);
        }

        if(!Exists) continue;

        int Vertex_0 = Node_VertexIndex[NodeIndices[0]];
        int Vertex_1 = Node_VertexIndex[NodeIndices[1]];
        int Vertex_2 = Node_VertexIndex[NodeIndices[2]];
        int Vertex_3 = Node_VertexIndex[NodeIndices[3]];

        uint AtomicIndex   = atomicAdd(AtomicCounter2, 6);
        Indices[AtomicIndex]     = uint(Vertex_0);
        Indices[AtomicIndex + 1] = uint(Vertex_1);
        Indices[AtomicIndex + 2] = uint(Vertex_2);
        
        Indices[AtomicIndex + 3] = uint(Vertex_0);
        Indices[AtomicIndex + 4] = uint(Vertex_2);
        Indices[AtomicIndex + 5] = uint(Vertex_3);
    }
    }
}