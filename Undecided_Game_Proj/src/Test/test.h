#ifndef TEST_H
#define TEST_H

#include <godot_cpp/classes/mesh_instance3d.hpp>

namespace godot {

    class GDTest : public MeshInstance3D {
        GDCLASS(GDTest, MeshInstance3D)

        private:

        protected:
        static void _bind_methods();

        public:
            GDTest();
            ~GDTest();

            void Print();
    };
}

#endif