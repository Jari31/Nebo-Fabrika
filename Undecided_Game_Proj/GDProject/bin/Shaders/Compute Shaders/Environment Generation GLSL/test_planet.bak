#[compute]
#version 450
// feel free to copy and paste this to use as boilerplate. I can't separate any more of this into other files, as doing so would be a massive pain in the ass. 
#include "res://bin/Shaders/ComputeShaders/Libs/Noise/Simplex3D.gdshader"
#include "res://bin/Shaders/ComputeShaders/Libs/SVO/SVO_Utility.gdshader"
#include "res://bin/Shaders/ComputeShaders/Libs/SVO/SparseVoxelOctrees.gdshader"

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(std140, set = 0, binding = 0) ComputeUniforms
{
    uvec4 SCENE_PROPERTIES;

    vec4 NOISE_PARAMS;
                
    uvec4 CHUNK_SIZE;

    uvec4 VOXELS_PER_CHUNK;

    ivec4 ENTITY_LOCATION; 

    ivec4 ENTITY_LOCATION_P2;

    vec4 PLANET_BOUNDS;
}

layout(std450, set = 0, binding = 1) buffer voxelData {
    struct voxelData {
        float matID;
        float density;
    };
    voxelData VoxelData[];
};

layout(std450, set = 0, binding = 2) buffer SVONodeBuffer{
    uvec2 SVO_Nodes[];
}

layout(std450, set = 0, binding = 3) buffer AtomicCounter {
    uint NextAvailableNodeIndex;
    uint DC_AtomicCounter;
}

layout(std450, set = 0, binding = 4) buffer NodePointerGridA {
    uint PtrGridA[];
}

layout(std450, set = 0, binding = 5) buffer NodePointerGridB {
    uint PtrGridB[];
}

layout(std450, set = 0, binding = 6) buffer vertexBuffer {
    struct VBuffer{
    vec4 Position;
    uint MatID;
    }
    Vbuffer VertexBuffer[];
}

layout(std450, set = 0, binding = 7) buffer indicesBuffer {
    uint IndicesBuffer[];
}

layout(std450, set = 0, binding = 8) buffer activeCubes {
    uint ActiveCubes[];
}

layout(std450, set = 0, binding = 9) writeonly buffer savedReferences{
    uvec4 SavedReferences_low[];
    uvec4 SavedReferences_high[];
}

layout(push_constant) uniform pushConstant{
    uint PushConstant;
}
//...yes, those are a lot of buffers, but I don't think I can lower it more.

vec2 pickMatID(float noiseLayer1, float noiseLayer2, float matID_layer1, float matID_layer2){
    if(noiseLayer1 >= noiseLayer2){
        vec2 combined_Values = vec2(noiseLayer2, matID_layer2);

        return combined_Values;
    } 

    vec2 combined_Values = vec2(noiseLayer1, matID_layer1);

    return combined_Values;
}



void Stage_GenerateLeaves(){ // only thing you need to actually touch unless you're insane enough to optimize the other parts 

    vec3 VoxelLocalCoordinates = gl_GlobalInvocationID.xyz;
    vec3 ChunkWorldPosition = convert_to_ivec64(ENTITY_LOCATION, ENTITY_LOCATION_P2);
    vec3 VoxelWorldPosition = ChunkWorldPosition + VoxelLocalCoordinates;

    uint Index = flatten_coord(VoxelLocalCoordinates, VOXELS_PER_CHUNK.xyz);

    float PlanetRadius = PLANET_BOUNDS.w; 
    if(distance(VoxelWorldPosition, PLANET_BOUNDS.xyz) > PlanetRadius){
        VoxelData[Index].density = 0.0;
        VoxelData[Index].matID = 0.0;
        return;
    }

    const uint SEED = SCENE_PROPERTIES.x;
    const float WorldScale = NOISE_PARAMS.y;
    const float NoiseScale1 = 1.2;
    const float NoiseScale2 = 1.3;
    const float SDF_ApproxDistance = 2.0;

    float noiseLayer1 = simplex3D(VoxelWorldPosition * NoiseScale1, SEED) * SDF_ApproxDistance;
    float noiseLayer2 = simplex3D(VoxelWorldPosition * NoiseScale2, SEED) * SDF_ApproxDistance;

    vec2 Dens_MatID = pickMatID(noiseLayer1, noiseLayer2, 1.0, 2.0);

    if(density < 0 || density == 0){
        voxelData[index].matID = floor(Dens_MatID.y);
        VoxelData[index].density = Dens_MatID.x;
    }
}
/*
void Stage_GenerateVertices(){
    uvec3 CubeCoordinates = gl_GlobalInvocationID;
    float CornerDensities[8];
    uint SignMask = 0;

    if(!SignMask || SignMask == 0xFF)
        return;

    vec3 VertexPosition = vec3(CubeCoordinates) + 0.5;

    uint VertexIndex = atomicAdd(DC_AtomicCounter, 1);

    VertexBuffer[VertexIndex].Position = vec4()VertexPosition, 1.0;
    VertexBuffer[VertexIndex].MatID = 1;
}
*/
void main() {
    uint Stage = SCENE_PROPERTIES.w;
    uint PassNum = PushConstant.PassNum;

    switch (Stage)
    {
        case 0:
            Stage_GenerateLeaves();
            break;
        case 1:
            Stage_BuildSVOFromLeaves();
            break;
        case 2:
            Stage_BuildSVOHigherLayers(PassNum);
            break;
        case 3:
            Stage_GenerateVertices();
            break;
        case 4:
            Stage_GenerateIndices();
            break;
    }
}