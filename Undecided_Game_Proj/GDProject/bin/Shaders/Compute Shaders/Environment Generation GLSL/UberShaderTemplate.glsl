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

layout(local_size_x = WORKGROUP_SIZE_X, local_size_y = WORKGROUP_SIZE_Y, local_size_z = WORKGROUP_SIZE_Z) in;

#include "res://bin/Shaders/Compute Shaders/Libs/General/PCG_GENERATION_PushConstant.glsl"

#include "res://bin/Shaders/Compute Shaders/Libs/Noise/Hasher.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/Noise/Simplex3D.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/MortonCurve.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/SDFs.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/OctahedralMapping.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/MathLibs/CompressFloat.glsl"

// pre-processor definition to aid in pasting the uber shader text
#define _SHADER_IMPORT_ 0

float SampleDensity(uint Case, vec3 Coordinates, uint Seed)
{
    float FinalDensity = 0.0f;

    switch(Case)
    {
        #define _CASE_IMPORT_ 0

        default:
            FinalDensity = 0.0f;
    }

    return FinalDensity;
}



vec3 SolveCholeskyQEF(vec3 IntersectionNormals[12], vec3 IntersectionPoints[12], 
                vec3 Centroid, uint EdgeMask, vec3 AverageNormals)
{
    mat3 ATA = mat3(0.0);
    vec3 ATb = vec3(0.0);

    float p_count = 0;

    float n_divergence = 0;

    for(int i = 0; i < 12; i++)
    {
        if((EdgeMask & (1u << i)) == 0u) continue;

        //n_divergence += (1.0 - dot(AverageNormals, IntersectionNormals[i]));

        //p_count++;
    }

    if(p_count < 2) return Centroid;

    for(int i = 0; i < 12; i++)
    {
        if((EdgeMask & (1u << i)) == 0u) continue;
        
        vec3 n = IntersectionNormals[i];
        vec3 p = IntersectionPoints[i];
        
        //n_divergence += (1.0 - dot(AverageNormals, n));
        //p_count++;
        
        ATA[0][0] += n.x * n.x;
        ATA[0][1] += n.x * n.y;
        ATA[0][2] += n.x * n.z;
        
        ATA[1][1] += n.y * n.y;
        ATA[1][2] += n.y * n.z;
        
        ATA[2][2] += n.z * n.z;
        
        ATb += n * dot(n, p);
    }

    ATA[1][0] = ATA[0][1];
    ATA[2][0] = ATA[0][2];
    ATA[2][1] = ATA[1][2];

    float Lambda = 0.0001;

    vec3 Bias = Centroid;
    ATA[0][0] += Lambda;
    ATA[1][1] += Lambda;
    ATA[2][2] += Lambda;
    ATb += Bias * Lambda;
    
    if(determinant(ATA) < 1e-8) return Centroid;

    vec3 result;
    float L11 = sqrt(ATA[0][0]);
    float L21 = ATA[1][0] / L11;
    float L31 = ATA[2][0] / L11;

    //if(L11 < 0 || L21 < 0 || L31 < 0) return Centroid;

    float L22 = sqrt(ATA[1][1] - L21 * L21);
    float L32 = (ATA[2][1] - L31 * L21)/L22;
    
    float L33 = sqrt(ATA[2][2] - (L31 * L31 + L32 * L32));

    vec3 forward_subs;
    forward_subs.x = ATb.x / L11;
    forward_subs.y = (ATb.y - (L21 * forward_subs.x)) / L22;
    forward_subs.z = (ATb.z - (L31 * forward_subs.x + L32 * forward_subs.y)) / L33;
    
    // backward substitution
    result.z = forward_subs.z / L33;
    result.y = (forward_subs.y - (L32 * result.z)) / L22;
    result.x = (forward_subs.x - (L21 * result.y + L31 * result.z)) / L11;

    if(length(result - Centroid) > 2)
        return Centroid;

    //if(isinf(result.x) || isinf(result.y) || isinf(result.z)) return Centroid;

    //float StabilityThreshold = 0.2;
    return result;
}

void Stage_GenerateMesh()
{

}

void Stage_GenerateIndices()
{
    
}

void main()
{
    switch(PassOffset)
    {
        case 0:
            break;

        case 1:
            break;
    }
}