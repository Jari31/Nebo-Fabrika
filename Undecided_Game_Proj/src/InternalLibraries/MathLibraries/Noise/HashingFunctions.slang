module HashingFunctions;

public inline float3 ScalarGradientHash(float3 InputVector, uint Seed)
{
    uint x = asuint(InputVector.x);
    uint y = asuint(InputVector.y);
    uint z = asuint(InputVector.z);

    x = (x ^ Seed) * 0x45d9f3bu;
    y = (y ^ Seed) * 0x135a50f3u;
    z = (z ^ Seed) * 0x2c641115u;

    x ^= x >> 16;
    y ^= y >> 16;
    z ^= z >> 16;

    uint hash0 = x * 0x9e3779b9u;
    uint hash1 = y * 0x9e3779b9u;
    uint hash2 = z * 0x9e3779b9u;

    hash0 = ((hash0 >> 11) | (hash0 << 21)) ^ ((hash1 * 0x7331bf4bu) >> 13);
    hash1 = ((hash1 >> 11) | (hash1 << 21)) ^ ((hash2 * 0x7331bf4bu) >> 13);
    hash2 = ((hash2 >> 11) | (hash2 << 21)) ^ ((hash0 * 0x7331bf4bu) >> 13);

    float scale = 2.0f / 4294967296.0f;
    return float3(
        fma(float(hash0), scale, -1.0f),
        fma(float(hash1), scale, -1.0f),
        fma(float(hash2), scale, -1.0f)
    );
}
