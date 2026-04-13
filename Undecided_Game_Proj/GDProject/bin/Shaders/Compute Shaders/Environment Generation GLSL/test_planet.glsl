#[compute]
#version 450

#ifndef WORKGROUP_SIZE_X
#define WORKGROUP_SIZE_X 8
#endif
#ifndef WORKGROUP_SIZE_Y
#define WORKGROUP_SIZE_Y 8
#endif
#ifndef WORKGROUP_SIZE_Z
#define WORKGROUP_SIZE_Z 8
#endif

#extension GL_GOOGLE_include_directive : require

/*
    COPYRIGHT (c) 2026 Jari
    Licensed under the MIT license. Refer to the license file provided within the README for details.
*/

layout(local_size_x = WORKGROUP_SIZE_X, local_size_y = WORKGROUP_SIZE_Y, local_size_z = WORKGROUP_SIZE_Z) in;

layout(std140, set = 0, binding = 0) uniform ComputeUniforms
{
    uvec4 SCENE_PROPERTIES;

    vec4 NOISE_PARAMS;
                
    uvec4 ubCHUNK_SIZE;

    uvec4 ubVOXELS_PER_CHUNK;

    ivec4 ubENTITY_LOCATION; 

    ivec4 ubENTITY_LOCATION_P2;

    vec4 PLANET_BOUNDS;
};

// --------------------------------------------------- set 1

// --------------------------------------------------- struct
struct SVO_NodeArray
{
    uint ChildPointer;
    uint ChildMask;

    uint Data;
    uint MortonAddress;

    uint densities_lower4;
    uint densities_higher4;
};

struct VoxelDataArray 
{
    float matID;
    float density;
};

// --------------------------------------------------- buff

layout(std430, set = 1, binding = 0) buffer voxelData {
    VoxelDataArray VoxelData[];
};

layout(std430, set = 1, binding = 1) buffer SVONodeBuffer
{
    SVO_NodeArray SVO_Node[]; //Buffer A
};

layout(std430, set = 1, binding = 2) buffer SVOAuxNodeBuffer
{
    SVO_NodeArray SVO_AuxNode[]; //Buffer B
};

layout(std430, set = 1, binding = 3) buffer atomicCounter
{
    uint AtomicCounter;
    uint AtomicCounter2;

    uint VertexCounter;
};

layout(std430, set = 1, binding = 4) buffer HistogramBuffer
{
    uint Histogram[6][16];
};

layout(std430, set = 1, binding = 5) buffer OffsetBuffer 
{
    uint Offsets[6][16];
};

//-------------------------------------------------------- push const
layout(push_constant) uniform PushConstants 
{
    uint  PassNum;
    uint  PassOffset;
    uint  PassStage;
    uint  Dense_SaveAsMortonCode;

    uint  Dense_TotalNodes;
    uint  SEED;
    float SVO_VoxelSize;
    uint SVO_BufferSize;

    uint HASH_SIZE;
    uint pad1;
    uint pad2;
    uint pad3;
    
    ivec4 dCHUNK_SIZE;
    ivec4 dVOXELS_PER_CHUNK;
};

ivec3 CHUNK_SIZE = ivec3(dCHUNK_SIZE);
ivec3 VOXELS_PER_CHUNK = ivec3(dVOXELS_PER_CHUNK);


#include "res://bin/Shaders/Compute Shaders/Libs/Noise/Hasher.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/Noise/Simplex3D.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/MortonCurve.glsl"

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

// todo: switch to shared memory access. the GPU can more than handle the 2ms goal. it's just that it's taking too long to write to the global buffer
// *maybe

void Stage_GenerateLeaves(){ // only thing you need to actually touch unless you're insane enough to optimize the other parts 
    uvec3 VoxelLocalCoordinates = gl_GlobalInvocationID.xyz;
    // uvec3 ChunkWorldPosition = convert_to_ivec64(ENTITY_LOCATION.xyz, ENTITY_LOCATION_P2.xyz);
    // vec3 VoxelWorldPosition = vec3(ChunkWorldPosition) + vec3(VoxelLocalCoordinates);

    uint Index = 0;  
    if(Dense_SaveAsMortonCode != 0)
        Index = GetMortonCode(ivec3(VoxelLocalCoordinates));
    else
        Index = VoxelLocalCoordinates.x + (VoxelLocalCoordinates.x * (VoxelLocalCoordinates.y + VoxelLocalCoordinates.y * VoxelLocalCoordinates.z));

    // inefficient; just calculate the distance on the CPU and pass it with a push constant and approximate the bounds on the GPU
    // float PlanetRadius = PLANET_BOUNDS.w; 
    /*
    if(distance(VoxelWorldPosition, PLANET_BOUNDS.xyz) > PlanetRadius){
        VoxelData[Index].density = 0.0;
        VoxelData[Index].matID = 0.0;
        return;
    };
    */

    const uint SEED = SCENE_PROPERTIES.x;
    const float WorldScale = NOISE_PARAMS.y;
    const float NoiseScale1 = 0.02;
    const float NoiseScale2 = 0.04;
    const float SDF_ApproxDistance = 2.0;

    float noiseLayer1 = simplex3D(VoxelLocalCoordinates * NoiseScale1, SEED);
    float noiseLayer2 = simplex3D(VoxelLocalCoordinates * NoiseScale2, SEED);

    vec2 Dens_MatID = pickMatID(noiseLayer1, noiseLayer2, 1.0, 2.0);

    VoxelData[Index].matID = floor(Dens_MatID.y);
    VoxelData[Index].density = Dens_MatID.x;
}

void main() {
    Stage_GenerateLeaves();
}