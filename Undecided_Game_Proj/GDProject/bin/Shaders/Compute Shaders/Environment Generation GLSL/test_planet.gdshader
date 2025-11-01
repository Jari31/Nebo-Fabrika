#[compute]
#version 450

#include "res://bin/Shaders/ComputeShaders/Environment Generation GLSL/Libs/Simplex3D.gdshader"

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(std140, set = 0, binding = 0) ComputeUniforms
{
    uint is_STARTING_SCENE;
    uint SEED;
    uint MAXVERTs;
    
    uint CHUNKSIZE_X;
    uint CHUNKSIZE_Y;
    uint CHUNKSIZE_Z;
    
    uint VOXELS_PER_CHUNK_X;
    uint VOXELS_PER_CHUNK_Y;
    uint VOXELS_PER_CHUNK_Z;

    uint CURRENT_ENTITY_LOCATION_X;
    uint CURRENT_ENTITY_LOCATION_Y;
    uint CURRENT_ENTITY_LOCATION_Z;
}

layout(std430, set = 0, binding = 2) buffer VoxelData {
    uint voxels[]; 
};

void main() {
    vec3 VoxelLocalCoordinates = gl_GlobalInvocationID.xyz;

    vec3 ChunkWorldOrigin = vec3(CURRENT_ENTITY_LOCATION_X, CURRENT_ENTITY_LOCATION_Y, CURRENT_ENTITY_LOCATION_Z);

    vec3 VoxelWorldPosition = ChunkWorldOrigin + VoxelLocalCoordinates;

    float NoiseAmount = 0.1;
    vec3 NoiseInVec = VoxelWorldPosition * NoiseAmount;

    float density = simplex3D(NoiseInVec);

    if(density > 0){

    }

    uint Index = VoxelLocal.x + (VoxelLocal.y * VOXELS_PER_CHUNK_X) + 
                 (VoxelLocal.z * VOXELS_PER_CHUNK_X * VOXELS_PER_CHUNK_Y);
}