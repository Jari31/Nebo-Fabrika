uint SVO_GetChildMask(uvec2 SVO_Nodes, uint NodeIndex, const uint SVO_NODE_SIZE){
    return SVO_Nodes[NodeIndex * SVO_NODE_SIZE].x & 0xff;
}

uint SVO_GetLeafMask(uvec2 SVO_Nodes, uint NodeIndex, const uint SVO_NODE_SIZE){
    return (SVO_Nodes[NodeIndex * SVO_NODE_SIZE].x >> 8) & 0xff;
}

uint SVO_GetChildOffset(uvec2 SVO_Nodes, uint NodeIndex, const uint SVO_NODE_SIZE){
    return SVO_Nodes[NodeIndex * SVO_NODE_SIZE + 1].x;
}

uint SVO_GetNthChildIndex(uvec2 SVO_Nodes, uint NodeIndex, const uint SVO_NODE_SIZE, uint ChildIndex){
    uint ChildMask = SVO_GetChildMask(SVO_Nodes, NodeIndex, SVO_NODE_SIZE);
    uint MaskBeforeChild = (1 << ChildIndex) - 1;

    uint AmountOfPreviousSiblings = bitCount(ChildMask & MaskBeforeChild);

    return SVO_GetChildOffset(SVO_Nodes, NodeIndex, SVO_NODE_SIZE) + AmountOfPreviousSiblings;
}
// the ones above have been deprecated, it was from a doc that I somewhat followed before abandoning it

uint flatten_coord(uvec3 coord, uvec3 dimensions) {
    return coord.x + (coord.y * dimensions.x) + (coord.z * dimensions.x * dimensions.y);
}

uint SVO_AtomicAllocator(uint count){
    return atomicAdd(NextAvailableNodeIndex, count);
}