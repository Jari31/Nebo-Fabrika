#pragma once

/*
    COPYRIGHT (c) 2026 Jari
    Licensed under the MIT license. Refer to the license file provided within the README for details.
*/

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
#include <fstream>
#include <string>
#include <stdexcept>

struct Morton_LUT // Look Up Table
{
    uint32_t Interleave_Table[1024];
    uint32_t De_Interleave_Table[1024];
};

namespace godot {
    class String;

    class PCG_Environment : public Node3D {
        GDCLASS(PCG_Environment, Node3D)

        private:
            RenderingDevice* RenderingDevice_Local = nullptr;
            RenderingServer* RenderingServer_Local = nullptr;
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

            struct PushConstant 
            {
                uint32_t PassNum = 0;
                uint32_t PassOffset = 0;
                uint32_t PassStage = 0;

                uint32_t  Dense_SaveAsMortonCode = 0;
                uint32_t  Dense_TotalNodes = 1024;

                uint32_t SEED = 0;
                float    SVO_VoxelSize = 1.0;
                uint32_t SVO_BufferSize = 1024;

                uint32_t HashSize = 0;
                uint32_t pad1;
                uint32_t pad2;
                uint32_t pad3;
                //Vector3i ENTITY_LOCATION = Vector3i(0, 0, 0);

                //Vector3i ENTITY_LOCATION_P2 = Vector3i(0, 0, 0);

                Vector4i CHUNK_SIZE = Vector4i(4, 4, 4, 0);

                Vector4i VOXELS_PER_CHUNK = Vector4i(64, 64, 64, 0);
            };

            struct Histogram
            {
              uint32_t Buckets[6][16];
            };

            struct PSOffset
            {
                uint32_t Offsets[6][16];
            };

            struct DC_VertexBuffer
            {
                Vector3 Vertices[];
            };

            struct DC_NormalBuffer
            {
                Vector3 Normals[];
            };
            
            struct DC_UVBuffer
            {
                Vector2 UV[];
            };

            struct DC_IndexBuffer
            {
                uint32_t Indices[];
            };

            struct DC_VertexIndexBuffer
            {
                int32_t Node_VertexIndex[];
            };

            struct DC_EdgeMaskBuffer
            {
                uint32_t Node_EdgeMask[];
            };

            struct PCGPipelines 
            {
                RID density            = RID();
                RID svo                = RID();
                RID dual_contour_dense = RID();
                RID dual_contour_sparse= RID();
                RID prefixsum          = RID();
                RID histogram          = RID();
            };

            struct PCGStorage {
                RID uniform_set = RID();
                RID voxel_svo_storage_compiledShader = RID();
                RID svo_storage       = RID();
                RID histogram_storage = RID();
                RID dc_dense_storage[2]  = {RID(), RID()};
                RID dc_sparse_storage[2] = {RID(), RID()};

                RID atomic_counter  = RID();
                RID atomic_counter2 = RID();

                RID svo_aux      = RID();
                RID voxel_output = RID();

                RID prefixsum_offset = RID();

                RID histogram_buffer          = RID();
                RID morton_lookuptable_buffer = RID();
                RID uniform_buffer            = RID();

                RID dc_vertex_buffer       = RID();
                RID dc_normal_buffer       = RID();
                RID dc_UV_buffer           = RID();
                RID dc_index_buffer        = RID();
                RID dc_vertex_index_buffer = RID();
                RID dc_edge_mask_buffer    = RID();

                // doesn't have to be limited to just RIDs; I'm too lazy to refactor everything into this

                int32_t WORKGROUP_SIZE_PLANET = 8;
                int32_t WORKGROUP_SIZE_SVO = 8;
                int32_t WORKGROUP_SIZE_DUAL_CONTOUR = 8;
            };

            struct CompiledShaders
            {
                RID CompiledShader                   = RID();
                RID CompiledShader_SVO               = RID();
                RID CompiledShader_DualContour_Dense = RID();
                RID CompiledShader_DualContour_Sparse= RID();
                RID CompiledShader_Radix             = RID();
                RID CompiledShader_Histogram         = RID();
            };

            CompiledShaders compiled_shaders;
            PCGStorage storage;
            PCGPipelines pipelines;

            PushConstant BasicPushConstant;
            PackedByteArray pushconst_buffer;

            bool G_INITIALIZED = 0;
            bool G_SKIP_SVO = 0; // G = global
            bool G_SKIP_MORTON_CODE_LUT = 0;
            bool G_DEBUG = 0;
        protected:
            static void _bind_methods();


        public:
            PCG_Environment();
            ~PCG_Environment();
            
            // reminder to 6-month-older self: abstract the params into a struct. thank you. -- 5 month older self: I did (mostly), no problem
            // 5 months older guy lied. this is still not fully abstracted. someone abstract it please - I'm too lazy to do it 
            RID DEPRECATED_loadGDShader(const String &path_to_compute_shader, const String &CompileTo, const bool doCompilation, 
                                int MacroDefPos, const uint64_t WORKGROUP_SIZE, const bool DEBUG);
                                                                                                                 
            void passParams_to_PCG(const bool isCPU_or_GPU, const bool SYNC_CPU_TO_GPU,
                                    const uint8_t FOR_EACH_ENTITY,
                                    const PackedInt32Array CURRENT_ENTITY_LOCATION, 
                                    const PackedInt32Array CURRENT_PLANET_POSITION,
                                    const uint8_t &PASS_AMOUNT,
                                    const PackedInt32Array VOXELS_PER_CHUNK, 
                                    const PackedInt32Array CHUNK_SIZE);

            void SVOPass(uint32Vec3 &VecObj, uint32Vec3 &CurrentDispatchDimension, int64_t &ComputeList, PackedInt32Array CHUNK_SIZE);

            void Active_Passive_Generation_Pass(uint32Vec3 &VecObj, PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                                int64_t &ComputeList, ComputeUniformData &UniformData, size_t &UDA_Size, RID UniformBuffer_RID,
                                                PackedByteArray UniformDataArray, const bool SKIP_SVO);

            void LoopGenerationForEntity(const uint8_t FOR_EACH_ENTITY, PackedInt32Array CURRENT_ENTITY_LOCATION, PackedInt32Array CURRENT_PLANET_LOCATION,
                                        const uint8_t &PASS_AMOUNT,
                                        uint32Vec3 &VecObj, PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                        int64_t &ComputeList);
            
           //void RegisterLocalLocation(PackedInt32Array LocalEntityLocation, uint32_t &Stage);

            Ref<RDUniform> RefWrapper(int Binding, RID Buffer_RID, RenderingDevice::UniformType UniformType);

            void Density_Generation_Pass(PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                              int64_t &ComputeList);

            void SVO_Generation_Pass(uint32Vec3 &VecObj, PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                          int64_t &ComputeList);
                                          
            void SVO_DualContour_Generation_pass(PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                                 int64_t &ComputeList);                    

            void DualContour_Generation_Pass(PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                             int64_t &ComputeList);
            
            void Histogram_pass(int64_t &ComputeList,
                                PackedInt32Array &VOXELS_PER_CHUNK, PackedInt32Array &CHUNK_SIZE);                                      
            void PrefixSum(int64_t &ComputeList,
                            PackedInt32Array &VOXELS_PER_CHUNK, PackedInt32Array &CHUNK_SIZE);

            void initCompute(const uint32_t &SEED, const int32_t &MAXVERTs, const int32_t &IS_STARTINGSCENE,
                            const bool SKIP_SVO, const bool SKIP_MORTON_CODE_LUT,
                            const PackedStringArray MORTON_CODE_LUT_FileName,
                            const PackedInt32Array CHUNK_SIZE, const PackedInt32Array VOXELS_PER_CHUNK,
                            const PackedInt32Array CURRENT_ENTITY_LOCATION, const PackedInt32Array CURRENT_PLANET_POSITION,
                            const uint32_t &SVO_MAX_NODES_PER_CHUNK);

            void LoadLUT(const std::string &FileName, uint32_t *Buffer);

            void SetCompiledShaders(RID PlanetShader, RID SVOShader, RID DualContouring_Dense, RID DualContouring_Sparse, RID RadixSort_PrefixSum_Shader, RID RadixSortHistogram_Scatter_Shader);

            Variant GetRID();
            void SetSettings(bool initLocalRenderingServer, bool DEBUG, uint32_t DC_WORKGROUP_SIZE=8, uint32_t SVO_WORKGROUP_SIZE=8, uint32_t PLANET_GEN_WORKGROUP_SIZE=8);
            
    }; 
}