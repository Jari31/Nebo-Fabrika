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
    uint VertexOffsets[];
};

layout(std430, set = 1, binding = 7) buffer vertexBuffer
{
    vec4 VertexBuffer[];
};

layout(std430, set = 1, binding = 8) buffer normalBuffer
{
    vec4 NormalBuffer[];
};

struct Triangle {
    vec4 OriginNormal;

    uint VIndex[4];
    float EdgeBudget[4];
};

layout(std430, set = 1, binding = 9) buffer triangleBuffer
{
    Triangle TriangleBuffer[];
};

layout(std430, set = 1, binding = 10) buffer indirectBuffer
{
    uint x;
    uint y;
    uint z;
    uint w;
} indirect_dispatch_params;

