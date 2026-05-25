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

layout(local_size_x = WORKGROUP_SIZE_X, local_size_y = WORKGROUP_SIZE_Y, local_size_z = WORKGROUP_SIZE_Z) in;

#include "res://bin/Shaders/Compute Shaders/Libs/General/PCG_GENERATION_Set1.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/General/PCG_GENERATION_PushConstant.glsl"

ivec3 CHUNK_SIZE = ivec3(dCHUNK_SIZE);
ivec3 VOXELS_PER_CHUNK = ivec3(dVOXELS_PER_CHUNK);

const float NoiseScale1 = 0.003;
const float NoiseScale2 = 0.002;
const float NoiseScale3 = 0.005;
const float SDF_ApproxDistance = 2.0;

#include "res://bin/Shaders/Compute Shaders/Libs/Noise/Hasher.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/Noise/Simplex3D.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/MortonCurve.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/SDFs.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/OctahedralMapping.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/CompressFloat.glsl"

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

float sample_generator(vec3 Coordinates)
{
    float noiseLayer1 = simplex3D(Coordinates * NoiseScale1, SEED);
    //float noiseLayer2 = -simplex3D(VoxelLocalCoordinates * NoiseScale2, SEED);
    float noiseLayer3 = -simplex3D(Coordinates * NoiseScale3, SEED + 1);

    float Amp = 15.0f;

    //vec2 Dens_MatID = pickMatID(noiseLayer1, noiseLayer2, 1.0, 2.0);
    
    //float Sphere2 = sdSphere(vec3(gl_GlobalInvocationID) - vec3(GridSize/2 + 20), 147.5);
    //float Sphere = sdSphere(VoxelLocalCoordinates - vec3(TRUTH_GRID_SIZE/2), TRUTH_GRID_SIZE/2);
    float Box = sdBox(Coordinates.xyz, vec3(TRUTH_GRID_SIZE/2));
    //float Torus = sdTorus(VoxelLocalCoordinates - vec3(TRUTH_GRID_SIZE/2), vec2(TRUTH_GRID_SIZE/4, 50));

    return smooth_max(noiseLayer1, noiseLayer3, 0.01);//max(max(noiseLayer3, noiseLayer1), -Box);//Box;//max(Sphere, Box);//-Sphere;//
}

float calculate_central_difference(float d1, float d2)
{
    return (d1 - d2) * 0.5;
}

void Stage_GenerateLeaves(){
    vec3 VoxelLocalCoordinates = gl_GlobalInvocationID.xyz * VoxelSize;

    uint pGridSize = GridSize;
    uint Index = 0;  

    Index = gl_GlobalInvocationID.x + (gl_GlobalInvocationID.y * pGridSize) + (gl_GlobalInvocationID.z * pGridSize * pGridSize);
    
    if(gl_GlobalInvocationID.x >= GridSize - 1 || gl_GlobalInvocationID.y >= GridSize - 1 || gl_GlobalInvocationID.z >= GridSize - 1)
    {
        VoxelData[Index].matID = 0;
        VoxelData[Index].density = 0;
        return;
    }

    float FinalDensity = sample_generator(VoxelLocalCoordinates);
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