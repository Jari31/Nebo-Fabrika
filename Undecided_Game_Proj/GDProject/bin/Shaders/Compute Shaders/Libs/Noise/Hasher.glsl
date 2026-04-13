/*
    COPYRIGHT (c) 2026 Jari
    Licensed under the MIT license. Refer to the license file provided within the README for details.
*/

float hash(ivec3 inVector){
    uint MultSum =  uint(inVector.x) * 374761393u ^ 
                    uint(inVector.y) * 1664525u ^
                    uint(inVector.z) * 1103515245u;

    MultSum = MultSum + (MultSum << 3);
    MultSum = MultSum ^ (MultSum >> 11);
    MultSum = MultSum + (MultSum << 15);

    return float(MultSum) * 2.3283064365386963e-10; // conv int to normal (normalized float)
} // convert these into LUTs

vec3 gradient_hash(ivec3 inVector, const uint SEED){
    int iSEED = int(SEED);

    float rand_X = hash(inVector);
    float rand_Y = hash(inVector + (ivec3(4, 7, 13) * iSEED));
    float rand_Z = hash(inVector + (ivec3(19, 23, 29) * iSEED));

    vec3 outVector = vec3(rand_X, rand_Y, rand_Z) * 2.0 - 1.0;

    return outVector;
}

float cast_hash(float Var){
    int intVal = int(Var);

    intVal ^= intVal >> 16;
    intVal *= 0x45d9f3b;
    intVal ^= intVal >> 16;

    return (float(intVal) * (1.0 / 4294967296.0)) * 2.0 - 1;
}

vec3 cast_gradient_hash(ivec3 inVector, uint SEED)
{
    int iSEED = int(SEED);

    float rand_X = cast_hash(float(int(inVector.x) ^ (iSEED * 0x27D4EB2D)));
    float rand_Y = cast_hash(float(int(inVector.y) ^ (iSEED * 0x375E546B)));
    float rand_Z = cast_hash(float(int(inVector.z) ^ (iSEED * 0x4385DF64)));

    vec3 outVector = vec3(rand_X, rand_Y, rand_Z) - floor(vec3(rand_X, rand_Y, rand_Z));
    return outVector;
}

uint iqint1(uint x) {
    x = ((x >> 16) ^ x) * 0x45d9f3bu;
    x = ((x >> 16) ^ x) * 0x45d9f3bu;
    x = (x >> 16) ^ x;
    return x;
}

vec3 iqint_gradient_hash(ivec3 inVector, const uint SEED){
    int iSEED = int(SEED);

    float rand_X = iqint1(uint(inVector.x));
    float rand_Y = iqint1(uint(inVector.y) + (8 * iSEED));
    float rand_Z = iqint1(uint(inVector.z) + (8 * iSEED));

    vec3 outVector = vec3(rand_X, rand_Y, rand_Z) * 2.0 - 1.0;

    return outVector;
}