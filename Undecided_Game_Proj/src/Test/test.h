#pragma once

#include <godot_cpp/classes/mesh_instance3d.hpp>

namespace godot {
    class String;
    class GDTest : public MeshInstance3D {
        GDCLASS(GDTest, MeshInstance3D)

        private:
        int i = 0;

        protected:
        static void _bind_methods();

        public:
            GDTest();
            ~GDTest();

            // void _process(double delta) override;

            void CPP_Print(const String &printable_string);
    };
}