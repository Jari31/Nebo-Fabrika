#[compute]
#version 450
#extension GL_GOOGLE_include_directive : require

layout(local_size_x = WORKGROUP_SIZE, local_size_y = WORKGROUP_SIZE, local_size_z = WORKGROUP_SIZE) in;

layout(std140, set = 0, binding = 0) uniform ComputeUniforms
{
    uvec4 SCENE_PROPERTIES;

    vec4 NOISE_PARAMS;
                
    uvec4 CHUNK_SIZE;

    uvec4 VOXELS_PER_CHUNK;

    ivec4 bENTITY_LOCATION; 

    ivec4 bENTITY_LOCATION_P2;

    vec4 PLANET_BOUNDS;
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
    
    ivec3 ENTITY_LOCATION;
    ivec3 ENTITY_LOCATION_P2;

    ivec3 CHUNK_SIZE;
    ivec3 VOXELS_PER_CHUNK;
};

layout(std430, set = 1, binding = 0) buffer voxelData {
    VoxelDataArray VoxelData[];
};

#include "../Libs/Noise/Hasher.glsl"
#include "../Libs/Noise/Simplex3D.glsl"
#include "../Libs/MathLibs/MortonCurve.glsl"

vec2 pickMatID(float noiseLayer1, float noiseLayer2, float matID_layer1, float matID_layer2){
    if(noiseLayer1 >= noiseLayer2){
        vec2 combined_Values = vec2(noiseLayer2, matID_layer2);

        return combined_Values;
    } 

    vec2 combined_Values = vec2(noiseLayer1, matID_layer1);

    return combined_Values;
}

vec3 convert_to_ivec64(ivec3 low, ivec3 high){
    vec3 low_f;
    vec3 high_f;

    low_f.x = float(low.x);
    low_f.y = float(low.y);
    low_f.z = float(low.z);

    high_f.x = float(high.x);
    high_f.y = float(high.y);
    high_f.z = float(high.z);

    return vec3(high_f) * 4294967296.0 + vec3(low_f);
}

void Stage_GenerateLeaves(){ // only thing you need to actually touch unless you're insane enough to optimize the other parts 

    vec3 VoxelLocalCoordinates = gl_GlobalInvocationID.xyz;
    vec3 ChunkWorldPosition = convert_to_ivec64(ENTITY_LOCATION.xyz, ENTITY_LOCATION_P2.xyz);
    // vec3 VoxelWorldPosition = vec3(ChunkWorldPosition) + vec3(VoxelLocalCoordinates);

    uint Index = GetMortonCode(ivec3(VoxelLocalCoordinates));  // flatten_coord(VoxelLocalCoordinates, VOXELS_PER_CHUNK.xyz);

    float PlanetRadius = PLANET_BOUNDS.w; 
    /*
    if(distance(VoxelWorldPosition, PLANET_BOUNDS.xyz) > PlanetRadius){
        VoxelData[Index].density = 0.0;
        VoxelData[Index].matID = 0.0;
        return;
    };
    */

    const uint SEED = SCENE_PROPERTIES.x;
    const float WorldScale = NOISE_PARAMS.y;
    const float NoiseScale1 = 1.2;
    const float NoiseScale2 = 1.3;
    const float SDF_ApproxDistance = 2.0;

    float noiseLayer1 = simplex3D(VoxelWorldPosition * NoiseScale1, SEED) * SDF_ApproxDistance;
    float noiseLayer2 = simplex3D(VoxelWorldPosition * NoiseScale2, SEED) * SDF_ApproxDistance;

    vec2 Dens_MatID = pickMatID(noiseLayer1, noiseLayer2, 1.0, 2.0);

    VoxelData[Index].matID = floor(Dens_MatID.y);
    VoxelData[Index].density = Dens_MatID.x;
}

void main() {
    Stage_GenerateLeaves();
}