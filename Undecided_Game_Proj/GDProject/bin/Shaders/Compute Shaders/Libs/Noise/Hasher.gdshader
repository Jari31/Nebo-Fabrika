float hash(ivec3 inVector){
    uint MultSum =  uint(inVector.x) * 374761393u ^ 
                    uint(inVector.y) * 1664525u ^
                    uint(inVector.z) * 1103515245u;

    MultSum = MultSum + (MultSum << 3);
    MultSum = MultSum ^ (MultSum >> 11);
    MultSum = MultSum + (MultSum << 15);

    return float(MultSum) * 2.3283064365386963e-10; // conv int to normal (normalized float)
}

vec3 gradient_hash(ivec3 inVector, const uint SEED){
    float rand_X = hash(inVector);
    float rand_Y = hash(inVector + (ivec3(4, 7, 13) * SEED));
    float rand_Z = hash(inVector + (ivec3(19, 23, 29) * SEED));

    vec3 outVector = vec3(rand_X, rand_Y, rand_Z) * 2.0 - 1.0;

    return outVector;
}