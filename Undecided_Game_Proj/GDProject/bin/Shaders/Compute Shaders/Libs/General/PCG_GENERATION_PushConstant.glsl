//-------------------------------------------------------- push const
layout(push_constant) uniform PushConstants 
{
    uint  PassNum;
    uint  PassOffset;
    uint  PassStage;
    uint  WriteToTexturesInFirstPass;

    uint  Dense_TotalNodes;
    uint  SEED;
    float VoxelSize;
    uint LOD_Index;

    uint VertexIntervalOnEdge;
    uint GridSize;
    float IndexCoefficient;
    uint ThreadAllocationPerTriangle;
    
    ivec4 dCHUNK_SIZE;
    ivec4 dVOXELS_PER_CHUNK;

    ivec4 VertexOffsetLoD;
};