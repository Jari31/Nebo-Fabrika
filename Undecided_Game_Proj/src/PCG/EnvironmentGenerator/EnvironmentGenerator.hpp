#ifndef ENVIRONMENTGENERATOR_H
#define ENVIRONMENTGENERATOR_H

#include "Libraries/include/cpuinfo.h"
#include "Libraries/include/enkiTS/TaskScheduler.h"
#include "ShaderCompiler/ShaderCompiler.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/classes/rendering_device.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/variant/packed_float32_array.hpp"
#include "godot_cpp/variant/utility_functions.hpp"
#include "godot_cpp/variant/variant.hpp"
#include "ispc/Simplex3D.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <godot_cpp/classes/node3d.hpp>
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

inline void ParallelNoise3D(ParallelNoise3D_Options &Options, auto NoiseInvokerLambda)
{
    auto start = std::chrono::high_resolution_clock::now();

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
                uint32_t i_x = i % NumberOfChunksX;
                uint32_t i_y = (i / NumberOfChunksX) % NumberOfChunksY;
                uint32_t i_z = i / ChunksPerSlice;

                ParallelNoise3D_Context Context;

                Context.StartFromIndex = {
                    i_x * SUB_CHUNK_SIZE_X,
                    i_y * SUB_CHUNK_SIZE_Y,
                    i_z * SUB_CHUNK_SIZE_Z,
                }; // thread local
                Context.WorkUntilIndex = {
                    Context.StartFromIndex.at(0) + SUB_CHUNK_SIZE_X,
                    Context.StartFromIndex.at(1) + SUB_CHUNK_SIZE_Y,
                    Context.StartFromIndex.at(2) + SUB_CHUNK_SIZE_Z,
                };

                NoiseInvokerLambda(Context);
            }
        });

    Options.TaskScheduler.AddTaskSetToPipe(&task);
    Options.TaskScheduler.WaitforTask(&task);
    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    godot::UtilityFunctions::print(
        "Time took to process the chunk: ", duration.count(), " microseconds");
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
    using namespace CPU_Generics;
    ParallelNoise3D_Options options = {
        .TaskScheduler         = TaskScheduler,
        .SupportedSIMDLanes    = SupportedSIMDLanes,
        .GridSizeOfASingleAxis = GridSizeOfASingleAxis};

    ParallelNoise3D(
        options,
        [=](ParallelNoise3D_Context Context_) -> void
        {
            ispc::Simplex3D(
                Origin.data(),
                Seed,
                GridSizeOfASingleAxis,
                Context_.StartFromIndex.data(),
                Context_.WorkUntilIndex.data(),
                OutputBuffer.data());
        });
}
}; // namespace Simplex3D
}; // namespace NoiseAlgorithms

namespace godot
{

class VoxelBuffer : public RefCounted
{
    GDCLASS(VoxelBuffer, RefCounted); // NOLINT

  protected:
    static void _bind_methods()
    {
        ClassDB::bind_method(D_METHOD("ResizeBuffer", "ToSize"), &VoxelBuffer::ResizeBuffer);
        ClassDB::bind_method(
            D_METHOD("ExtractSlice", "FromIndex", "ToIndex"), &VoxelBuffer::ExtractSlice);
        ClassDB::bind_method(D_METHOD("GetVoxelBufferSize"), &VoxelBuffer::GetVoxelBufferSize);
    };

  public:
    VoxelBuffer() {};  // NOLINT
    ~VoxelBuffer() {}; // NOLINT

    std::vector<float> Buffer;
    std::span<float>   Slice;

    void ResizeBuffer(uint32_t ToSize) { Buffer.resize(ToSize); }

    void ExtractSlice(size_t FromIndex, size_t ToIndex)
    {
        std::span<float> buffer_view(Buffer);
        Slice = buffer_view.subspan(FromIndex, ToIndex);
    }

    auto GetVoxelBufferSize() const -> size_t { return Buffer.size(); }; // NOLINT
};

class EnvironmentGenerator : public Node3D
{
    GDCLASS(EnvironmentGenerator, Node3D); // NOLINT

  protected:
    static void _bind_methods()
    {
        ClassDB::bind_method(
            D_METHOD(
                "CPU_ParallelSimplex3D", "Seed", "Origin", "GridSizeOfASingleAxis", "OutputSlice"),
            &EnvironmentGenerator::CPU_ParallelSimplex3D);

        ClassDB::bind_method(
            D_METHOD("InitializeGPUGenerator"), &EnvironmentGenerator::InitializeGPUGenerator);

        ClassDB::bind_method(
            D_METHOD("InitializeCPUGenerator", "GridSizeOfASingleAxis"),
            &EnvironmentGenerator::InitializeCPUGenerator);

        ClassDB::bind_method(D_METHOD("PrintCPUBuffers"), &EnvironmentGenerator::PrintCPUBuffers);

        ClassDB::bind_method(
            D_METHOD("GetCPUVoxelBuffer", "Density_or_Properties_Buffer"),
            &EnvironmentGenerator::GetCPUVoxelBuffer);
    };

  private:
  public:
    struct VoxelGridParameters
    {
        uint32_t TotalResolution         = 0;
        uint32_t ResolutionOfASingleAxis = 0;
    };

    struct Generators
    {
        struct CPU
        {
            std::unique_ptr<VoxelGridParameters> ObjectVoxelGridParameters;
            Ref<VoxelBuffer>                     VoxelDensities;
            Ref<VoxelBuffer>                     VoxelProperties;

            std::unique_ptr<enki::TaskScheduler> TaskScheduler;
            uint8_t                              SupportedSIMDLanes;

            void InitializeGenerator(uint32_t TotalResolution)
            {
                ObjectVoxelGridParameters = std::make_unique<VoxelGridParameters>();

                ObjectVoxelGridParameters->TotalResolution = TotalResolution;

                VoxelDensities.instantiate();
                VoxelProperties.instantiate();

                VoxelDensities->ResizeBuffer(ObjectVoxelGridParameters->TotalResolution);
                VoxelProperties->ResizeBuffer(ObjectVoxelGridParameters->TotalResolution);

                UtilityFunctions::print(VoxelDensities);

                TaskScheduler = std::make_unique<enki::TaskScheduler>();
                TaskScheduler->Initialize();

                SupportedSIMDLanes = NoiseAlgorithms::SIMD_Helpers::QuerySIMDLanes();
            }

            void DeInitializeGenerator() {}
        };

        struct GPU
        {
            RenderingDevice *ObjectRenderingDevice;
            RenderingServer *ObjectRenderingServer;

            void InitializeGenerator()
            {
                ObjectRenderingServer = RenderingServer::get_singleton();
                ObjectRenderingDevice = ObjectRenderingServer->get_rendering_device();

                namespace ShaderSlang = ShaderCompiler::ShaderSlang;

                constexpr char SlangSourceCode[] = // NOLINT
                    {
#embed "Shaders/TestShader.slang"
                    };

                ShaderSlang::CompileToSPIRV_Options CompilerOptions = {
                    .Source = SlangSourceCode, .EntryPointName = "main"};

                auto CompiledSource =
                    ShaderSlang::CompileSourceToSPIRV(CompilerOptions, std::array<char *, 0>{});

                String GodotCompatibleCompiledSource = "";

                for (uint8_t i : CompiledSource)
                {
                    GodotCompatibleCompiledSource += i;
                }

                UtilityFunctions::print(GodotCompatibleCompiledSource);
            }
        };

        std::unique_ptr<CPU> ObjectCPU;
        std::unique_ptr<GPU> ObjectGPU;
    };

    EnvironmentGenerator() {};
    ~EnvironmentGenerator() {};

    Generators HybridGenerator;

    void InitializeCPUGenerator(uint32_t GridSizeOfASingleAxis)
    {

        auto &ObjectCPU = HybridGenerator.ObjectCPU;

        if (ObjectCPU == nullptr)
        {
            UtilityFunctions::print("CPU generator not initialized. Initializing... ");
            ObjectCPU = std::make_unique<Generators::CPU>();
        }

        ObjectCPU->InitializeGenerator(
            GridSizeOfASingleAxis * GridSizeOfASingleAxis * GridSizeOfASingleAxis);
    }

    void PrintCPUBuffers()
    {
        auto &CPU                     = HybridGenerator.ObjectCPU;
        auto &voxel_density_buffer    = CPU->VoxelDensities->Buffer;
        auto &voxel_properties_buffer = CPU->VoxelProperties->Buffer;

        // auto _convert_vector_into_packed_array =
        //     []<typename Type>(std::vector<Type> &Vector, auto PackedArray)
        // {
        //     PackedArray.resize(Vector.size());
        //     std::copy(Vector.begin(), Vector.end(), PackedArray.ptrw());
        // };

        // PackedFloat32Array intermediate_voxel_buffer;
        // _convert_vector_into_packed_array(voxel_density_buffer, intermediate_voxel_buffer);

        // UtilityFunctions::print("Density buffer: ", intermediate_voxel_buffer);

        for (float i : voxel_density_buffer) // NOLINT
        {
            UtilityFunctions::print("Density buffer: ", i);
        }

        for (float i : voxel_properties_buffer) // NOLINT
        {
            UtilityFunctions::print("Properties buffer: ", i);
        }
    }

    auto GetCPUVoxelBuffer(uint8_t DensityOrPropertiesBuffer) -> Ref<VoxelBuffer>
    {
        auto &ObjectCPU = HybridGenerator.ObjectCPU;

        switch (DensityOrPropertiesBuffer)
        {
        default:
            UtilityFunctions::print(ObjectCPU->VoxelDensities);
            return ObjectCPU->VoxelDensities;
            break;
        case 1:
            return ObjectCPU->VoxelProperties;
            break;
        };
    }

    void CPU_ParallelSimplex3D(
        uint32_t     Seed,
        Vector3      Origin,
        uint32_t     GridSizeOfASingleAxis,
        VoxelBuffer *OutputSlice)
    {
        std::array<float, 3> origin_array  = {Origin.x, Origin.y, Origin.z};
        auto                &cpu_generator = HybridGenerator.ObjectCPU;

        NoiseAlgorithms::Simplex3D::CPU_ParallelSimplex3D(
            OutputSlice->Slice,
            *(cpu_generator->TaskScheduler),
            cpu_generator->SupportedSIMDLanes,
            origin_array,
            GridSizeOfASingleAxis,
            Seed);
    }

    void InitializeGPUGenerator()
    {
        auto &gpu_generator = HybridGenerator.ObjectGPU;
        if (gpu_generator == nullptr)
        {
            HybridGenerator.ObjectGPU = std::make_unique<Generators::GPU>();
        }
        gpu_generator->InitializeGenerator();
    }
};

} // namespace godot
#endif
