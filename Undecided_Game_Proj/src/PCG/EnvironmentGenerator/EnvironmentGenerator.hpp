#ifndef ENVIRONMENTGENERATOR_H
#define ENVIRONMENTGENERATOR_H

#include <cstddef>
#include <cstdint>
#include <godot_cpp/classes/node3d.hpp>
#include "../../cache/ispc/Simplex3D.h"
#include "../../cache/Libraries/taskflow/taskflow.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/variant/utility_functions.hpp"
#include <vector>

namespace godot {

class EnvironmentGenerator : public Node3D {
    GDCLASS(EnvironmentGenerator, Node3D);

protected:
    static void _bind_methods();

private:
    struct VoxelGridParameters
    {
        uint32_t TotalResolution = 0;
        uint32_t ResolutionOfASingleAxis = 0;
    };

    struct GeneratorProperties {
        struct CPU {
            VoxelGridParameters VoxelGridParameters;
            std::vector<float> VoxelDensities;
            std::vector<int32_t> VoxelProperties;
        } CPU;
    };
public:
    EnvironmentGenerator()
    {

    };
    ~EnvironmentGenerator();

    GeneratorProperties HybridGeneratorProperties;



    inline void InitializeGenerator_CPU()
    {
        auto& cpu_properties = HybridGeneratorProperties.CPU;
        auto& voxel_grid_size = cpu_properties.VoxelGridParameters.TotalResolution;
        cpu_properties.VoxelDensities.resize(voxel_grid_size);
        cpu_properties.VoxelProperties.resize(voxel_grid_size);
    }

    inline void InitializeGenerator_GPU()
    {

    }

    void InitializeGenerator(int8_t CPU_or_GPU_or_Hybrid, uint32_t GridSizeOfASingleAxis_CPU, uint32_t GridSizeOfASingleAxis_GPU)
    {
        switch(CPU_or_GPU_or_Hybrid){
        case 0: // CPU
            HybridGeneratorProperties.CPU.VoxelGridParameters.TotalResolution = GridSizeOfASingleAxis_CPU * GridSizeOfASingleAxis_CPU * GridSizeOfASingleAxis_CPU;

            InitializeGenerator_CPU();
            break;
        case 1: // GPU
            InitializeGenerator_GPU();
            break;
        case 2: // Hybrid
            break;
        default:
            ERR_PRINT("Failed to initialize noise generator. Unkown generator arch.");
            break;
        }
    }
};

}

#endif
