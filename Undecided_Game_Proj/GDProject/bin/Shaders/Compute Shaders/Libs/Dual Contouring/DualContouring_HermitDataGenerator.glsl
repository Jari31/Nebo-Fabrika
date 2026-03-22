#[compute]

#version 450

layout(local_size_x = WORKGROUP_SIZE, local_size_y = WORKGROUP_SIZE, layout_size_z = WORKGROUP_SIZE) in;
#extension GL_GOOGLE_include_directive : require

struct SVO_NodeArray
{
    uint ChildPointer;
    uint ChildMask;

    uint Data;
    uint MortonAddress;

    uint densities_lower4;
    uint densities_higher4;
};

layout(std430, set = 2, binding = 1) buffer atomicCounter
{
    uint AtomicCounter;
};

layout(std430, set = 2, binding = 3) buffer atomicCounter2 
{
    uint AtomicCounter2;
};

layout(push_constant) uniform PushConstants 
{
    uint PassNum;
    uint PassOffset;
    uint PassStage;

    uint SEED;
    
    ivec3 ENTITY_LOCATION;
    ivec3 ENTITY_LOCATION_P2;

    ivec3 CHUNK_SIZE;
    ivec3 VOXELS_PER_CHUNK;
};

layout(std430, set = 2, binding = 0) buffer SVONodeBuffer
{
    SVO_NodeArray SVO_Node[]; //Buffer A
};

layout(std430, set = 2, binding = 2) buffer SVOAuxNodeBuffer
{
    SVO_NodeArray SVO_AuxNode[]; //Buffer B
};

//what buffer has the actual data depends on what pass the SVO algorithm finished at

float UnpackDensity(uint PackedData, uint Index)
{
    uint byte = (PackedData >> (i * 8)) & 0xFF;
    return ((float(byte) * 0.007843137254902) - 1.0);
}

void main()
{
    SVO_NodeArray Node = SVO_Node[gl_GlobalInvocationID.x];
    // corner X
    d0 = UnpackDensity(Node.densities_lower4, 0);
}