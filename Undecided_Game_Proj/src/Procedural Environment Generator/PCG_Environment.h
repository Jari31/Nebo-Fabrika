#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rd_shader_file.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>

#include <algorithm>
#include <cmath>

namespace godot {
    class String;

    class PCG_Environment : public Node3D {
        GDCLASS(PCG_Environment, Node3D)

        private:
            RenderingDevice* RenderingDevice = RenderingServer::get_singleton()->create_local_rendering_device();

            const float WORLD_SCALE = 1.0;
            struct ComputeUniformData {
                Vector4i SCENE_PROPERTIES;

                Vector4 NOISE_PARAMS;
                
                Vector4i CHUNK_SIZE;

                Vector4i VOXELS_PER_CHUNK;

                Vector4i ENTITY_LOCATION;
            };

            struct returnedVoxel
            {
                struct voxelData {
                    float matID;
                    float density;
                };

                voxelData VoxelData[];
            };

            struct uint32Vec {
                uint32_t x;
                uint32_t y;
                uint32_t z;
            };

            struct VertexBuffer {
                struct VBuffer{
                    Vector4 Position;
                    uint32_t MatID;
                    uint32_t padding[3];
                };
                VBuffer VertexBuffer[];
            };

            struct PushConstant {
                uint32_t PassNum;
                uint32_t padding[3];
            };

            struct AtomicCounters
            {
                uint32_t NextAvailableNodeIndex;
                uint32_t DC_AtomicCounter;
                uint32_t paddingp[2];
            };
            
            
        protected:
            static void _bind_methods();


        public:
            PCG_Environment();
            ~PCG_Environment();
            
            RID loadGDShader(String &path_to_compute_shader, String &CompileAndSaveTo_REQUIRED, const bool doCompilation, const bool DEBUG); // need to include the name for the shader in CompileAndSaveTo. eg. res://x/xx/nameOfShader.res 
                                                                                                                  // It's also required even if Compile_YN is set to off
            void passParams_to_PCG(RID CompiledShader, bool isCPU_or_GPU,
                                    const String &EditFileLocation, const String &SVO_VertexFileLocation,
                                    const bool IS_STARTINGSCENE, const uint32_t &SEED, const uint32_t MAXVERTs,
                                    const PackedByteArray CHUNK_SIZE, const PackedByteArray VOXELS_PER_CHUNK, const PackedByteArray LOCAL_CHUNK_SIZE,
                                    const uint32_t SVO_MAX_NODES_PER_CHUNK,   
                                    const PackedByteArray CURRENT_PLAYER_LOCATION, 
                                    const bool CLEAR_RIDs, const bool DEBUG);
            void SVOPass(uint32Vec &VecObj, uint32Vec &CurrentDispatchDimension, int64_t &ComputeList, PackedByteArray LOCAL_CHUNK_SIZE);
            
            
            Ref<RDUniform> RefWrapper(int Binding, RID Buffer_RID, RenderingDevice::UniformType UniformType);
    }; 
}