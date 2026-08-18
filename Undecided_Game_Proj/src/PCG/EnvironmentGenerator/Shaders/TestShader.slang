import MathLibraries.Noise.Simplex3D;

// Macro Definitions
#define THREADS_X_3D 8
#define THREADS_Y_3D 8
#define THREADS_Z_3D 8

#define MAX_OCTAVES 8
#define DEFAULT_FREQUENCY 0.05f
#define DEFAULT_PERSISTENCE 0.5f
#define DEFAULT_LACUNARITY 2.0f

#define USE_PER_INSTANCE_SEED 1
#define ENABLE_DOMAIN_WARPING 1

// Struct Definitions
struct DomainWarpParams
{
    float3 warpOffset;
    float warpStrength;
    float warpFrequency;
    int pad0;
    int pad1;
    int pad2;
};

struct NoiseConfig
{
    float4 colorTint;
    float frequency;
    float persistence;
    float lacunarity;
    int octaves;
    uint seed;
    float blendMode;
    float threshold;
};

// Push Constants
struct PushConstants
{
    float4x4 worldToLocal;
    float3 globalOffset;
    float time;
    uint instanceID;
    uint flags;
};

// Global / Uniform Buffer Inputs
[[vk::push_constant]] PushConstants g_PushData;
ConstantBuffer<NoiseConfig> g_ConfigBuffer;
StructuredBuffer<DomainWarpParams> g_WarpParams;
RWTexture3D<float4> g_OutputTexture;

// Helper Functions with Extra Params
float SampleWarpedNoise(
    float3 pos,
    float frequency,
    float persistence,
    float lacunarity,
    int octaves,
    uint seed,
    DomainWarpParams warp,
    out float3 outWarpedPos)
{
    float total = 0.0f;
    float amplitude = 1.0f;
    float freq = frequency;

    #if ENABLE_DOMAIN_WARPING
        float3 warpVal = float3(
            Simplex3D(pos * warp.warpFrequency + warp.warpOffset, seed),
            Simplex3D(pos * warp.warpFrequency + warp.warpOffset + float3(17.0, 31.0, 47.0), seed),
            Simplex3D(pos * warp.warpFrequency + warp.warpOffset + float3(83.0, 97.0, 113.0), seed)
        );
        pos += warpVal * warp.warpStrength;
    #endif

    outWarpedPos = pos;

    [unroll]
    for (int i = 0; i < MAX_OCTAVES; ++i)
    {
        if (i >= octaves) break;

        float n = Simplex3D(pos * freq, seed + uint(i));
        total += n * amplitude;

        amplitude *= persistence;
        freq *= lacunarity;
    }

    return total;
}

[shader("compute")]
[numthreads(THREADS_X_3D, THREADS_Y_3D, THREADS_Z_3D)]
void main(
    uint3 threadID : SV_DispatchThreadID,
    uint3 groupID : SV_GroupID,
    uint3 groupThreadID : SV_GroupThreadID,
    uint groupIndex : SV_GroupIndex)
{
    float3 worldPos = (float3)threadID + g_PushData.globalOffset;

    DomainWarpParams warp = g_WarpParams[g_PushData.instanceID];

    float3 finalPosition = float3(0.0f, 0.0f, 0.0f);

    float noiseVal = SampleWarpedNoise(
        worldPos,
        g_ConfigBuffer.frequency,
        g_ConfigBuffer.persistence,
        g_ConfigBuffer.lacunarity,
        g_ConfigBuffer.octaves,
        g_ConfigBuffer.seed ^ g_PushData.flags,
        warp,
        finalPosition
    );

    float alpha = saturate((noiseVal - g_ConfigBuffer.threshold) * 5.0f);
    float4 finalColor = g_ConfigBuffer.colorTint * alpha;

    if (all(threadID < uint3(256, 256, 256)))
    {
        g_OutputTexture[threadID] = finalColor;
    }
}
