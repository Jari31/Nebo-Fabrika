#ifndef ENVIRONMENTGENERATOR_H
#define ENVIRONMENTGENERATOR_H

#include "Libraries/include/enkiTS/TaskScheduler.h"
#include "MathLibraries/Noise/NoiseAlgorithms.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/classes/rendering_device.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/godot.hpp"
#include "godot_cpp/variant/packed_float32_array.hpp"
#include "godot_cpp/variant/utility_functions.hpp"
#include "godot_cpp/variant/variant.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <godot_cpp/classes/node3d.hpp>
#include <memory>
#include <span>
#include <vector>

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

                auto CompiledSource = ShaderSlang::CompileSourceToSPIRV(
                    CompilerOptions,
                    std::array<char *, 60>{const_cast<char *>(
                        "F:/Openworld_Game/Undecided_Game_Proj/src/InternalLibraries")});

                String ByteString = "[";
                for (size_t i = 0; i < CompiledSource.size(); ++i)
                {
                    ByteString += String::num_int64(CompiledSource[i]);
                    if (i + 1 < CompiledSource.size())
                    {
                        ByteString += ", ";
                    }
                }
                ByteString += "]";

                UtilityFunctions::print(ByteString);
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
