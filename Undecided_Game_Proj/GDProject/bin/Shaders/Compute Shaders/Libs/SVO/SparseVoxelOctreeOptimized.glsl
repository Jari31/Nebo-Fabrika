#[compute]
#version 450

layout(local_size_x = WORKGROUP_SIZE, local_size_y = WORKGROUP_SIZE, local_size_z = WORKGROUP_SIZE) in;

struct VoxelDataArray 
{
    float matID;
    float density;
};

layout(std430, set = 1, binding = 0) buffer voxelData {
    VoxelDataArray VoxelData[];
};

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

void Stage_BuildSVOFromLeaves(){
    float SumDensity = 0;
    uint SumMatID = 0;

    uint Child_Mask = 0;

    uint CurrentIndex = gl_GlobalInvocationID.x * 8;

    uint HigherDensities = 0;
    uint LowerDensities = 0;

    for(uint i = 0; i <= 7;)
    {
        float Density = VoxelData[CurrentIndex + i].density;

        if(Density != 0.0)
        {
            Child_Mask |= (1u << i);
        }

        SumDensity += Density;
        SumMatID += uint(VoxelData[CurrentIndex + i].matID);
        i++;
    }

    for(uint i = 0; i < 4; i++) // not sparse due to it being the first pass
    {
        LowerDensities |= uint((VoxelData[CurrentIndex + i].density + 1) * 127.5) << 8 * i;
    }
    
    for(uint i = 4; i <= 7; i++)
    {
        HigherDensities |= uint((VoxelData[CurrentIndex + i].density + 1) * 127.5) << 8 * (i - 4);
    }

    uint AverageDensity = uint(((SumDensity / 8) + 1) * 127.5); // 255/2 max unsigned int8
    
    if(AverageDensity > 0.0f)
    {
        uint CurrentThread_NodeIndex = atomicAdd(AtomicCounter, 1);

        uint AverageMatID = (SumMatID / 8);

        uint PackedDensity_MatID = (AverageMatID << 8) | AverageDensity;
        SVO_Node[CurrentThread_NodeIndex].ChildMask = Child_Mask;
        SVO_Node[CurrentThread_NodeIndex].Data = PackedDensity_MatID;
        SVO_Node[CurrentThread_NodeIndex].ChildPointer = CurrentIndex;
        SVO_Node[CurrentThread_NodeIndex].MortonAddress = gl_GlobalInvocationID.x;

        SVO_Node[CurrentThread_NodeIndex].densities_lower4 = LowerDensities;
        SVO_Node[CurrentThread_NodeIndex].densities_higher4 = HigherDensities;
    }
}

void BuildHigherLayer_AUX_NODE_AtomicCounter()
{
    /*
        buffer SVO_NodeArray InputNodeBuffer = SVO_AuxNode
        buffer SVO_NodeArray OutputNodeBuffer = SVO_Node
        buffer uint atomicCounter_READ = AtomicCounter
        buffer uint atomicCounter_WRITE = AtomicCounter2
    */
    uint NodeIndex = gl_GlobalInvocationID.x;

    if(NodeIndex >= AtomicCounter)
        return;

    SVO_NodeArray CurrentNode = SVO_AuxNode[NodeIndex];

    uint ParentMortonAddress = CurrentNode.MortonAddress >> 3;

    bool isFirstChild = (NodeIndex == 0);
    if(NodeIndex > 0)
    {
        isFirstChild = (ParentMortonAddress != (SVO_AuxNode[NodeIndex - 1].MortonAddress >> 3));
    }

    if(isFirstChild)
    {
        uint ParentNodeIndex = atomicAdd(AtomicCounter2, 1);

        uint ChildMask = 0;
        float SumDensity = 0.0;
        uint SumMatID = 0;
        uint ChildCount = 0;

        for(uint Offset = 0; Offset < 8; Offset++)
        {
            uint SiblingIndex = NodeIndex + Offset;
            if(SiblingIndex >= AtomicCounter)
                break;

            SVO_NodeArray SiblingNode = SVO_AuxNode[SiblingIndex];
            uint SiblingParentMortonAddr = SiblingNode.MortonAddress >> 3;
            if(SiblingParentMortonAddr != ParentMortonAddress)
                break;

            uint Octant = SiblingNode.MortonAddress & 7u;
            ChildMask |= (1u << Octant);

            float Density = (float(SiblingNode.Data & 0xFFu) * 0.007843137254902) - 1.0f; // 0.007843137254902 = 1/127.5
            uint MatID = (SiblingNode.Data >> 8) & 0xFFu;

            SumDensity += Density;
            SumMatID += MatID;
            ChildCount++;
        }

        uint AverageDensity = uint(((SumDensity / float(ChildCount)) + 1) * 127.5);

        if(AverageDensity == 0.0f)
            return;

        if(!ChildCount)
            return;

        uint HigherDensities = 0;
        uint LowerDensities = 0;

        vec3 deInterleavedCoordinates;

        float SampledDensity = 0.0f;

        for(uint i = 0; i < 4; i++)
        {   
            if(ChildMask & (1u << i))
                LowerDensities |= uint(((SVO_AuxNode[NodeIndex + i].Data & 0xFF) + 1) * 127.5) << 8 * i;
        }
        for(uint i = 4; i < 8; i++)
        {
            if(ChildMask & (1u << i))
                HigherDensities |= uint(((SVO_AuxNode[NodeIndex + i].Data & 0xFF) + 1) * 127.5) << 8 * (i - 4);
        }

        
        uint AverageMatID = uint(SumMatID / ChildCount);
        uint PackedData = (AverageMatID << 8) | AverageDensity;
        
        SVO_Node[ParentNodeIndex].MortonAddress = ParentMortonAddress;
        SVO_Node[ParentNodeIndex].Data = PackedData;
        SVO_Node[ParentNodeIndex].ChildPointer = NodeIndex;
        SVO_Node[ParentNodeIndex].ChildMask = ChildMask;
        
        SVO_Node[ParentNodeIndex].densities_lower4 = LowerDensities;
        SVO_Node[ParentNodeIndex].densities_higher4 = HigherDensities; 
    }
}

void BuildHigherLayer_NODE_AUX_AtomicCounter2()
{
    /*
        buffer SVO_NodeArray InputNodeBuffer = SVO_Node
        buffer SVO_NodeArray OutputNodeBuffer = SVO_AuxNode
        buffer uint atomicCounter_READ = AtomicCounter2
        buffer uint atomicCounter_WRITE = AtomicCounter
    */
    uint NodeIndex = gl_GlobalInvocationID.x;

    if(NodeIndex >= AtomicCounter2)
        return;

    SVO_NodeArray CurrentNode = SVO_Node[NodeIndex];

    uint ParentMortonAddress = CurrentNode.MortonAddress >> 3;

    bool isFirstChild = (NodeIndex == 0);
    if(NodeIndex > 0)
    {
        isFirstChild = (ParentMortonAddress != (SVO_Node[NodeIndex - 1].MortonAddress >> 3));
    }

    if(isFirstChild)
    {
        uint ParentNodeIndex = atomicAdd(AtomicCounter, 1);

        uint ChildMask = 0;
        float SumDensity = 0.0;
        uint SumMatID = 0;
        uint ChildCount = 0;

        for(uint Offset = 0; Offset < 8; Offset++)
        {
            uint SiblingIndex = NodeIndex + Offset;
            if(SiblingIndex >= AtomicCounter2)
                break;

            SVO_NodeArray SiblingNode = SVO_Node[SiblingIndex];
            uint SiblingParentMortonAddr = SiblingNode.MortonAddress >> 3;
            if(SiblingParentMortonAddr != ParentMortonAddress)
                break;

            uint Octant = SiblingNode.MortonAddress & 7u;
            ChildMask |= (1u << Octant);

            float Density = (float(SiblingNode.Data & 0xFFu) * 0.007843137254902) - 1.0f;
            uint MatID = (SiblingNode.Data >> 8) & 0xFFu;

            SumDensity += Density;
            SumMatID += MatID;
            ChildCount++;
        }

        uint AverageDensity = uint(((SumDensity / float(ChildCount)) + 1) * 127.5);

        if(AverageDensity == 0.0f)
            return;

        if(!ChildCount)
            return;

        uint HigherDensities = 0;
        uint LowerDensities = 0;

        vec3 deInterleavedCoordinates;

        float SampledDensity = 0.0f;

        for(uint i = 0; i < 4; i++)
        {   
            if(ChildMask & (1u << i))
                LowerDensities |= uint(((SVO_Node[NodeIndex + i].Data & 0xFF) + 1) * 127.5) << 8 * i;
        }
        for(uint i = 4; i < 8; i++)
        {
            if(ChildMask & (1u << i))
                HigherDensities |= uint(((SVO_Node[NodeIndex + i].Data & 0xFF) + 1) * 127.5) << 8 * (i - 4);
        }

        
        uint AverageMatID = uint(SumMatID / ChildCount);
        uint PackedData = (AverageMatID << 8) | AverageDensity;
        
        SVO_AuxNode[ParentNodeIndex].MortonAddress = ParentMortonAddress;
        SVO_AuxNode[ParentNodeIndex].Data = PackedData;
        SVO_AuxNode[ParentNodeIndex].ChildPointer = NodeIndex;
        SVO_AuxNode[ParentNodeIndex].ChildMask = ChildMask;
        
        SVO_AuxNode[ParentNodeIndex].densities_lower4 = LowerDensities;
        SVO_AuxNode[ParentNodeIndex].densities_higher4 = HigherDensities; 
    }
}

void Stage_BuildSVOHigherLayers(uint passNum)
{   
    switch(passNum)
    {
        case 1:
        {
            BuildHigherLayer_AUX_NODE_AtomicCounter();
            break;
        }
        
        case 2:
        {
            BuildHigherLayer_NODE_AUX_AtomicCounter2();
            break;
        }
        
        default:
            return;
    }
}
void main(){
    uint passNum = PushConstants.PassNum;

    if(!passNum)
    {
        Stage_BuildSVOFromLeaves();
    };

    Stage_BuildSVOHigherLayers(passNum);

}