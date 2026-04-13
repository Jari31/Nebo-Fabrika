#[compute]
#version 450

/*
    COPYRIGHT (c) 2026 Jari
    Licensed under the MIT license. Refer to the license file provided within the README for details.
*/

layout(local_size_x = 256) in;

// technically RadixSort. it's more dedicated for an SVO. though I guess you could refactor it to serve a general purpose

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


//-------------------------------------------------------- push const
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

shared uint LocalHistogram[16];

void Scatter()
{
    uint passNum = PassNum;
    uint GlobalIndex = gl_GlobalInvocationID.x;  

    if(GlobalIndex >= SVO_Node.length())
        return;
    
    SVO_NodeArray Node = SVO_Node[GlobalIndex];

    uint Bucket = (Node.MortonAddress >> PassOffset) & 15u; // 21 >> 4 & 1111
                                                                // 10101 >> 4 & 1111; 0001 & 1 = 1; same logic from the histogram logic in main()

    uint OutputIndex = atomicAdd(Offsets[PassNum][Bucket], 1u);

    SVO_AuxNode[OutputIndex] = Node;
}


void main()
{
    if(PassStage == 1)
    {
        // histogram
        if(gl_GlobalInvocationID.x < 16)
            LocalHistogram[gl_GlobalInvocationID.x] = 0;
        barrier();

        uint GlobalIndex = gl_GlobalInvocationID.x;

        if(GlobalIndex < SVO_Node.length())
        {
            uint MortonCode = SVO_Node[GlobalIndex].MortonAddress;
            uint Bucket = (MortonCode >> PassOffset) & 15u;

            atomicAdd(LocalHistogram[Bucket], 1u);
        }
        barrier();

        if(gl_LocalInvocationID.x < 16)
        {
            uint BucketIndex = gl_LocalInvocationID.x;
            atomicAdd(Histogram[PassNum][BucketIndex], LocalHistogram[BucketIndex]);
        }

        return;
    }

    Scatter();
}