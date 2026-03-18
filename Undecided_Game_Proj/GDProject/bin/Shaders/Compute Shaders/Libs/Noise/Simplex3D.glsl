float pointProcess(vec3 inPoint, vec3 gradientHash){
    const float RadiusFalloff = 0.5;

    float n = 0.0;
    float t0_sq = dot(inPoint, inPoint);
    float t0 = RadiusFalloff - t0_sq;
    if (t0 > 0.0) {
        float t0_pow3 = t0 * t0 * t0; 
        return n += t0_pow3 * dot(gradientHash, inPoint);
    }
    return n;
}

// Based off of Open Simplex 2
float simplex3D(vec3 inVector, const uint SEED){
    const float skewFactor = 0.333333333;
    const float unskewingFactor = 0.166666666;

    float simplexSum = inVector.x + inVector.y + inVector.z;
    float skew_offset = simplexSum * skewFactor;

    vec3 skewed_vec = inVector + skew_offset;
    ivec3 flooredVec = ivec3(floor(skewed_vec));

    float flooredVecSum = float(flooredVec.x + flooredVec.y + flooredVec.z);

    float unskew_offset = flooredVecSum * unskewingFactor;

    vec3 unskewed_flooredVec = vec3(flooredVec) - unskew_offset;
    
    vec3 pointZero = inVector - unskewed_flooredVec;

    ivec3 i1_offset, i2_offset;
    
    if (pointZero.x >= pointZero.y) {
        if (pointZero.y >= pointZero.z) { // X >= Y >= Z
            i1_offset = ivec3(1, 0, 0);
            i2_offset = ivec3(1, 1, 0);
        } else if (pointZero.x >= pointZero.z) { // X >= Z > Y
            i1_offset = ivec3(1, 0, 0);
            i2_offset = ivec3(1, 0, 1);
        } else { // Z > X >= Y
            i1_offset = ivec3(0, 0, 1);
            i2_offset = ivec3(1, 0, 1);
        }
    } else {
        if (pointZero.y < pointZero.z) { // Z > Y > X
            i1_offset = ivec3(0, 0, 1);
            i2_offset = ivec3(0, 1, 1);
        } else if (pointZero.x >= pointZero.z) { // Y >= X >= Z
            i1_offset = ivec3(0, 1, 0);
            i2_offset = ivec3(1, 1, 0);
        } else { // Y >= Z > X
            i1_offset = ivec3(0, 1, 0);
            i2_offset = ivec3(0, 1, 1);
        }
    }

    ivec3 i3_offset = ivec3(1, 1, 1);
    
    ivec3 i0_cell = flooredVec;
    ivec3 i1_cell = flooredVec + i1_offset;
    ivec3 i2_cell = flooredVec + i2_offset;
    ivec3 i3_cell = flooredVec + i3_offset;

    float i1_sum = float(i1_offset.x + i1_offset.y + i1_offset.z);
    vec3 pointOne = pointZero - (vec3(i1_offset) - i1_sum * unskewingFactor); 

    float i2_sum = float(i2_offset.x + i2_offset.y + i2_offset.z);
    vec3 pointTwo = pointZero - (vec3(i2_offset) - i2_sum * unskewingFactor);

    float i3_sum = float(i3_offset.x + i3_offset.y + i3_offset.z);
    vec3 pointThree = pointZero - (vec3(i3_offset) - i3_sum * unskewingFactor);

    vec3 gradientVec = gradient_hash(i0_cell, SEED);
    vec3 gradientVec1 = gradient_hash(i1_cell, SEED);
    vec3 gradientVec2 = gradient_hash(i2_cell, SEED);
    vec3 gradientVec3 = gradient_hash(i3_cell, SEED);

    float N = pointProcess(pointZero, gradientVec);
    N += pointProcess(pointOne, gradientVec1);
    N += pointProcess(pointTwo, gradientVec2);
    N += pointProcess(pointThree, gradientVec3);

    return clamp(N * 9, 0.0, 1.0);
}