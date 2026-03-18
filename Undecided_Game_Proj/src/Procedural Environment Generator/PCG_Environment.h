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
            struct ComputeUniformData 
            {
                Vector4i SCENE_PROPERTIES;

                Vector4 NOISE_PARAMS;
                
                Vector4i CHUNK_SIZE;

                Vector4i VOXELS_PER_CHUNK;

                Vector4i ENTITY_LOCATION;

                Vector4i ENTITY_LOCATION_P2;

                Vector4 PLANET_BOUNDS;
            };

            struct returnedVoxel
            {
                struct voxelData {
                    float matID;
                    float density;
                };

                voxelData VoxelData[];
            };

            struct uint32Vec3
            {
                uint32_t x;
                uint32_t y;
                uint32_t z;
            };

            struct SVO_Node
            {
                uint32_t ChildPointer;
                uint32_t ChildMask;

                uint32_t Data;
                uint32_t MortonCode;
            };
            struct SVO_NodeBuffer
            {
                SVO_Node SVO_Node[];
            };

            struct SVO_NodeBufferAux
            {
                SVO_Node SVO_NodeAux[];
            };

            struct SVO_NodeBufferAux2
            {
                SVO_Node SVO_NodeAux2[];
            };

            struct PushConstant // approx 40 bytes
            {
                uint32_t PassNum;
                uint32_t PassOffset;

                Vector4i ENTITY_LOCATION;
                Vector4i ENTITY_LOCATION_P2;
            };

            struct Histogram
            {
              uint32_t Buckets[6][16];
            };

            struct PSOffset
            {
                uint32_t Offsets[6][16];
            };

            struct PCGPipelines 
            {
                RID density;
                RID svo;
                RID dual_contour;
                RID prefixsum;
                RID histogram;
            };

            struct PCGStorage {
                RID uniform_set;
                RID voxel_storage;
                RID svo_storage;
                RID histogram_storage;
                RID uniform_buffer;
                RID atomic_counter;
                RID atomic_counter2;
                RID svo_aux;
                RID voxel_output;
                RID prefixsum_offset;
                RID histogram_buffer;

                // doesn't have to be limited to just RIDs; I'm too lazy to refactor everything into this

                int32_t WORKGROUP_SIZE_PLANET;
                int32_t WORKGROUP_SIZE_SVO;
                int32_t WORKGROUP_SIZE_DUAL_CONTOUR;
            };

            struct CompiledShaders
            {
                RID CompiledShader;
                RID CompiledShader_SVO;
                RID CompiledShader_DualContour;
                RID CompiledShader_Radix;
                RID CompiledShader_Histogram;
            };

            CompiledShaders compiled_shaders;
            PCGStorage storage;
            PCGPipelines pipelines;

            PushConstant BasicPushConstant;
            PackedByteArray pushconst_buffer;
        protected:
            static void _bind_methods();


        public:
            PCG_Environment();
            ~PCG_Environment();
            
            // reminder to 6-month-older self: abstract the params into a struct. thank you. -- 5 month older self: I did (mostly), no problem

            RID loadGDShader(String &path_to_compute_shader, String &CompileTo, const bool doCompilation, const uint64_t WORKGROUP_SIZE, const bool DEBUG);
                                                                                                                 
            void passParams_to_PCG(const bool isCPU_or_GPU,
                                    const uint8_t FOR_EACH_ENTITY,
                                    const PackedInt64Array CURRENT_ENTITY_LOCATION, 
                                    const PackedInt64Array CURRENT_PLANET_POSITION,
                                    const uint8_t &PASS_AMOUNT,
                                    const PackedInt32Array VOXELS_PER_CHUNK, 
                                    const PackedInt32Array CHUNK_SIZE,
                                    const bool DEBUG);

            void SVOPass(uint32Vec3 &VecObj, uint32Vec3 &CurrentDispatchDimension, int64_t &ComputeList, PackedInt32Array CHUNK_SIZE);

            void Active_Passive_Generation_Pass(uint32Vec3 &VecObj, PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                                int64_t &ComputeList, ComputeUniformData &UniformData, size_t &UDA_Size, RID UniformBuffer_RID,
                                                PackedByteArray UniformDataArray, const bool SKIP_SVO);

            void LoopGenerationForEntity(const uint8_t FOR_EACH_ENTITY, PackedInt64Array CURRENT_ENTITY_LOCATION, PackedInt64Array CURRENT_PLANET_LOCATION,
                                        const uint8_t &PASS_AMOUNT,
                                        uint32Vec3 &VecObj, PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                        int64_t &ComputeList);
            
            void RegisterLocalLocation(PackedInt64Array LocalEntityLocation, uint32_t &Stage);

            Ref<RDUniform> RefWrapper(int Binding, RID Buffer_RID, RenderingDevice::UniformType UniformType);

            void Density_Generation_Pass(uint32Vec3 &VecObj, PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                              int64_t &ComputeList);

            void SVO_Generation_Pass(uint32Vec3 &VecObj, PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                          int64_t &ComputeList);
                                          
            void DualContour_Generation_Pass(uint32Vec3 &VecObj, PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                             int64_t &ComputeList);
            
            void Histogram_pass(int64_t &ComputeList,
                                PackedInt32Array &VOXELS_PER_CHUNK, PackedInt32Array &CHUNK_SIZE);                                      
            void PrefixSum(int64_t &ComputeList,
                            PackedInt32Array &VOXELS_PER_CHUNK, PackedInt32Array &CHUNK_SIZE);

            void initCompute(const int32_t &SEED, const int32_t &MAXVERTs, const int32_t &IS_STARTINGSCENE,
                            const PackedInt32Array CHUNK_SIZE, const PackedInt32Array VOXELS_PER_CHUNK,
                            const PackedInt64Array CURRENT_ENTITY_LOCATION, const PackedInt64Array CURRENT_PLANET_POSITION,
                            const uint32_t &SVO_MAX_NODES_PER_CHUNK);
    }; 
}