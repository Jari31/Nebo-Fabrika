#[compute]

#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

#include "res://bin/Shaders/Compute Shaders/Libs/General/PCG_GENERATION_Set1.glsl"
#include "res://bin/Shaders/Compute Shaders/Libs/Dual Contouring/DUAL_CONTOURING_Set2.glsl"

void main()
{
    vec3 NodeMin = vec3(InvocationID) * SizeOffset;
    vec3 NodeMax =  vec3(InvocationID + 1) * SizeOffset;

    ivec3 ivec3_NodeMin = ivec3(round(NodeMin));

    // edge configuration
    vec3 Corners[8];
    /*
    if(InvocationID.x > 1 && InvocationID.y > 1 && InvocationID.z > 1)
    {
    Corners[0] = ivec3_NodeMin + vec3(0, 0, 0);
    Corners[1] = ivec3_NodeMin + vec3(1, 0, 0);
    Corners[2] = ivec3_NodeMin + vec3(0, 1, 0);
    Corners[3] = ivec3_NodeMin + vec3(1, 1, 0);
    Corners[4] = ivec3_NodeMin + vec3(0, 0, 1);
    Corners[5] = ivec3_NodeMin + vec3(1, 0, 1);
    Corners[6] = ivec3_NodeMin + vec3(0, 1, 1);
    Corners[7] = ivec3_NodeMin + vec3(1, 1, 1);
    }
    else*/

    Corners[0] = ivec3_NodeMin + vec3(-SizeOffset, -SizeOffset, -SizeOffset);
    Corners[1] = ivec3_NodeMin + vec3(0,          -SizeOffset, -SizeOffset);
    Corners[2] = ivec3_NodeMin + vec3(-SizeOffset, 0,           -SizeOffset);
    Corners[3] = ivec3_NodeMin + vec3(0,          0,           -SizeOffset);
    Corners[4] = ivec3_NodeMin + vec3(-SizeOffset, -SizeOffset, 0         );
    Corners[5] = ivec3_NodeMin + vec3(0,          -SizeOffset, 0         );
    Corners[6] = ivec3_NodeMin + vec3(-SizeOffset, 0,           0         );
    Corners[7] = ivec3_NodeMin;

    float CornerDensities[8];
    CornerDensities[0] = GetCornerDensities(Corners[0]);
    CornerDensities[1] = GetCornerDensities(Corners[1]);
    CornerDensities[2] = GetCornerDensities(Corners[2]);
    CornerDensities[3] = GetCornerDensities(Corners[3]);
    CornerDensities[4] = GetCornerDensities(Corners[4]);
    CornerDensities[5] = GetCornerDensities(Corners[5]);
    CornerDensities[6] = GetCornerDensities(Corners[6]);
    CornerDensities[7] = GetCornerDensities(Corners[7]);

    uint Empty_Full = 0;
    for(int i = 0; i < 8; i++) 
    {
        if(CornerDensities[i] > 0.0f)
            Empty_Full |= 1u << i;
        else if(CornerDensities[i] < 0.0f)
            Empty_Full |= 1u << (i + 8);
        else if(CornerDensities[i] == 0.0f)
            Empty_Full |= 1u << (i + 16);
    }

    if((Empty_Full & 0xFF) == 255u || (Empty_Full & 0xFF00) == 255u || (Empty_Full & 0xFF0000) == 255u) return;

    const int EdgeCorners[12][2] = {
    // Edges on the bottom face (Z = -1)
    {0, 1}, {1, 3}, {3, 2}, {2, 0}, 
    // Edges on the top face (Z = 0)
    {4, 5}, {5, 7}, {7, 6}, {6, 4},
    // Vertical pillars connecting bottom to top
    {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    vec3 Normals = vec3(0.0);

    int IntersectionCount = 0;
    vec3 IntersectionSum = vec3(0.0);

    uvec3 DEBUG_intersect_centroid = uvec3(0.0f);

    uint EdgeMask = 0;
    for(int k = 0; k < 12; k++)
    {
        int i0   = EdgeCorners[k][0]; // i = index
        int i1   = EdgeCorners[k][1];
        float d0 = CornerDensities[i0]; // d = density
        float d1 = CornerDensities[i1];

        if(d0 * d1 < 0.0)
        {
            float denom = d1 - d0;
            //if (abs(denom) < 1e-6) continue;
            float t = -d0 / denom;
            //if(abs(t) < 0.00001) continue;
            //clamp(t, 0.00001, 1 - 0.00001);

            vec3 n;
            vec3 p;

            vec3 CornerA = Corners[i0];
            vec3 CornerB = Corners[i1];

            bool CornerA_OutOfBounds = OutOfBoundsCheck(CornerA);
            bool CornerB_OutOfBounds = OutOfBoundsCheck(CornerB);
            
            // normals
            /*
            if(!CornerA_OutOfBounds && !CornerB_OutOfBounds)
            {
                n = mix(CalculateNormals(CornerA), CalculateNormals(CornerB), t);
            }
            else if(!CornerA_OutOfBounds && CornerB_OutOfBounds)
            {
                n = CalculateNormals(CornerA);
            }
            else if(CornerA_OutOfBounds && !CornerB_OutOfBounds)
                n = CalculateNormals(CornerB);
            else continue;*/


            p = mix(CornerA, CornerB, t);


            //vec3 p = mix(Corners[i0], Corners[i1], t);
            n = mix(CalculateNormals(Corners[i0]), CalculateNormals(Corners[i1]), t);
            //vec3 n = CalculateNormalGradient(p);
            
            Normals += n; 

            IntersectionSum += p;
            IntersectionCount++;

            EdgeMask |= (1u << k);
            if(d0 > d1)
                EdgeMask |= (1u << (k + 12));

            DEBUG_intersect_centroid = uvec3(t * 255.0, 0, 0);

            //if(t < 0.01 || t > 0.99) {
            //    DEBUG_intersect_centroid = uvec3(1, 0, 0);
            //}
        }
    }
}