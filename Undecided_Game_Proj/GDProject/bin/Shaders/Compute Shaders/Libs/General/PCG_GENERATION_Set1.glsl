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