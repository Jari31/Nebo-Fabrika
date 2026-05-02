//-------------------------------------------------------- push const
layout(push_constant) uniform PushConstants 
{
    uint  PassNum;
    uint  PassOffset;
    uint  PassStage;
    uint  FLAG;

    uint  Dense_TotalNodes;
    uint  SEED;
    float SVO_VoxelSize;
    uint SVO_BufferSize;

    uint HASH_SIZE;
    uint GridSize;
    uint pad2;
    uint pad3;
    
    ivec4 dCHUNK_SIZE;
    ivec4 dVOXELS_PER_CHUNK;
};