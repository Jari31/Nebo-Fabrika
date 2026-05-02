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

#include "res://bin/Shaders/Compute Shaders/Libs/General/PCG_GENERATION_PushConstant.glsl"

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

float smooth_max(float a, float b, float k){
    float h = max(k - abs(a - b), 0.0)/k;

    return float(max(a, b) + (h * h) * k * 0.25);
}

float sdBox( vec3 p, vec3 b )
{
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

float sdSphere( vec3 p, float r )
{
  return length(p) - r;
}

float calculate_distance_to_boundary(vec3 point, float radius)
{
    vec3 absolute_position = abs(point);

    float max_distance = max(absolute_position.x, max(absolute_position.y, absolute_position.z));
    
    return radius - max_distance;
}

float converge_boundary(float raw_value, float margin)
{
    float _distance = calculate_distance_to_boundary(gl_GlobalInvocationID.xyz, GridSize);
    float t = clamp(_distance / margin, 0.0f, 1.0f);

    float smooth_t = t * t * (3.0f - 2.0f * t);

    return mix(0.1f, raw_value, smooth_t);
}


// todo: switch to shared memory access. the GPU can more than handle the 2ms goal. it's just that it's taking too long to write to the global buffer
// *maybe

void Stage_GenerateLeaves(){ // only thing you need to actually touch unless you're insane enough to optimize the other parts 
    uvec3 VoxelLocalCoordinates = gl_GlobalInvocationID.xyz;

    uint pGridSize = GridSize;
    uint Index = 0;  
    //if((FLAG & 1u) != 0)
    //    Index = GetMortonCode(ivec3(VoxelLocalCoordinates));
    //else
        Index = VoxelLocalCoordinates.x + (VoxelLocalCoordinates.y * pGridSize) + (VoxelLocalCoordinates.z * pGridSize * pGridSize);

    const float WorldScale = NOISE_PARAMS.y;
    const float NoiseScale1 = 0.017;
    const float NoiseScale2 = 0.01;
    const float NoiseScale3 = 0.015;
    const float SDF_ApproxDistance = 2.0;

    float noiseLayer1 = simplex3D(VoxelLocalCoordinates * NoiseScale1, SEED);
    //float noiseLayer2 = -simplex3D(VoxelLocalCoordinates * NoiseScale2, SEED);
    //float noiseLayer3 = -simplex3D(VoxelLocalCoordinates * NoiseScale3, SEED + 1);

    float Amp = 15.0f;

    //vec2 Dens_MatID = pickMatID(noiseLayer1, noiseLayer2, 1.0, 2.0);
    
    //float Sphere2 = sdSphere(vec3(gl_GlobalInvocationID) - vec3(GridSize/2 + 20), 147.5);
    float Sphere = -sdSphere(vec3(gl_GlobalInvocationID) - vec3(GridSize/2), GridSize/2);
    float Box = -sdBox(vec3(gl_GlobalInvocationID.xyz), vec3(GridSize/2 - 50));

    float FinalDensity = noiseLayer1;//min(Box, Sphere);//-Sphere;//max(Sphere, smooth_max(noiseLayer3, noiseLayer1, 0.2) * Amp);
    //if(VoxelLocalCoordinates.x > 0 && VoxelLocalCoordinates.x < GridSize -1 && 
    //    VoxelLocalCoordinates.y > 0 && VoxelLocalCoordinates.y < GridSize -1 &&
    //    VoxelLocalCoordinates.z > 0 && VoxelLocalCoordinates.z < GridSize -1)
    //    FinalDensity = min(-1.0f, FinalDensity);
    if(abs(FinalDensity) < 1e-6)
    {
        FinalDensity = (FinalDensity >= 0) ? 1e-6 : -1e-6;
    }

    VoxelData[Index].matID = 0;//floor(Dens_MatID.y);
    VoxelData[Index].density = -FinalDensity;
}

void main() {
    Stage_GenerateLeaves();
}