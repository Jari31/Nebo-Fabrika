#[compute]
#version 450

#ifndef WORKGROUP_SIZE_X
#define WORKGROUP_SIZE_X 16
#endif
#ifndef WORKGROUP_SIZE_Y
#define WORKGROUP_SIZE_Y 6
#endif
#ifndef WORKGROUP_SIZE_Z
#define WORKGROUP_SIZE_Z 1
#endif

layout(local_size_x = WORKGROUP_SIZE_X, local_size_y = WORKGROUP_SIZE_Y) in;

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

shared uint localOffsets[6][16]; // y | x because why not

void main()
{
    uint Row = gl_LocalInvocationID.y;
    uint Column = gl_LocalInvocationID.x;

    localOffsets[Row][Column] = Histogram[Row][Column];

    barrier();

    for(uint stride = 1; stride < 16; stride <<= 1)
    {
        uint Value = 0;
        if(Column >= stride)
        {
            Value = localOffsets[Row][Column - stride]; 
            /* i.e., 
            [3, 2, 4, 5]; 
            if Column = 1, stride = 1; 
            Column - stride = 1 - 1 = 0; 
            Value = [Row][0] = 3 
            */
        }

        barrier();

        if(Column >= stride)
        {
            localOffsets[Row][Column] += Value;
            /* i.e.,
            [3, 2, 4, 5];
            Column = 1, Stride = 1;
            localOffsets[Row][Column] = [3, 2, 4];
                                   index[0, 1, 2];
            localOffsets[Row][1] += Value = [3, 2 + 3, 4];
                                          = [3, 5, 4]
            */
        }
        barrier();
    }

    if(Column == 0)
    {
        Offsets[Row][Column] = 0;
    }
    else
    {
        Offsets[Row][Column] = localOffsets[Row][Column - 1];
    }
}

/* Naive approach
void prefixSum()
{
    uint passNum = PassNum - 6; // local
    if(gl_GlobalInvocationID.x == 0)
    {
        uint Sum = 0;
        for(uint i = 0; i < 16; i++)
        {
            uint Count = Histogram[passNum][i];
            Offsets[passNum][i] = Sum;

            Sum += Count;
        }
    }
}
*/