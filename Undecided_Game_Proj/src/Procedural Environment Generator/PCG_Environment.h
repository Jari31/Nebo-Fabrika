#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rd_shader_file.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>

namespace godot {
    class String;

    class PCG_Environment : public Node3D {
        GDCLASS(PCG_Environment, Node3D)

        private:
            RenderingDevice* RenderingDevice = RenderingServer::get_singleton()->create_local_rendering_device();
            struct ComputeUniformData {
                uint32_t is_STARTING_SCENE;
                uint32_t seed;
                uint32_t MAXVERTs;
                uint32_t padding[2]; 
            };

            struct returnedVertex 
            {
                Vector3 Position;
                Vector3 Normals;
                uint16_t matID;
            };
            
        protected:
            static void _bind_methods();


        public:
            PCG_Environment();
            ~PCG_Environment();
            
            RID loadGDShader(String &path_to_compute_shader, String &CompileAndSaveTo_REQUIRED, const bool doCompilation, const bool DEBUG); // need to include the name for the shader in CompileAndSaveTo. eg. res://x/xx/nameOfShader.res 
                                                                                                                  // It's also required even if Compile_YN is set to off

            void passParams_to_PCG(RID &CompiledShader, bool isCPU_or_GPU, const String &EditFileLocation, const String SVO_VertexFileLocation, const bool IS_STARTINGSCENE, const uint32_t &SEED, const uint32_t MAXVERTs, const uint8_t (&CHUNK_SIZE)[3], const bool DEBUG); // 0 for CPU, 1 for GPU
    }; 
}