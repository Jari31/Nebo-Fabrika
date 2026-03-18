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
};

layout(std430, set = 2, binding = 1) buffer atomicCounter
{
    uint AtomicCounter;
};

layout(std430, set = 2, binding = 3) buffer atomicCounter2 
{
    uint AtomicCounter2;
}

layout(push_constant) uniform PushConstants 
{
    uint PassNum;
    uint PassOffset;
    uint padding[2];
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

    for(uint i = 0; i <= 7;)
    {
        float Density = VoxelData[CurrentIndex + i].density;

        if(Density > 0.0)
        {
            Child_Mask |= (1u << i);
        }

        SumDensity += Density;
        SumMatID += uint(VoxelData[CurrentIndex + i].matID);
        i++;
    }

    uint AverageDensity = uint((SumDensity / 8) * 255); // 255 max unsigned int8
    
    if(AverageDensity > 0)
    {
        uint CurrentThread_NodeIndex = atomicAdd(AtomicCounter, 1);

        uint AverageMatID = (SumMatID / 8);

        uint PackedDensity_MatID = (AverageMatID << 8) | AverageDensity;

        SVO_Node[CurrentThread_NodeIndex].ChildMask = Child_Mask;
        SVO_Node[CurrentThread_NodeIndex].Data = PackedDensity_MatID;
        SVO_Node[CurrentThread_NodeIndex].ChildPointer = CurrentIndex;
        SVO_Node[CurrentThread_NodeIndex].MortonAddress = gl_GlobalInvocationID.x;
    }
}

void BuildHigherLayer(buffer SVO_NodeArray InputNodeBuffer,
                      buffer SVO_NodeArray OutputNodeBuffer,
                      buffer uint atomicCounter_READ,
                      buffer uint atomicCounter_WRITE)
{
    uint NodeIndex = gl_GlobalInvocationID.x;

    if(NodeIndex >= atomicCounter_READ)
        return;

    SVO_NodeArray CurrentNode = InputNodeBuffer[NodeIndex];

    uint ParentMortonAddress = CurrentNode.MortonAddress >> 3;

    bool isFirstChild = (NodeIndex == 0);
    if(NodeIndex > 0)
    {
        isFirstChild = (ParentMortonAddress != (InputNodeBuffer[NodeIndex - 1].MortonAddress >> 3));
    }

    if(isFirstChild)
    {
        uint ParentNodeIndex = atomicAdd(atomicCounter_WRITE, 1);

        uint ChildMask = 0;
        float SumDensity = 0.0;
        uint SumMatID = 0;
        uint ChildCount = 0;

        for(uint Offset = 0; Offset < 8; Offset++)
        {
            uint SiblingIndex = NodeIndex + Offset;
            if(SiblingIndex >= atomicCounter_READ)
                break;

            SVO_NodeArray SiblingNode = InputNodeBuffer[SiblingIndex];
            uint SiblingParentMortonAddr = SiblingNode.MortonAddress >> 3;
            if(SiblingParentMortonAddr != ParentMortonAddress)
                break;

            uint Octant = SiblingNode.MortonAddress & 7u;
            ChildMask |= (1u << Octant);

            float Density = float(SiblingNode.Data & 0xFFu) / 255.0;
            uint MatID = (SiblingNode.Data >> 8) & 0xFFu;

            SumDensity += Density;
            SumMatID += MatID;
            ChildCount++;
        }

        uint AverageDensity = uint((SumDensity / float(ChildCount)) * 255.0);
        uint AverageMatID = SumMatID / ChildCount;
        uint PackedData = (AverageMatID << 8) | AverageDensity;
        
        OutputNodeBuffer[ParentNodeIndex].MortonAddress = ParentMortonAddress;
        OutputNodeBuffer[ParentNodeIndex].Data = PackedData;
        OutputNodeBuffer[ParentNodeIndex].ChildPointer = NodeIndex;
        OutputNodeBuffer[ParentNodeIndex].ChildMask = ChildMask; 
    }
}

void Stage_BuildSVOHigherLayers(uint passNum)
{   
    switch(passNum)
    {
        case 1:
        {
            BuildHigherLayer(SVO_AuxNode,      
                             SVO_Node,
                             AtomicCounter,
                             AtomicCounter2);
            break;
        }
        
        case 2:
        {
            BuildHigherLayer(SVO_Node,  
                             SVO_AuxNode,
                             AtomicCounter2,
                             AtomicCounter);
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