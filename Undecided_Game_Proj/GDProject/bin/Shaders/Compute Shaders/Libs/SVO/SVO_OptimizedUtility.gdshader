// Morton Z Order Curve
uint ExpandBits(uint x)
{
    x = (x * 0x00010001) & 0xFF0000FF; // if x1 = 1; 0001 x 00010001 = 010001 & FF0000FF = 00000001 | 1
    x = (x * 0x00000101) & 0x0F00F00F; // x1 = 1 || x1 = 2 || X1 = 4
    x = (x * 0x00000011) & 0xC30C30C3; // x1 = 1 || x1 = 8 || X1 = 64
    x = (x * 0x00000005) & 0x49249249; // x1 = 1 || x1 = 8 || x1 = 64; x^3

    return x;
}

uint GetMortonCode(ivec3 xyz)
{
    return ExpandBits(xyz.x) | (ExpandBits(xyz.y) << 1) | (ExpandBits(xyz.z) << 2);
}