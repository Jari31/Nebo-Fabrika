layout(local_size_x = 16, local_size_y = 6) in;

layout(std430, set = 3, binding = 0) buffer HistogramBuffer
{
    uint Histogram[6][16];
};

layout(std430, set = 3, binding = 1) buffer OffsetBuffer 
{
    uint Offsets[6][16];
};

shared uint localOffsets[6][16]; // y | x because why not

void main() // 4 steps; O(log2 n)
{
    uint Row = gl_LocalInvocationID.y;
    uint Column = gl_LocalInvocationID.x;

    localOffsets[Row][Column] = Histogram[Row][Column];

    barrier();

    for(uint stride = 1; stride < 16; stride <<= 1)
    {
        uint Value = 0;
        if(Column >= stride)
        {
            Value = localOffsets[Row][Column - stride]; 
            /* i.e., 
            [3, 2, 4, 5]; 
            if Column = 1, stride = 1; 
            Column - stride = 1 - 1 = 0; 
            Value = [Row][0] = 3 
            */
        }

        barrier();

        if(Column >= stride)
        {
            localOffsets[Row][Column] += Value;
            /* i.e.,
            [3, 2, 4, 5];
            Column = 1, Stride = 1;
            localOffsets[Row][Column] = [3, 2, 4];
                                   index[0, 1, 2];
            localOffsets[Row][1] += Value = [3, 2 + 3, 4];
                                          = [3, 5, 4]
            */
        }
        barrier();
    }

    if(Column == 0)
    {
        Offsets[Row][Column] = 0; // exclusive, so the first index is always zero
    }
    else
    {
        Offsets[Row][Column] = localOffsets[Row][Column - 1]; // again, exclusive, so offset by - 1 make up for the fact
    }
}

/* Naive approach
void prefixSum()
{
    uint passNum = PassNum - 6; // local
    if(gl_GlobalInvocationID.x == 0)
    {
        uint Sum = 0;
        for(uint i = 0; i < 16; i++)
        {
            uint Count = Histogram[passNum][i];
            Offsets[passNum][i] = Sum;

            Sum += Count;
        }
    }
}
*/