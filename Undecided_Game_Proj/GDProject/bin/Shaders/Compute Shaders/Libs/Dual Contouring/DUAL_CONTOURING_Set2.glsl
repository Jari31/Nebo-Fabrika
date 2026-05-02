//---------------------------------------------------------- set 2
/*
    layout(std430, set = 1, binding = 0) buffer vertexBuffer 
    {
        vec4 Vertices[];    
    };

    layout(std430, set = 1, binding = 1) buffer DC_NormalBuffer
    {
        vec4 Normals[];
    };

    layout(std430, set = 1, binding = 2) buffer DC_UVBuffer
    {
        vec2 UV[];
    };

    layout(std430, set = 1, binding = 3) buffer indexBuffer
    {
        uint Indices[];
    };
*/

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

