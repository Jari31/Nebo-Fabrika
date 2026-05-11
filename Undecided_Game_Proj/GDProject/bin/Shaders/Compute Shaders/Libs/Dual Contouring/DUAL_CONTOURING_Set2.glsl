//---------------------------------------------------------- set 2

layout(set = 1, binding = 0, rgba32f) coherent uniform image2D  VertexTexture;
layout(set = 1, binding = 1, rgba32f) coherent uniform image2D  NormalTexture;
layout(set = 1, binding = 2, rgba32f) coherent uniform image2D  VertexTexture_B;
layout(set = 1, binding = 3, r32f)    writeonly uniform image2D  IndexTexture;

layout(std430, set = 1, binding = 4) buffer nodeVertexBuffer
{
    int Node_VertexIndex[];
};


layout(std430, set = 1, binding = 5) buffer nodeEdgeMaskBuffer
{
    uint Node_EdgeMask[];
};

layout(std430, set = 1, binding = 6) buffer vertexOffsets
{
    uint VertexOffsets[]
};