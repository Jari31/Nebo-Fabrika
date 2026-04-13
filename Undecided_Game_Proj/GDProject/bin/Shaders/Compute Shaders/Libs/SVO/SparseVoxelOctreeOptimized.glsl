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

        if(ChildCount == 0)
            return;

        uint AverageDensity = uint(((SumDensity / float(ChildCount)) + 1) * 127.5);

        if(AverageDensity == 0.0f) // when avg density = 0, child count is also zero. since where did the density come from if there were no children?
            return;


        uint HigherDensities = 0;
        uint LowerDensities = 0;

        for(uint i = 0; i < 4; i++)
        {   
            if((ChildMask & (1u << i)) > 0)
                LowerDensities |= uint(((SVO_AuxNode[NodeIndex + i].Data & 0xFF) + 1) * 127.5) << 8 * i;
        }
        for(uint i = 4; i < 8; i++)
        {
            if((ChildMask & (1u << i)) > 0)
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

        if(ChildCount == 0)
            return;

        uint AverageDensity = uint(((SumDensity / float(ChildCount)) + 1) * 127.5);

        if(AverageDensity == 0.0f)
            return;

        uint HigherDensities = 0;
        uint LowerDensities = 0;

        for(uint i = 0; i < 4; i++)
        {   
            if((ChildMask & (1u << i)) > 0)
                LowerDensities |= uint(((SVO_Node[NodeIndex + i].Data & 0xFF) + 1) * 127.5) << 8 * i;
        }
        for(uint i = 4; i < 8; i++)
        {
            if((ChildMask & (1u << i)) > 0)
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
    uint passNum = PassNum;

    if(passNum <= 0)
    {
        Stage_BuildSVOFromLeaves();
    };

    Stage_BuildSVOHigherLayers(passNum);

}