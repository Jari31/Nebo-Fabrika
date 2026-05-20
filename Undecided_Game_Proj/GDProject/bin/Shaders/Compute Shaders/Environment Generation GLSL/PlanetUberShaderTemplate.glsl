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

float SampleDensity(uint Case)
{
    switch(Case)
    {

    }
}

main()
{


}