// --------------------------------------------------- set 1

// --------------------------------------------------- struct
struct VoxelDataArray 
{
    float matID;
    float density;
    vec2 normals_packed_oct;
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
