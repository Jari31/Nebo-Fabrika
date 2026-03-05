#include "res://bin/Shaders/ComputeShaders/Libs/SVO_Utility.gdshader"

uint SVO_AtomicAllocator(uint count){
    return atomicAdd(NextAvailableNodeIndex, count);
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

void Stage_BuildSVOFromLeaves(){
    vec3 ParentNodeCoordinates = gl_GlobalInvocationID.xyz;
    vec3 ChildBaseCoordinates = ParentNodeCoordinates * 2;

    uint ChildMask = 0;
    uint LeafMask = 0;

    for(int i = 0; i < 8; i++){
        uvec3 offset = uvec3((i & 1), (i & 2) >> 1, (i & 4) >> 2);
        vec3 ChildCoordinates = uvec3(ChildBaseCoordinates) + offset;

        uint ChildIndex = flatten_coord(ChildCoordinates, VOXELS_PER_CHUNK.xyz);

        if (VoxelData[ChildIndex].density > 0.0)
            ChildMask |= (1 << i);
    }

    uvec3 ParentGridDimensions = VOXELS_PER_CHUNK.xyz / 2;
    uint ParentIndex = flatten_coord(ParentNodeCoordinates, ParentGridDimensions);



    if (!ChildMask){
        PtrGridA[ParentIndex] = 0;

        return;
    }
    
    uint NewNodeIndex = SVO_AtomicAllocator(1);

    PtrGridA[ParentIndex] = NewNodeIndex + 1;

    uvec2 Node;
    LeafMask = ChildMask;

    Node.x = (ChildMask) | (LeafMask << 8);
    Node.y = 0;

    SVO_Nodes[NewNodeIndex] = Node;
}

void Stage_BuildSVOHigherLayers(in uint32 PassNum){
    uvec3 ParentGridDimensions = VOXELS_PER_CHUNK.xyz >> PassNum;
    uvec3 ChildGridDimensions = VOXELS_PER_CHUNK.xyz >> (PassNum - 1);

    uvec3 ParentNodeCoordinates = gl_GlobalInvocationID.xyz;
    uvec3 ChildBaseCoordinates = ParentNodeCoordinates * 2;

    uint ChildMask = 0;
    uint LeafMask = 0;

    uint ChildNodePointer[8];

    for(int i = 0; i < 8; i++){
        uvec3 offset = uvec3((i & 1), (i & 2) >> 1, (i & 4) >> 2);
        uvec3 ChildCoordinates = ChildBaseCoordinates + offset;
        uint ChildIndex = flatten_coord(ChildCoordinates, ChildGridDimensions);

        uint Pointer = 0;

        if((PassNum - 1) % 2 == 1){
            Pointer = PtrGridA[ChildIndex];
        }
        else
        {
            Pointer = PtrGridB[ChildIndex];
        }

        ChildNodePointer[i] = Pointer;

        if(Pointer != 0){
            ChildMask |= (1 << i);

            if (!(SVO_Nodes[Pointer - 1].x & 0xFF)) {
                 LeafMask |= (1 << i);
            }
        }
    }

    uint ParentIndex = flatten_coord(ParentNodeCoordinates, ParentGridDimensions);
    uint OutputPointer = 0;

    if (ChildMask != 0){
        uint NumberOfChildren = bitCount(ChildMask);
        uint ChildBlockOffset = SVO_AtomicAllocator(NumberOfChildren);
        uint ParentNodeIndex = SVO_AtomicAllocator(1);

        uint CurrentChild = 0;
        for(int i = 0; i < 8; i++){
            if ((ChildMask & (1 << i)) != 0) {
                SVO_Nodes[ChildBlockOffset + CurrentChild] = SVO_Nodes[ChildNodePointer[i] - 1];
                CurrentChild++;
            }
        }

        uvec2 ParentNode;
        ParentNode.x = (ChildMask) | (LeafMask << 8);
        ParentNode.y = ChildBlockOffset;
        SVO_Nodes[ParentNodeIndex] = ParentNode;

        OutputPointer = ParentNodeIndex + 1;

        if (ParentGridDimensions.x == 1 && ParentGridDimensions.y == 1 && ParentGridDimensions.z == 1) {  
            uint saved_index = atomicAdd(NextAvailableSavedReferenceIndex, 1);
            
            SavedReferences_low[saved_index] = uvec4(ENTITY_LOCATION.x + ParentNodeCoordinates.x, ENTITY_LOCATION.y + ParentNodeCoordinates.y,
                                                     ENTITY_LOCATION.z + ParentNodeCoordinates.z, OutputPointer);
            SavedReferences_high[saved_index] = uvec4(ENTITY_LOCATION_P2.x + ParentNodeCoordinates.x, ENTITY_LOCATION_P2.y + ParentNodeCoordinates.y,
                                                     ENTITY_LOCATION_P2.z + ParentNodeCoordinates.z, OutputPointer)
        }
    }

    if (PassNum % 2 == 1){
        PtrGridB[ParentIndex] = OutputPointer;
    }
    else
    {
        PtrGridA[ParentIndex] = OutputPointer;
    }
}
