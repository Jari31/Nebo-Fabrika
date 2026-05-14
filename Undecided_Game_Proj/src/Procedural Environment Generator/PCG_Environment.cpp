/*
    COPYRIGHT (c) 2026 Jari
*/

//! compile with: scons --target=PCG_Environment --targetFolder='Procedural Environment Generator' --productionBuild=0
//todo: Implement documentation system
#include "PCG_Environment.h"

using namespace godot;
//#define CMP_SHADER_DEBUG
#ifndef PRODUCTION_BUILD

#define CHECK_RENDERING_DEVICE()\
    {\
        if(RenderingDevice_Local == nullptr)\
        {\
            ERR_PRINT("THE RENDERING DEVICE HAS NOT BEEN INITIALIZED. INITIALIZE IT FIRST USING SetSettings(true...). GOD SPEED.");\
            return;\
        }\
    }

#define ERR_PRINT_LINE()\
    {\
        ERR_PRINT(UtilityFunctions::str(__LINE__));\
    }

#define COMPUTE_LIST_CHECK()\
    {\
        if(G_DEBUG && ComputeList != NULL)\
            UtilityFunctions::print("Compute list initialized. ", ComputeList);\
        else if(G_DEBUG){\
            ERR_PRINT(UtilityFunctions::str("'COMPUTE LIST HAS NOT BEEN INITIALIZED. MAY GOD HAVE MERCY. I'M BAILING'", \
                        "-CPU,", __DATE__, ". Oh yeah, the error is at: ", __LINE__, "|", __FILE__));\
            return;\
        }\
    }

#endif

#define SAFE_FREE_RID(device, rid) \
    if (rid.is_valid()) { \
        device->free_rid(rid); \
        rid = RID(); \
    }

void PCG_Environment::_bind_methods() {
    // PCG Parameter Passing
    ClassDB::bind_method(D_METHOD("PassParamsToPCG", 
        "isCPU_or_GPU", "SYNC_CPU_TO_GPU", 
        "VOXELS_PER_CHUNK", "CHUNK_SIZE", 
        "VERTEX_LOCATION_OFFSET", 
        "LEVEL_OF_DETAIL"), &PCG_Environment::passParams_to_PCG);

    ClassDB::bind_method(D_METHOD("initCompute", 
        "SEED", "MAXVERTs",
        "SKIP_PRE_INIT_TEXTURES",
        "CHUNK_SIZE", "VOXELS_PER_CHUNK", 
        "MeshInstance", "INDEX_COEFFICIENT",
        "VertexLoDOffsets", "RefMeshVertexCount"), 
        &PCG_Environment::initCompute);
        
    ClassDB::bind_method(D_METHOD("GetLocalRenderingDeviceRID"), &PCG_Environment::GetLocalRenderingDeviceRID);

    ClassDB::bind_method(
        D_METHOD("SetCompiledShaders", 
            "planet_shader", 
            "dc_dense",
            "vertex_pull"), 
        &PCG_Environment::SetCompiledShaders
    );

    ClassDB::bind_method(D_METHOD("SetSettings", 
                                "initLocalRenderingServer", 
                                "UseLocalRenderingDevice",
                                "DEBUG",
                                "DC_WORKGROUP_SIZE",
                                "SVO_WORKGROUP_SIZE",
                                "PLANET_GEN_WORKGROUP_SIZE"),
                                &PCG_Environment::SetSettings,
                                DEFVAL(8), DEFVAL(8), DEFVAL(8));
    ClassDB::bind_method(D_METHOD("PrintGeneratedData"), &PCG_Environment::PrintGeneratedData);

    ClassDB::bind_method(D_METHOD("SetRIDStorage", "VertexTexture", "NormalTexture", "UVTexture", "IndexTexture"), &PCG_Environment::SetRIDStorage);
}

PCG_Environment::PCG_Environment(){

}

PCG_Environment::~PCG_Environment(){
    if(RenderingDevice_Local != nullptr)
        memdelete(RenderingDevice_Local);

// --- Shaders ---
    SAFE_FREE_RID(RenderingDevice_Local, compiled_shaders.CompiledShader);
    SAFE_FREE_RID(RenderingDevice_Local, compiled_shaders.CompiledShader_DualContour_Dense);
    // --- Pipelines ---
    SAFE_FREE_RID(RenderingDevice_Local, pipelines.density);
    SAFE_FREE_RID(RenderingDevice_Local, pipelines.dual_contour_dense);

    // --- Storage ---
    SAFE_FREE_RID(RenderingDevice_Local, storage.voxel_output);
    SAFE_FREE_RID(RenderingDevice_Local, storage.uniform_buffer);
    SAFE_FREE_RID(RenderingDevice_Local, storage.uniform_set);
    SAFE_FREE_RID(RenderingDevice_Local, storage.atomic_counter);
    SAFE_FREE_RID(RenderingDevice_Local, storage.atomic_counter2);
    SAFE_FREE_RID(RenderingDevice_Local, storage.voxel_storage);

    // --- Dual Contour Buffers ---
    SAFE_FREE_RID(RenderingDevice_Local, storage.dc_edge_mask_buffer);
    SAFE_FREE_RID(RenderingDevice_Local, storage.dc_normal_buffer);
    SAFE_FREE_RID(RenderingDevice_Local, storage.dc_UV_buffer);
    SAFE_FREE_RID(RenderingDevice_Local, storage.dc_triangle_buffer);
    SAFE_FREE_RID(RenderingDevice_Local, storage.dc_vertex_buffer);
    SAFE_FREE_RID(RenderingDevice_Local, storage.dc_vertex_index_buffer);

    SAFE_FREE_RID(RenderingDevice_Local, storage.dc_vertex_texture);
    SAFE_FREE_RID(RenderingDevice_Local, storage.dc_index_texture);
    SAFE_FREE_RID(RenderingDevice_Local, storage.dc_normal_texture);
    SAFE_FREE_RID(RenderingDevice_Local, storage.dc_uv_texture);

    SAFE_FREE_RID(RenderingServer_Local, storage.rendering_server_vertex_texture);
    SAFE_FREE_RID(RenderingServer_Local, storage.rendering_server_index_texture );

    // --- Arrays ---
    for(int i = 0; i < 2; i++) {
        SAFE_FREE_RID(RenderingDevice_Local, storage.dc_dense_storage[i]);
    }

    SAFE_FREE_RID(RenderingDevice_Local, InstanceRID);
    SAFE_FREE_RID(RenderingDevice_Local, Mesh_RID);
    SAFE_FREE_RID(RenderingDevice_Local, Material_RID);
    SAFE_FREE_RID(RenderingDevice_Local, Scenario);
}

Variant PCG_Environment::GetLocalRenderingDeviceRID()
{
    return RenderingDevice_Local;
}

void PCG_Environment::SetRIDStorage(RID VertexTexture, RID NormalTexture, RID UVTexture, RID IndexTexture)
{
    storage.dc_vertex_texture = VertexTexture;
    storage.dc_index_texture = IndexTexture;
    storage.dc_uv_texture = UVTexture;
    storage.dc_normal_texture = NormalTexture;
}

void PCG_Environment::PrintGeneratedData()
{
    PackedByteArray Data = RenderingDevice_Local->texture_get_data(storage.dc_vertex_texture, 0);
    const Vector4* DataPtr = reinterpret_cast<const Vector4*>(Data.ptr());
    for(int i = 3; i < 6 + 6; i++)
        UtilityFunctions::print("Vertex found: ", i, " | ", DataPtr[i]);
    
    PackedByteArray counterData = RenderingDevice_Local->buffer_get_data(storage.atomic_counter);
    const uint32_t* counterPtr = reinterpret_cast<const uint32_t*>(counterData.ptr());
    for(int i = 0; i < 3; i++)
        UtilityFunctions::print("Counter data: ", counterPtr[i]);

    counterData = RenderingDevice_Local->buffer_get_data(storage.dc_indirect_dispatch_list_buffer);
    counterPtr = reinterpret_cast<const uint32_t*>(counterData.ptr());

    UtilityFunctions::print("Dispatch X: ",  counterPtr[0]);
    UtilityFunctions::print("Dispatch Y: ",  counterPtr[1]);
    UtilityFunctions::print("Dispatch Z: ",  counterPtr[2]);

    PackedByteArray IndexData = RenderingDevice_Local->texture_get_data(storage.dc_index_texture, 0);
    const float* IndexPtr = reinterpret_cast<const float*>(IndexData.ptr());
    if(IndexData.is_empty())
        UtilityFunctions::print("Index data is empty.");
    else
        for(int i = 23; i < 26; i++)
            UtilityFunctions::print("Indice data", "[", i, "]",":", IndexPtr[i]);
    /*
    PackedByteArray VIData = RenderingDevice_Local->buffer_get_data(storage.dc_vertex_index_buffer, 0);
    const int* VIndexPtr = reinterpret_cast<const int*>(VIData.ptr());
    for(int i = 0; i < 20; i++)
        UtilityFunctions::print("Vertex index data: ", VIndexPtr[i]);
    */
}

void PCG_Environment::SetSettings(bool initLocalRenderingServer, bool UseLocalRenderingDevice, bool DEBUG, 
                                  uint32_t DC_WORKGROUP_SIZE, uint32_t SVO_WORKGROUP_SIZE, uint32_t PLANET_GEN_WORKGROUP_SIZE)
{
    if(initLocalRenderingServer){
        RenderingServer_Local = RenderingServer::get_singleton();
        if(!UseLocalRenderingDevice)
            RenderingDevice_Local = RenderingServer_Local->get_rendering_device();
        else
            RenderingDevice_Local = RenderingServer::get_singleton()->create_local_rendering_device();
    }
    G_DEBUG = DEBUG;

    storage.WORKGROUP_SIZE_DUAL_CONTOUR = DC_WORKGROUP_SIZE;
    storage.WORKGROUP_SIZE_SVO = SVO_WORKGROUP_SIZE;
    storage.WORKGROUP_SIZE_PLANET = PLANET_GEN_WORKGROUP_SIZE;
}

void PCG_Environment::SetCompiledShaders(RID PlanetShader, RID DualContouring_Dense, Ref<Shader> VertexPull_Shader)
{
    compiled_shaders.CompiledShader = PlanetShader;
    #ifdef CMP_SHADER_DEBUG
        WARN_PRINT(UtilityFunctions::str(compiled_shaders.CompiledShader));
    #endif
    compiled_shaders.CompiledShader_DualContour_Dense = DualContouring_Dense;
    #ifdef CMP_SHADER_DEBUG
        WARN_PRINT(UtilityFunctions::str(compiled_shaders.CompiledShader_DualContour_Dense));
    #endif
    compiled_shaders.CompiledShader_VertexPull = VertexPull_Shader;
    #ifdef CMP_SHADER_DEBUG
        WARN_PRINT(UtilityFunctions::str(compiled_shaders.CompiledShader_VertexPull));
    #endif
}


Ref<RDUniform> PCG_Environment::RefWrapper(int Binding, RID Buffer_RID, RenderingDevice::UniformType UniformType){
    Ref<RDUniform> Uniform_Ref;
    Uniform_Ref.instantiate();
    Uniform_Ref->set_uniform_type(UniformType);
    Uniform_Ref->set_binding(Binding);
    Uniform_Ref->add_id(Buffer_RID);
    return Uniform_Ref;
}

void SetVector4i(Vector4i &Vector, PackedInt32Array InVector){
    Vector.x = InVector[0];
    Vector.y = InVector[1];
    Vector.z = InVector[2];
    if(InVector.size() > 3)
        if(InVector[3])
            Vector.w = InVector[3];
};


void PCG_Environment::Density_Generation_Pass(int64_t &ComputeList)
{
    RenderingDevice_Local->compute_list_bind_compute_pipeline(ComputeList, pipelines.density);
    memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
    RenderingDevice_Local->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());
    //RenderingDevice_Local->compute_list_bind_uniform_set(ComputeList, storage.uniform_set, 0);
    RenderingDevice_Local->compute_list_bind_uniform_set(ComputeList, storage.voxel_storage, 0);
    
    size_t DispatchSize = G_GRID_SIZE / storage.WORKGROUP_SIZE_PLANET;
    RenderingDevice_Local->compute_list_dispatch(ComputeList, DispatchSize, DispatchSize, DispatchSize);
    #ifndef PRODUCTION_BUILD
    if(G_DEBUG)
        UtilityFunctions::print("Density pass computed.", DispatchSize);
    #endif

    RenderingDevice_Local->compute_list_add_barrier(ComputeList);
}

void PCG_Environment::DualContour_Generation_Pass(int64_t &ComputeList)
{
    RenderingDevice_Local->compute_list_bind_compute_pipeline(ComputeList, pipelines.dual_contour_dense);

    BasicPushConstant.PassNum = 0;
    BasicPushConstant.PassOffset = 0;

    RenderingDevice_Local->compute_list_bind_uniform_set(ComputeList, storage.dc_dense_storage[0], 0);
    RenderingDevice_Local->compute_list_bind_uniform_set(ComputeList, storage.dc_dense_storage[1], 1);

    uint32_t DispatchSize = ceil(G_GRID_SIZE / storage.WORKGROUP_SIZE_DUAL_CONTOUR); 

    auto p_const_copy = [&]()
    {
        memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
        RenderingDevice_Local->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());
    };

    auto pass = [&]()
    {
        p_const_copy();

        RenderingDevice_Local->compute_list_dispatch(ComputeList, DispatchSize, DispatchSize, DispatchSize);
        RenderingDevice_Local->compute_list_add_barrier(ComputeList);
    };

    pass();

    BasicPushConstant.PassOffset = 1;
    int Iterations = 0;
    if(Iterations != 0){
        for(int i = 1; i < Iterations; i++) // smoothing iterations
        {
            if(BasicPushConstant.PassOffset > 2)
            {
                BasicPushConstant.PassOffset = 1;
            }

            pass();

        
            BasicPushConstant.PassOffset++;
        }

        if(BasicPushConstant.PassOffset == 1)
        {
            BasicPushConstant.PassOffset = 3; // transfer over the buffers to the visual one
            pass();
        }
    }
    BasicPushConstant.PassOffset = 2147483647; // INT32_MAX. It serves as a special condition for the index generation pass. 
                                                 // If the pass offset is this specific value, the index generation runs. Otherwise, it does not.
    pass(); // triangle generation pass
    #ifndef PRODUCTION_BUILD
    if(G_DEBUG)
        UtilityFunctions::print("DC Finished.", DispatchSize);
    #endif

    if(!BasicPushConstant.WriteToTexturesInFirstPass) // dual contouring from implicit mesh
    {
        BasicPushConstant.PassOffset = 4; // looks at how many triangles there are and then indirectly dispatches a proportional amount of threads
        p_const_copy();
        
        RenderingDevice_Local->compute_list_dispatch(ComputeList, 1, 1, 1);
        RenderingDevice_Local->compute_list_add_barrier(ComputeList);
        /*
        BasicPushConstant.PassOffset = 666;
        p_const_copy();
        RenderingDevice_Local->compute_list_dispatch(ComputeList, 1, 1, 1);
        RenderingDevice_Local->compute_list_add_barrier(ComputeList);
        */
        BasicPushConstant.PassOffset = 5;
        BasicPushConstant.WriteToTexturesInFirstPass = 1;
        p_const_copy();

        memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
        RenderingDevice_Local->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

        //RenderingDevice_Local->compute_list_dispatch_indirect(ComputeList, storage.dc_indirect_dispatch_list_buffer, 0);
        RenderingDevice_Local->compute_list_dispatch(ComputeList, 1, 1, 1);
        RenderingDevice_Local->compute_list_add_barrier(ComputeList);

        BasicPushConstant.WriteToTexturesInFirstPass = 0;
        p_const_copy();
    }
}

// i love writing boilerplate i love writing boilerplate i love writing boilerplate i love writing boilerplate i love writing boilerplate
void PCG_Environment::initCompute(const uint32_t &SEED, const int32_t &MAXVERTs,
                                const bool SKIP_PRE_INIT_TEXTURES,
                                const PackedInt32Array CHUNK_SIZE, const PackedInt32Array VOXELS_PER_CHUNK,
                                RID MeshInstance, const float INDEX_COEFFICIENT,
                                PackedInt32Array VertexLoDOffsets, const int32_t &RefMeshVertexCount)
{
    pipelines.density            = RenderingDevice_Local->compute_pipeline_create(compiled_shaders.CompiledShader);
    pipelines.dual_contour_dense = RenderingDevice_Local->compute_pipeline_create(compiled_shaders.CompiledShader_DualContour_Dense);

    int TRUTH_GRID_SIZE = 64 * 4;

    SetVector4i(BasicPushConstant.CHUNK_SIZE,       CHUNK_SIZE      );
    SetVector4i(BasicPushConstant.VOXELS_PER_CHUNK, VOXELS_PER_CHUNK);

    const int SQRT_MAX_VERTS = ceil(sqrt(MAXVERTs));
    const int IDX_SQRT_MAX_VERTS = int(ceil(sqrt(MAXVERTs * INDEX_COEFFICIENT)));
    BasicPushConstant.CHUNK_SIZE.w = SQRT_MAX_VERTS;
    BasicPushConstant.VOXELS_PER_CHUNK.w = IDX_SQRT_MAX_VERTS;
    
    BasicPushConstant.IndexCoefficient = INDEX_COEFFICIENT;

    BasicPushConstant.GridSize = BasicPushConstant.CHUNK_SIZE.x * BasicPushConstant.VOXELS_PER_CHUNK.x;
    G_GRID_SIZE = BasicPushConstant.GridSize;
    BasicPushConstant.VoxelSize = TRUTH_GRID_SIZE / G_GRID_SIZE;

    #ifndef PRODUCTION_BUILD
    if(G_DEBUG)
        UtilityFunctions::print("Chunk size: ", BasicPushConstant.CHUNK_SIZE, "\n", 
                                "Voxels per chunk: ", BasicPushConstant.VOXELS_PER_CHUNK, "\n",
                                "Grid size: ", BasicPushConstant.GridSize, "\n",
                                "Voxel size: ", BasicPushConstant.VoxelSize);
    #endif

    BasicPushConstant.SEED = SEED;

    G_INITIALIZED          = true;

    pushconst_buffer.resize(sizeof(PushConstant));

    int BufferPadding = 0;

    {
    BasicPushConstant.Dense_TotalNodes = (CHUNK_SIZE[0]) * (CHUNK_SIZE[1]) * (CHUNK_SIZE[2] ) * 
                                         (VOXELS_PER_CHUNK[0]) * (VOXELS_PER_CHUNK[1]) * (VOXELS_PER_CHUNK[2]);
    #ifndef PRODUCTION_BUILD
        if(G_DEBUG)
        UtilityFunctions::print("Voxel grid's total nodes per axis: ", BasicPushConstant.Dense_TotalNodes);
    #endif
    returnedVoxel VoxelBuffer;
    PackedByteArray VoxelBufferArray;
    int64_t OutputSize = sizeof(voxelData) * (CHUNK_SIZE[0] + BufferPadding) * (CHUNK_SIZE[1] + BufferPadding) * (CHUNK_SIZE[2] + BufferPadding) 
                         * (VOXELS_PER_CHUNK[0] + BufferPadding) * (VOXELS_PER_CHUNK[1] + BufferPadding) * (VOXELS_PER_CHUNK[2] + BufferPadding);
    
    #ifndef PRODUCTION_BUILD
    if(G_DEBUG)
        UtilityFunctions::print("Voxel grid size: ", OutputSize);
    #endif
    
    VoxelBufferArray.resize(OutputSize);
    storage.voxel_output = RenderingDevice_Local->storage_buffer_create(VoxelBufferArray.size(), VoxelBufferArray);
    }

        
    auto create_storage_buffer = [&](auto& buffer_struct, int64_t count, float multiplier = 1.0f) {
        using BufferType = std::remove_reference_t<decltype(buffer_struct)>;
        
        PackedByteArray byteArray;
        int64_t size = static_cast<int64_t>(ceil(sizeof(BufferType) * count * multiplier));
        byteArray.resize(size);

        return RenderingDevice_Local->storage_buffer_create(byteArray.size(), byteArray);
    };

    {
    AtomicBuffer AtomicCounter;
    storage.atomic_counter = create_storage_buffer(AtomicCounter, 1);
    }

    {

    {
    DC_VertexIndexBuffer VIndexBuffer;
    PackedByteArray VIndexBufferArray;
    int64_t BufferSize = int64_t(sizeof(DC_VertexIndexBuffer) * std::pow((G_GRID_SIZE), 3));
    VIndexBufferArray.resize(BufferSize);

    #ifndef PRODUCTION_BUILD
    if(G_DEBUG)
        UtilityFunctions::print("VIndex buffer set to size (bytes): ", sizeof(DC_VertexIndexBuffer) * std::pow((G_GRID_SIZE), 3));
    #endif

    storage.dc_vertex_index_buffer = RenderingDevice_Local->storage_buffer_create(VIndexBufferArray.size(), VIndexBufferArray);
    }

    {
    DC_EdgeMaskBuffer EdgeMaskBuffer;
    storage.dc_edge_mask_buffer = create_storage_buffer(EdgeMaskBuffer, std::pow((G_GRID_SIZE), 3));
    }

    {
    DC_VertexOffsetBuffer VOffsetBuf;
    storage.dc_vertex_offset_buffer = create_storage_buffer(VOffsetBuf, sizeof(VertexLoDOffsets));
    }

    {
    DC_VertexBuffer VBuffer;
    storage.dc_vertex_buffer = create_storage_buffer(VBuffer, RefMeshVertexCount);
    }

    {
    DC_NormalBuffer NBuffer;
    storage.dc_normal_buffer = create_storage_buffer(NBuffer, RefMeshVertexCount);
    }

    {
    Triangle TBuffer;
    storage.dc_triangle_buffer = create_storage_buffer(TBuffer, RefMeshVertexCount, INDEX_COEFFICIENT);
    }

    {
    DC_Indirect_Dispatch_Buffer DIDBuffer;

    DIDBuffer.x = 1;
    DIDBuffer.y = 1;
    DIDBuffer.z = 1;
        
    PackedByteArray ByteArray;
    int64_t Size = sizeof(DC_Indirect_Dispatch_Buffer);
    ByteArray.resize(Size);

    uint8_t* dest = ByteArray.ptrw();
    memcpy(dest, &DIDBuffer, sizeof(DC_Indirect_Dispatch_Buffer));

    storage.dc_indirect_dispatch_list_buffer = RenderingDevice_Local->storage_buffer_create(ByteArray.size(), ByteArray, RenderingDevice::STORAGE_BUFFER_USAGE_DISPATCH_INDIRECT);
    }
    }

    // texture because godot spatial shaders don't support buffers and i don't want to go and write my own damn rendering pipeline
    if(!SKIP_PRE_INIT_TEXTURES){
    Ref<RDTextureFormat> TextureFormat;
    TextureFormat.instantiate();
    TextureFormat->set_format(RenderingDevice::DATA_FORMAT_R32G32B32A32_SFLOAT);
    
    TextureFormat->set_width(SQRT_MAX_VERTS);
    TextureFormat->set_height(SQRT_MAX_VERTS);
    TextureFormat->set_usage_bits(RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_STORAGE_BIT | RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT
                                    | RenderingDevice::TEXTURE_USAGE_CAN_COPY_TO_BIT);

    Ref<RDTextureView> TextureView;
    TextureView.instantiate();

    storage.dc_vertex_texture = RenderingDevice_Local->texture_create(TextureFormat, TextureView);
    
    storage.dc_normal_texture = RenderingDevice_Local->texture_create(TextureFormat, TextureView);
    
    TextureFormat->set_format(RenderingDevice::DATA_FORMAT_R32G32_SFLOAT);

    storage.dc_uv_texture     = RenderingDevice_Local->texture_create(TextureFormat, TextureView);
    
    TextureFormat->set_format(RenderingDevice::DATA_FORMAT_R32_SFLOAT);
    TextureFormat->set_width(IDX_SQRT_MAX_VERTS);
    TextureFormat->set_height(IDX_SQRT_MAX_VERTS);

    storage.dc_index_texture   = RenderingDevice_Local->texture_create(TextureFormat, TextureView); 
    }

    Ref<RDUniform> VoxelStorageBuffer_UniformRef = RefWrapper(0, storage.voxel_output,              RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> AtomicCounter_UniformRef      = RefWrapper(1, storage.atomic_counter,            RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);

    Ref<RDUniform> VP_VertexTexture_UniformRef   = RefWrapper(0, storage.dc_vertex_texture,         RenderingDevice::UNIFORM_TYPE_IMAGE         );
    Ref<RDUniform> VP_NormalTexture_UniformRef   = RefWrapper(1, storage.dc_normal_texture,         RenderingDevice::UNIFORM_TYPE_IMAGE         );
    Ref<RDUniform> VP_UVTexture_UniformRef       = RefWrapper(2, storage.dc_uv_texture,             RenderingDevice::UNIFORM_TYPE_IMAGE         );
    Ref<RDUniform> VP_IndexTexture_UniformRef    = RefWrapper(3, storage.dc_index_texture,          RenderingDevice::UNIFORM_TYPE_IMAGE         );

    Ref<RDUniform> DC_VertexIndexBuffer_UniformRef= RefWrapper(4, storage.dc_vertex_index_buffer,    RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> DC_EdgeMaskBuffer_UniformRef  = RefWrapper(5, storage.dc_edge_mask_buffer,       RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> DC_VertexOffsetBuffer_UniformRef = RefWrapper(6, storage.dc_vertex_offset_buffer, RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> DC_VertexBuffer_UniformRef    = RefWrapper(7, storage.dc_vertex_buffer,             RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> DC_NormalBuffer_UniformRef     = RefWrapper(8, storage.dc_normal_buffer,             RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> DC_IndexBuffer_UniformRef     = RefWrapper(9, storage.dc_triangle_buffer,             RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);

    Ref<RDUniform> DC_Indirect_Dispatch_Buffer_Uniform=RefWrapper(10, storage.dc_indirect_dispatch_list_buffer, RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);

    TypedArray<Ref<RDUniform>> Voxel_Storage;
    Voxel_Storage.push_back(VoxelStorageBuffer_UniformRef);
    Voxel_Storage.push_back(AtomicCounter_UniformRef);

    TypedArray<Ref<RDUniform>> GeometryArray;
    GeometryArray.push_back(VP_VertexTexture_UniformRef);
    GeometryArray.push_back(VP_NormalTexture_UniformRef);
    GeometryArray.push_back(VP_UVTexture_UniformRef);
    GeometryArray.push_back(VP_IndexTexture_UniformRef);

    GeometryArray.push_back(DC_VertexIndexBuffer_UniformRef);
    GeometryArray.push_back(DC_EdgeMaskBuffer_UniformRef);
    GeometryArray.push_back(DC_VertexOffsetBuffer_UniformRef);
    GeometryArray.push_back(DC_VertexBuffer_UniformRef);
    GeometryArray.push_back(DC_NormalBuffer_UniformRef);
    GeometryArray.push_back(DC_IndexBuffer_UniformRef);
    GeometryArray.push_back(DC_Indirect_Dispatch_Buffer_Uniform);
    
    #ifndef PRODUCTION_BUILD
    if(G_DEBUG)
        WARN_PRINT("Creating for planet shader. NOT AN ERROR; IGNORE THIS.");
    #endif
    storage.voxel_storage = RenderingDevice_Local->uniform_set_create(Voxel_Storage, compiled_shaders.CompiledShader,                   0);

    #ifndef PRODUCTION_BUILD
    if(G_DEBUG)
        WARN_PRINT("Creating for DC dense shader. NOT AN ERROR; IGNORE THIS.");
    #endif
    storage.dc_dense_storage[0]              = RenderingDevice_Local->uniform_set_create(Voxel_Storage, compiled_shaders.CompiledShader_DualContour_Dense, 0);
    storage.dc_dense_storage[1]              = RenderingDevice_Local->uniform_set_create(GeometryArray,     compiled_shaders.CompiledShader_DualContour_Dense, 1);
    
    // create the actual mesh to display DC generated verts
    if(!SKIP_PRE_INIT_TEXTURES){
    InstanceRID = MeshInstance;
    Material_RID = RenderingServer::get_singleton()->material_create();
    RenderingServer::get_singleton()->material_set_shader(Material_RID, compiled_shaders.CompiledShader_VertexPull->get_rid());

    storage.rendering_server_vertex_texture = RenderingServer_Local->texture_rd_create(storage.dc_vertex_texture);
    storage.rendering_server_index_texture = RenderingServer_Local->texture_rd_create(storage.dc_index_texture);
    storage.rendering_server_normal_texture = RenderingServer_Local->texture_rd_create(storage.dc_normal_texture);

    RenderingServer::get_singleton()->instance_geometry_set_material_override(InstanceRID, Material_RID);
    RenderingServer::get_singleton()->material_set_param(Material_RID, "IndexTexture", storage.rendering_server_index_texture);
    RenderingServer::get_singleton()->material_set_param(Material_RID, "VertexTexture", storage.rendering_server_vertex_texture);
    RenderingServer::get_singleton()->material_set_param(Material_RID, "NormalTexture", storage.rendering_server_normal_texture);
    RenderingServer::get_singleton()->material_set_param(Material_RID, "GridSizeIndex", int(IDX_SQRT_MAX_VERTS));
    RenderingServer::get_singleton()->material_set_param(Material_RID, "GridSizeVertex", int(SQRT_MAX_VERTS));
    #ifndef PRODUCTION_BUILD
    if(G_DEBUG)
    {
        UtilityFunctions::print("GridSizeIndex: ", int(IDX_SQRT_MAX_VERTS), " | ", BasicPushConstant.VOXELS_PER_CHUNK.w);
        UtilityFunctions::print("GridSizeVertex: ", int(SQRT_MAX_VERTS), " | ", BasicPushConstant.CHUNK_SIZE.w);
    }
    #endif
    }
    {/*
    PackedInt32Array PreInitVal;
    
    PreInitVal.resize(sizeof(DC_VertexIndexBuffer) * std::pow((G_GRID_SIZE), 3));
    PreInitVal.fill(-1);
    RenderingDevice_Local->buffer_update(storage.dc_vertex_index_buffer, 0, PreInitVal.size(), PreInitVal.to_byte_array());

    PreInitVal.resize(sizeof(voxelData) * std::pow((G_GRID_SIZE + BufferPadding), 3));
    PreInitVal.fill(1.0);
    RenderingDevice_Local->buffer_update(storage.voxel_output, 0, PreInitVal.size(), PreInitVal.to_byte_array());
    */

    RenderingDevice_Local->buffer_update(storage.dc_vertex_offset_buffer, 0, VertexLoDOffsets.size(), VertexLoDOffsets.to_byte_array());
    }
}

/*
    (mostly) a note to self:
    the system is node based. this is not a monolithic end all be all for every single planet.
    it generates a single planet - it does not generate an entire galaxy. you still need to place galaxies, solar systems etc.,
    with an assignment function. this is only for generating local bodies. limited to, for now, planets.
*/

// There's no point in doing a centralized passParams_to_PCG anymore; the system is way too complex. Break it down in GDScript for easier to understand logic.

void PCG_Environment::passParams_to_PCG(const bool isCPU_or_GPU, const bool SYNC_CPU_TO_GPU,
                                        const PackedInt32Array VOXELS_PER_CHUNK, 
                                        const PackedInt32Array CHUNK_SIZE,
                                        const PackedInt32Array VERTEX_LOCATION_OFFSET,
                                        const uint32_t LEVEL_OF_DETAIL){
    if(isCPU_or_GPU){
        CHECK_RENDERING_DEVICE();
        BasicPushConstant.SEED++;
        BasicPushConstant.VertexOffsetLoD.w = LEVEL_OF_DETAIL;
        BasicPushConstant.VertexOffsetLoD.x = VERTEX_LOCATION_OFFSET[0];
        BasicPushConstant.VertexOffsetLoD.y = VERTEX_LOCATION_OFFSET[1];
        BasicPushConstant.VertexOffsetLoD.z = VERTEX_LOCATION_OFFSET[2];

        BasicPushConstant.WriteToTexturesInFirstPass = 0;
        
        RenderingDevice_Local->buffer_clear(storage.atomic_counter, 0, sizeof(AtomicBuffer));
        RenderingDevice_Local->texture_clear(storage.dc_vertex_texture, Color(0,0,0,0), 0, 1, 0, 1);
        RenderingDevice_Local->texture_clear(storage.dc_index_texture, Color(0,0,0,0), 0, 1, 0, 1);

        G_GRID_SIZE = VOXELS_PER_CHUNK[0] * CHUNK_SIZE[0];
        
        
        int64_t ComputeList = RenderingDevice_Local->compute_list_begin();
        COMPUTE_LIST_CHECK();
  
        PCG_Environment::Density_Generation_Pass(ComputeList);
        PCG_Environment::DualContour_Generation_Pass(ComputeList);
        
        RenderingDevice_Local->compute_list_end();
       
        #ifndef PRODUCTION_BUILD
        if(G_DEBUG)
            UtilityFunctions::print("Compute list recorded: ", ComputeList);
        #endif
        
        #ifndef PRODUCTION_BUILD
        if(G_DEBUG)
            UtilityFunctions::print("Compute successful. Setting mesh visibility.");
        #endif

        if(SYNC_CPU_TO_GPU){ RenderingDevice_Local->submit(); RenderingDevice_Local->sync(); }
    }

    // CPU-specific logic (intended for servers)
}