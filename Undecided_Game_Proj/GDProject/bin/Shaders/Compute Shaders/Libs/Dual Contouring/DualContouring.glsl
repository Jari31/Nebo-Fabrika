#[compute]

#version 450

layout(local_size_x = WORKGROUP_SIZE, local_size_y = WORKGROUP_SIZE, layout_size_z = WORKGROUP_SIZE) in;
#extension GL_GOOGLE_include_directive : require

#include ""../MathLibs/MortonCurve.glsl"

struct HermiteData
{
    vec3 Position;
    vec3 Normals;
    uint Valid;
};

struct VoxelDataArray 
{
    float matID;
    float density;
};

layout(push_constant) uniform PushConstants 
{
    uint PassNum;
    uint PassOffset;
    uint PassStage;

    uint SEED;
    
    ivec4 ENTITY_LOCATION;
    ivec3 ENTITY_LOCATION_P2;

    ivec4 CHUNK_SIZE;
    ivec4 VOXELS_PER_CHUNK;
};

layout(std430, set = 1, binding = 0) buffer voxelData {
    VoxelDataArray VoxelData[];
};

layout(std430, set = 4, binding = 0) hermiteBuffer
{
    HermiteData HermiteData[][12];
};

const uvec2 TABLE_CUBE_EDGES[12] = 
{
    uvec2(0, 1), uvec2(1, 2), uvec2(2, 3), uvec2(3, 0), // bottom face
    uvec2(4, 5), uvec2(5, 6), uvec2(6, 7), uvec2(7, 4), // top face
    uvec2(0, 4), uvec2(1, 5), uvec2(2, 6), uvec2(3, 7)  // vertical edges   
};

const uvec2 TABLE_CUBE_VERTICES[8] = 
{
    uvec3(0, 0, 0), uvec3(1, 0, 0), uvec3(1, 1, 0), uvec3(0, 1, 0),
    uvec3(0, 0, 1), uvec3(1, 0, 1), uvec3(1, 1, 1), uvec3(0, 1, 1)
};

uint FlattenCell(uvec3 CellCoordinates, uvec3 GridSize)
{
    return CellCoordinates.x + CellCoordinates.y * GridSize.x + CellCoordinates.z * GridSize.x * GridSize.y;
    // a + bx + cxy
}

buffer float SampleDensity(uvec3 VoxelPosition)
{
    if(VoxelPosition.x >= VOXELS_PER_CHUNK.x || VoxelPosition.y >= VOXELS_PER_CHUNK.y || VoxelPosition.z >= VOXELS_PER_CHUNK.z)
    {
        return 1000.0;
    }

    uint Index = FlattenCell(VoxelPosition, VOXELS_PER_CHUNK);

}

vec3 CalculateGradient(uvec3 VoxelPosition)
{
    float dx = sample
}