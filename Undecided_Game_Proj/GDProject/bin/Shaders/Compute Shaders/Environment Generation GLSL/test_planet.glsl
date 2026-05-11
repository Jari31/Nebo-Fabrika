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

#include "res://bin/Shaders/Compute Shaders/Libs/General/PCG_GENERATION_Set1.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/General/PCG_GENERATION_PushConstant.glsl"

ivec3 CHUNK_SIZE = ivec3(dCHUNK_SIZE);
ivec3 VOXELS_PER_CHUNK = ivec3(dVOXELS_PER_CHUNK);

#include "res://bin/Shaders/Compute Shaders/Libs/Noise/Hasher.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/Noise/Simplex3D.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/MortonCurve.glsl"

float TRUTH_GRID_SIZE = 64 * 4;

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

float sdTorus( vec3 p, vec2 t )
{
  vec2 q = vec2(length(p.xz)-t.x,p.y);
  return length(q)-t.y;
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
    vec3 VoxelLocalCoordinates = gl_GlobalInvocationID.xyz * VoxelSize;

    uint pGridSize = GridSize;
    uint Index = 0;  

    Index = gl_GlobalInvocationID.x + (gl_GlobalInvocationID.y * pGridSize) + (gl_GlobalInvocationID.z * pGridSize * pGridSize);
    
    if(gl_GlobalInvocationID.x >= GridSize - 1 || gl_GlobalInvocationID.y >= GridSize - 1 || gl_GlobalInvocationID.z >= GridSize - 1)
    {
        VoxelData[Index].matID = 0;
        VoxelData[Index].density = 1e-6;
        return;
    }
    
    const float NoiseScale1 = 0.003;
    const float NoiseScale2 = 0.002;
    const float NoiseScale3 = 0.005;
    const float SDF_ApproxDistance = 2.0;

    //float noiseLayer1 = simplex3D(VoxelLocalCoordinates * NoiseScale1, SEED);
    //float noiseLayer2 = -simplex3D(VoxelLocalCoordinates * NoiseScale2, SEED);
    float noiseLayer3 = -simplex3D(VoxelLocalCoordinates * NoiseScale3, SEED + 1);

    float Amp = 15.0f;

    //vec2 Dens_MatID = pickMatID(noiseLayer1, noiseLayer2, 1.0, 2.0);
    
    //float Sphere2 = sdSphere(vec3(gl_GlobalInvocationID) - vec3(GridSize/2 + 20), 147.5);
    //float Sphere = sdSphere(VoxelLocalCoordinates - vec3(TRUTH_GRID_SIZE/2), TRUTH_GRID_SIZE/2);
    //float Box = sdBox(VoxelLocalCoordinates.xyz, vec3(TRUTH_GRID_SIZE/2));
    //float Torus = sdTorus(VoxelLocalCoordinates - vec3(TRUTH_GRID_SIZE/2), vec2(TRUTH_GRID_SIZE/4, 50));

    float FinalDensity = noiseLayer3;//max(noiseLayer3, noiseLayer1), Box;//Box;//max(Sphere, Box);//-Sphere;//
    if(abs(FinalDensity) < 1e-6)
    {
        FinalDensity = (FinalDensity >= 0) ? 1e-6 : -1e-6;
    }

    VoxelData[Index].matID = 0;//floor(Dens_MatID.y);
    VoxelData[Index].density = FinalDensity;
}

void main() {
    Stage_GenerateLeaves();
}