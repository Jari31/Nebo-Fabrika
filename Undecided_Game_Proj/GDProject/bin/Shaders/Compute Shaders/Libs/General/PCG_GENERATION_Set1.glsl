// --------------------------------------------------- set 1

// --------------------------------------------------- struct
struct VoxelDataArray 
{
    float matID;
    float density;
};

// --------------------------------------------------- buff

layout(std430, set = 0, binding = 0) buffer voxelData {
    VoxelDataArray VoxelData[];
};

layout(std430, set = 0, binding = 1) buffer atomicCounter
{
    uint AtomicCounter;
    uint AtomicCounter2;

    uint VertexCounter;
};

layout(std140, set = 0, binding = 2) uniform uniformParameterBuffer
{
    uint VerticesPerThread;
    uint VertexAllocationForEdges;
    uint TrianglesProcessedPerThread;
    float TRUTH_GRID_SIZE;
};