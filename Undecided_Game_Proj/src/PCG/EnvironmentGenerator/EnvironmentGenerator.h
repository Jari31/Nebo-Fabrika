#ifndef ENVIRONMENTGENERATOR_H
#define ENVIRONMENTGENERATOR_H

#include <cstdint>
#include <godot_cpp/classes/node3d.hpp>
#include "../../cache/ispc/TestCompile.h"

namespace godot {

class EnvironmentGenerator : public Node3D {
    GDCLASS(EnvironmentGenerator, Node3D);

protected:
    static void _bind_methods();

public:
    struct EnvironmentGeneratorFlags {

    };

    EnvironmentGenerator();
    ~EnvironmentGenerator();

    void GenerateTerrain(int8_t CPU_or_GPU_or_Hybrid);
};

}

#endif
