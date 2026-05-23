// A composition and include directive style file system
// Each comp file must have two things in order to work:
// includes of the biomes
// switch cases

#include "res://bin/Shaders/Compute Shaders/Environment Generation GLSL/Biomes/Biome_Template.glsl"

case 0:
    SampleTemplateNoise();
case 1:
    Noise();
case 4:
    Something();