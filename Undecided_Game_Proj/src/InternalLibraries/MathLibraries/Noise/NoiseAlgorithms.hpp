#pragma once

#include "Libraries/include/cpuinfo.h"
#include "Libraries/include/enkiTS/TaskScheduler.h"
#include "ShaderCompiler/ShaderCompiler.hpp"
#include "godot_cpp/variant/utility_functions.hpp"
#include "ispc/Simplex3D.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <system_error>
#include <vector>
namespace NoiseAlgorithms
{
namespace SIMD_Helpers
{
constexpr uint8_t ISPC_DEFAULT_SUPPORTED_LANES = 8;
constexpr uint8_t ISPC_MINIMUM_SUPPORTED_LANES = 4;
constexpr uint8_t ISPC_MAXIMUM_SUPPORTED_LANES = 16;

static auto QuerySIMDLanes() -> uint8_t
{
    static bool is_initialized = false;
    if (!is_initialized)
    {
        if (!cpuinfo_initialize())
        {
            return ISPC_MINIMUM_SUPPORTED_LANES; // generic-i32x4
        }
        is_initialized = true;
    }

#if CPUINFO_ARCH_X86 || CPUINFO_ARCH_X86_64
    // 16 lanes (512-bit)
    // avx512skx target requires AVX-512 Foundation, Vector Length, Doubleword/Quadword,
    // and Byte/Word instructions (Skylake-X feature set).
    if (cpuinfo_has_x86_avx512f() && cpuinfo_has_x86_avx512vl() && cpuinfo_has_x86_avx512dq() &&
        cpuinfo_has_x86_avx512bw())
    {
        return ISPC_MAXIMUM_SUPPORTED_LANES; // avx512skx-x16
    }

    // 8 lanes (256-bit)
    if (cpuinfo_has_x86_avx2())
    {
        return ISPC_DEFAULT_SUPPORTED_LANES; // avx2-i32x8
    }

    if (cpuinfo_has_x86_avx())
    {
        return ISPC_DEFAULT_SUPPORTED_LANES; // avx1-i32x8
    }

    if (cpuinfo_has_x86_sse4_2())
    {
        return ISPC_DEFAULT_SUPPORTED_LANES; // sse4.2-i32x8 (software/double-pumped 8-lane SIMD
                                             // using 128-bit registers)
    }

    // 4 lanes (128-bit)
    if (cpuinfo_has_x86_avx())
    {
        return ISPC_MINIMUM_SUPPORTED_LANES; // avx1-i32x4
    }

#elif CPUINFO_ARCH_ARM || CPUINFO_ARCH_ARM64
    // 8 lanes (ARM NEON double-pumped / 8x32-bit execution)
    if (cpuinfo_has_arm_neon())
    {
        return ISPC_DEFAULT_SUPPORTED_LANES; // neon-i32x8
    }
#endif

    return ISPC_MINIMUM_SUPPORTED_LANES; // generic-i32x4
}
} // namespace SIMD_Helpers

namespace CPU_Generics
{
struct ParallelNoise3D_Options
{
    enki::TaskScheduler &TaskScheduler;
    uint8_t              SupportedSIMDLanes    = SIMD_Helpers::ISPC_DEFAULT_SUPPORTED_LANES;
    uint32_t             GridSizeOfASingleAxis = 0;
};

struct ParallelNoise3D_Context
{
    std::array<uint32_t, 3> StartFromIndex = {0, 0, 0};
    std::array<uint32_t, 3> WorkUntilIndex = {0, 0, 0};
};

inline void ParallelNoise3D(ParallelNoise3D_Options &Options, auto &&NoiseInvokerLambda)
{
    const uint8_t     SUB_CHUNK_SIZE_X = Options.SupportedSIMDLanes;
    constexpr uint8_t SUB_CHUNK_SIZE_Y =
        NoiseAlgorithms::SIMD_Helpers::ISPC_MAXIMUM_SUPPORTED_LANES;
    constexpr uint8_t SUB_CHUNK_SIZE_Z =
        NoiseAlgorithms::SIMD_Helpers::ISPC_MAXIMUM_SUPPORTED_LANES;

    auto GridSizeOfASingleAxis = Options.GridSizeOfASingleAxis;

    const uint32_t NumberOfChunksX = GridSizeOfASingleAxis / SUB_CHUNK_SIZE_X;
    const uint32_t NumberOfChunksY = GridSizeOfASingleAxis / SUB_CHUNK_SIZE_Y;
    const uint32_t NumberOfChunksZ = GridSizeOfASingleAxis / SUB_CHUNK_SIZE_Z;

    const uint32_t TotalChunks    = NumberOfChunksX * NumberOfChunksY * NumberOfChunksZ;
    const uint32_t ChunksPerSlice = NumberOfChunksX * NumberOfChunksY; // X major

    enki::TaskSet task(
        TotalChunks,
        [=](enki::TaskSetPartition Range, uint32_t ThreadNumber) -> void
        {
            for (uint32_t i = Range.start; i < Range.end; i++)
            {
                // find the current thread's XYZ coordinates
                uint32_t i_x = i % NumberOfChunksX;
                uint32_t i_y = (i / NumberOfChunksX) % NumberOfChunksY;
                uint32_t i_z = i / ChunksPerSlice;

                ParallelNoise3D_Context Context;

                Context.StartFromIndex = {
                    i_x * SUB_CHUNK_SIZE_X,
                    i_y * SUB_CHUNK_SIZE_Y,
                    i_z * SUB_CHUNK_SIZE_Z,
                };
                Context.WorkUntilIndex = {
                    Context.StartFromIndex[0] + SUB_CHUNK_SIZE_X,
                    Context.StartFromIndex[1] + SUB_CHUNK_SIZE_Y,
                    Context.StartFromIndex[2] + SUB_CHUNK_SIZE_Z,
                };

                NoiseInvokerLambda(Context);
            }
        });

    Options.TaskScheduler.AddTaskSetToPipe(&task);
    Options.TaskScheduler.WaitforTask(&task);
}
} // namespace CPU_Generics

namespace Simplex3D
{
// Parallel by SIMD intrinsics and multi-threading.
inline void CPU_ParallelSimplex3D(
    std::span<float>    &OutputBuffer,
    enki::TaskScheduler &TaskScheduler,
    const uint8_t        SupportedSIMDLanes,
    std::array<float, 3> Origin, // NOLINT
    uint32_t             GridSizeOfASingleAxis,
    uint32_t             Seed // NOLINT
)
{
    auto profiler_start = std::chrono::high_resolution_clock::now();
    using namespace CPU_Generics;
    ParallelNoise3D_Options options = {
        .TaskScheduler         = TaskScheduler,
        .SupportedSIMDLanes    = SupportedSIMDLanes,
        .GridSizeOfASingleAxis = GridSizeOfASingleAxis};

    ParallelNoise3D(
        options,
        [=](ParallelNoise3D_Context Context) -> void
        {
            ispc::Simplex3D(
                Origin.data(),
                Seed,
                GridSizeOfASingleAxis,
                Context.StartFromIndex.data(),
                Context.WorkUntilIndex.data(),
                OutputBuffer.data());
        });
    auto profiler_end = std::chrono::high_resolution_clock::now();
    auto time_taken_nanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(profiler_end - profiler_start);
    auto time_taken_microseconds =
        std::chrono::duration_cast<std::chrono::microseconds>(profiler_end - profiler_start);
    auto time_taken_milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(profiler_end - profiler_start);

    auto nanoseconds  = time_taken_nanoseconds.count();
    auto microseconds = time_taken_microseconds.count();
    auto milliseconds = time_taken_milliseconds.count();

    godot::UtilityFunctions::print(
        milliseconds,
        " milliseconds | ",
        microseconds,
        " microseconds | ",
        nanoseconds,
        " nanoseconds");
}
}; // namespace Simplex3D
}; // namespace NoiseAlgorithms
