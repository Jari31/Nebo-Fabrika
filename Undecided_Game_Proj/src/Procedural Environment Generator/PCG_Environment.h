#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rd_shader_file.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

namespace godot {
    class String;

    class PCG_Environment : public Node3D {
        GDCLASS(PCG_Environment, Node3D);

        private:
            RenderingDevice* RenderDevice = RenderingServer::get_singleton()->get_rendering_device();
        protected:


        public:
            PCG_Environment();
            ~PCG_Environment();

            void _bind_methods();

            void passParams_to_PCG(String path_to_compute_shader);
    }; 
}