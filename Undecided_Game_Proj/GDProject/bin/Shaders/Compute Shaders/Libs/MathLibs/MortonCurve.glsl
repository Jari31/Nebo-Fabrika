// Morton Z Order Curve

uint ExpandBits(uint x)
{
    x = (x * 0x00010001) & 0xFF0000FF;
    x = (x * 0x00000101) & 0x0F00F00F;
    x = (x * 0x00000011) & 0xC30C30C3;
    x = (x * 0x00000005) & 0x49249249;

    return x;
}


uint DeInterleave(uint x)
{
    x &= 0x49249249;
    x = (x | (x >> 2))  & 0xC30C30C3;
    x = (x | (x >> 4))  & 0x0F00F00F;
    x = (x | (x >> 8))  & 0xFF0000FF;
    x = (x | (x >> 16)) & 0x000003FF;
    
    return x;
}

uint GetMortonCode(ivec3 xyz)
{
    return ExpandBits(xyz.x) | (ExpandBits(xyz.y) << 1) | (ExpandBits(xyz.z) << 2);
}