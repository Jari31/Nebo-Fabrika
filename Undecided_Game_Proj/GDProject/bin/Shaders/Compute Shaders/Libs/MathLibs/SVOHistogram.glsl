layout(local_size_x = 256) in;

// technically RadixSort 

struct SVO_NodeArray
{
    uint ChildPointer;
    uint ChildMask;

    uint Data;
    uint MortonAddress;
};

layout(std430, set = 2, binding = 0) buffer SVONodeBuffer{
    SVO_NodeArray SVO_Node[];
};

layout(std430, set = 2, binding = 2) buffer SVOAuxNodeBuffer{
    SVO_NodeArray SVO_AuxNode[]; //Buffer B
};

layout(std430, set = 3, binding = 0) buffer HistogramBuffer
{
    uint Histogram[6][16];
};

layout(std430, set = 3, binding = 1) buffer OffsetBuffer 
{
    uint Offsets[6][16];
};

layout(push_constant) uniform PushConstants {
    uint PassNum;
    uint PassOffset;
    uint padding[2];
};

shared uint LocalHistogram[16];

void Scatter()
{
    uint passNum = PassNum - 7;
    uint GlobalIndex = gl_GlobalInvocationID.x;  

    if(GlobalIndex >= SVO_Node.length()) // bounds check
        return;
    
    SVO_NodeArray Node = SVO_Node[GlobalIndex];

    uint Bucket = (SVO_Node.MortonAddress >> PassOffset) & 15u; // 21 >> 4 & 1111
                                                                // 10101 >> 4 & 1111; 0001 & 1 = 1; same logic from the histogram logic in main()

    uint OutputIndex = atomicAdd(Offsets[PassNum][Bucket], 1u);

    SVO_AuxNode[OutputIndex] = Node;
}


void main()
{
    if(PassNum <= 5)
    {
        // histogram
        if(gl_GlobalInvocationID.x < 16)
            LocalHistogram[gl_GlobalInvocationID.x] = 0;
        barrier();

        uint GlobalIndex = gl_GlobalInvocationID.x;

        if(GlobalIndex < SVO_Node.length())
        {
            uint MortonCode = SVO_node[GlobalIndex].MortonAddress;
            uint Bucket = (MortonCode >> PassOffset) & 15u;

            atomicAdd(LocalHistogram[Bucket], 1u);
        }
        barrier();

        if(gl_LocalInvocationID.x < 16)
        {
            uint BucketIndex = gl_LocalInvocationID.x;
            atomicAdd(HistogramBuffer.Histogram[PassNum][BucketIndex], LocalHistogram[BucketIndex]);
        }

        return;
    }

    Scatter();
}