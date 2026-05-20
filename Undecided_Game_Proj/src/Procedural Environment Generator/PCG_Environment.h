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
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/texture2drd.hpp>

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

            
            struct voxelData {
                float matID;
                float density;
                Vector2 normals_packed_oct;
            };
            struct returnedVoxel
            {
                voxelData VoxelData[];
            };

            struct uint32Vec3
            {
                uint32_t x;
                uint32_t y;
                uint32_t z;
            };

            struct PushConstant 
            {
                uint32_t PassNum = 0;
                uint32_t PassOffset = 0;
                uint32_t PassStage = 0;
                uint32_t WriteToTexturesInFirstPass = 1;

                uint32_t  Dense_TotalNodes = 16777216;
                uint32_t SEED = 0;
                float    VoxelSize = 1.0;
                uint32_t LOD_Index = 0;

                uint32_t HashSize = 0;
                uint32_t GridSize = 256;
                float IndexCoefficient = 3.8;
                uint32_t ThreadAllocationPerTriangle = 1;

                Vector4i CHUNK_SIZE = Vector4i(4, 4, 4, 256);
                Vector4i VOXELS_PER_CHUNK = Vector4i(64, 64, 64, 64);
                Vector4i VertexOffsetLoD = Vector4i(0, 0, 0, 0);
                
            };
            struct AtomicBuffer
            {
                uint32_t AtomicCounter1 = 0;
                uint32_t AtomicCounter2 = 0;

                uint32_t VertexCounter  = 0;
            };
            struct DC_VertexBuffer
            {
                Vector4 Vertices[];
            };

            struct DC_NormalBuffer
            {
                Vector4 Normals[];
            };
            
            struct DC_UVBuffer
            {
                Vector2 UV[];
            };

            struct Triangle
            {
                Vector4 OriginNormal;

                uint32_t VIndex[4];
                float EdgeBudget[4];
            };
            struct DC_TriangleBuffer
            {
                Triangle Indices[];
            };

            struct DC_VertexIndexBuffer
            {
                int32_t Node_VertexIndex[];
            };

            struct DC_EdgeMaskBuffer
            {
                uint32_t Node_EdgeMask[];
            };

            struct DC_VertexOffsetBuffer
            {
                uint32_t VertexOffset[];
            };

            struct DC_Indirect_Dispatch_Buffer
            {
                uint32_t x;
                uint32_t y;
                uint32_t z;
                uint32_t TriangleCount;
            };

            struct DC_Params
            {
                uint32_t VerticesPerThread = 16;
                uint32_t VertexAllocationForEdges = 4;
                uint32_t TrianglesProcessedPerThread = 1;
                float VertexEdgeSnapThreshold = 0.9;
            };

            struct PCGPipelines 
            {
                RID density            = RID();
                RID dual_contour_dense = RID();
            };

            struct PCGStorage {
                RID uniform_set = RID();
                RID voxel_storage = RID();
                RID dc_dense_storage[2]  = {RID(), RID()};

                RID atomic_counter  = RID();
                RID atomic_counter2 = RID();

                RID voxel_output = RID();

                RID uniform_buffer            = RID();

                RID dc_vertex_buffer       = RID();
                RID dc_normal_buffer       = RID();
                RID dc_UV_buffer           = RID();
                RID dc_triangle_buffer        = RID();
                RID dc_vertex_index_buffer = RID();
                RID dc_edge_mask_buffer    = RID();
                RID dc_vertex_offset_buffer= RID();

                RID dc_vertex_texture      = RID();
                RID dc_normal_texture      = RID();
                RID dc_uv_texture          = RID();
                RID dc_index_texture       = RID();

                RID dc_indirect_dispatch_list_buffer = RID();
                RID dc_indirect_dispatch_list_buffer_b = RID();

                RID dc_uniform_parameter_buffer = RID();

                RID rendering_server_vertex_texture = RID();
                RID rendering_server_index_texture  = RID();
                RID rendering_server_normal_texture = RID();

                // doesn't have to be limited to just RIDs; I'm too lazy to refactor everything into this

                int32_t WORKGROUP_SIZE_PLANET = 8;
                int32_t WORKGROUP_SIZE_SVO = 8;
                int32_t WORKGROUP_SIZE_DUAL_CONTOUR = 8;
            };

            struct CompiledShaders
            {
                RID CompiledShader                   = RID();
                RID CompiledShader_DualContour_Dense = RID();

                Ref<Shader> CompiledShader_VertexPull= RID();
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
            uint32_t G_GRID_SIZE = 0;

            RID InstanceRID  = RID();
            RID Mesh_RID     = RID();
            RID Material_RID = RID();
            RID Scenario     = RID();
        protected:
            static void _bind_methods();


        public:
            PCG_Environment();
            ~PCG_Environment();
            
            void passParams_to_PCG(const bool isCPU_or_GPU, const bool SYNC_CPU_TO_GPU,
                                    const PackedInt32Array VOXELS_PER_CHUNK, 
                                    const PackedInt32Array CHUNK_SIZE,
                                    const PackedInt32Array VERTEX_LOCATION_OFFSET,
                                    const uint32_t LEVEL_OF_DETAIL);

            static Ref<RDUniform> RefWrapper(int Binding, RID Buffer_RID, RenderingDevice::UniformType UniformType);

            void Density_Generation_Pass(int64_t &ComputeList);        

            void DualContour_Generation_Pass(int64_t &ComputeList);

            void initCompute(const uint32_t &SEED, const int32_t &MAXVERTs,
                            const bool SKIP_PRE_INIT_TEXTURES,
                            const PackedInt32Array CHUNK_SIZE, const PackedInt32Array VOXELS_PER_CHUNK,
                            RID MeshInstance, const float INDEX_COEFFICIENT,
                            PackedInt32Array VertexLoDOffsets, const int32_t &RefMeshVertexCount);

            void SetCompiledShaders(RID PlanetShader, RID DualContouring_Dense, Ref<Shader> VertexPull_Shader);

            Variant GetLocalRenderingDeviceRID();
            void SetSettings(bool initLocalRenderingServer, bool UseLocalRenderingDevice, bool DEBUG, uint32_t DC_WORKGROUP_SIZE=8, uint32_t SVO_WORKGROUP_SIZE=8, uint32_t PLANET_GEN_WORKGROUP_SIZE=8);
            void SetRIDStorage(RID VertexTexture, RID NormalTexture, RID UVTexture, RID IndexTexture);
            void PrintGeneratedData();
    }; 
}