// An include directive style file system
// Every function must return a float and have a unique name

float SampleTemplateNoise(vec3 Coordinates, uint Seed)
{
    return simplex3D(Coordinates, Seed);
}