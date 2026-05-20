#ifndef DC_MATH
#define DC_MATH
/*
    COPYRIGHT (c) 2026 Jari
    Licensed under the MIT license. Refer to the license file provided within the README for details.
*/
/*
float UnpackDensity(uint PackedData, uint Index)
{
    uint byte = (PackedData >> (Index * 8)) & 0xFF;
    return ((float(byte) * 0.007843137254902) - 1.0);
}


float TrilinearSample(vec3 Point, vec3 CellMin, float CellSize, float[8] CornerDensities)
{
    vec3 t = (Point - CellMin) / CellSize; // convert to local space; to the voxel itself

    float b0 = mix(mix(CornerDensities[0], CornerDensities[1], t.x),
                   mix(CornerDensities[2], CornerDensities[3], t.x), t.y);

    float b1 = mix(mix(CornerDensities[4], CornerDensities[5], t.x),
                   mix(CornerDensities[6], CornerDensities[7], t.x), t.y);

    return mix(b0, b1, t.z);
}

vec3 TrilinearGradient(vec3 Point, vec3 CellMin, float CellSize, float[8] CornerDensities)
{
    vec3 t = (Point - CellMin) / CellSize; // localize it
    float tx = t.x, ty = t.y, tz = t.z;

    float wx0 = 1.0 - tx, wx1 = tx; // w = weight. I'm too lazy to full-form it
    float wy0 = 1.0 - ty, wy1 = ty;
    float wz0 = 1.0 - tz, wz1 = tz;

    // i lost the plot on what this is supposed to be doing. 
    // it's just weighted linear interpolation but thrice
    float gx = wz0 * (wy0 * (CornerDensities[1] - CornerDensities[0]) + wy1 * (CornerDensities[3] - CornerDensities[2])) +
               wz1 * (wy0 * (CornerDensities[5] - CornerDensities[4]) + wy1 * (CornerDensities[7] - CornerDensities[6]));
    
    float gy = wz0 * (wx0 * (CornerDensities[2] - CornerDensities[0]) + wx1 * (CornerDensities[3] - CornerDensities[1])) +
               wz1 * (wx0 * (CornerDensities[6] - CornerDensities[4]) + wx1 * (CornerDensities[7] - CornerDensities[5]));
    
    float gz = wy0 * (wx0 * (CornerDensities[4] - CornerDensities[0]) + wx1 * (CornerDensities[5] - CornerDensities[1])) +
               wy1 * (wx0 * (CornerDensities[6] - CornerDensities[2]) + wx1 * (CornerDensities[7] - CornerDensities[3]));                                           

    return vec3(gx, gy, gz) / CellSize;
}

// Binary search
int FindNodeByMortonCode(uint MortonCode, uint TotalNodes)
{
    uint Low = 0; 
    uint High = TotalNodes - 1;
    while(Low <= High)
    {
        uint Mid = (Low + High) >> 1; // or, / 2. i mean, the compiler is going to turn /2 into >> 1 by itself anyways, but eh
        uint MidMorton = 0;
        if(PassStage > 0)
            MidMorton = SVO_Node[Mid].MortonAddress;
        else
            MidMorton = SVO_AuxNode[Mid].MortonAddress;

        if(MidMorton < MortonCode) 
            Low = Mid + 1;
        else if(MidMorton > MortonCode)
            High = Mid - 1;
        else
            return int(Mid);
    }
    return -1;
}
*/
void store_index(uint flat_idx, uint vertex_val) {
    int x = int(flat_idx % dVOXELS_PER_CHUNK.w);
    int y = int(flat_idx / dVOXELS_PER_CHUNK.w);
    imageStore(IndexTexture, ivec2(x, y), vec4(float(vertex_val), 0, 0, 0));
}

uint store_vertices_and_normals(vec4 Centroid, vec4 Normals) {
    uint VertexIndex = atomicAdd(VertexCounter, 1) + StorageOffset;

    if(WriteToTexturesInFirstPass > 0){
    int VertexIndex_x = int(VertexIndex % dCHUNK_SIZE.w);
    int VertexIndex_y = int(VertexIndex / dCHUNK_SIZE.w);

    imageStore(VertexTexture, ivec2(VertexIndex_x, VertexIndex_y), Centroid);
    imageStore(NormalTexture, ivec2(VertexIndex_x, VertexIndex_y), Normals);
    }
    else
    {
    VertexBuffer[VertexIndex] = Centroid;
    NormalBuffer[VertexIndex] = Normals;
    }
    return VertexIndex;
}

vec3 project_on_plane(vec3 v, vec3 normal)
{
    return v - dot(v, normal) * normal;
}
#endif