/*
    COPYRIGHT (c) 2026 Jari
    Licensed under the MIT license. Refer to the license file provided within the README for details.
*/

//! compile with: scons --target=PCG_Environment --targetFolder='Procedural Environment Generator' --productionBuild=0
//todo: Implement documentation system
#include "PCG_Environment.h"

using namespace godot;

#ifndef PRODUCTION_BUILD

#define CHECK_RENDERING_DEVICE()\
    {\
        if(RenderingDevice_Local == nullptr)\
        {\
            ERR_PRINT("THE RENDERING DEVICE HAS NOT BEEN INITIALIZED. INITIALIZE IT FIRST USING SetSettings(true...). GOD SPEED.");\
            return;\
        }\
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

// ai generated boilerplate
void PCG_Environment::_bind_methods() {
    // Basic Shader Loading
    ClassDB::bind_method(D_METHOD("DEPRECATED_loadGDShader", "path_to_compute_shader", "CompileTo", 
                                    "doCompilation", "MacroDefPos", "WORKGROUP_SIZE", "DEBUG"), &PCG_Environment::DEPRECATED_loadGDShader);

    // PCG Parameter Passing
    ClassDB::bind_method(D_METHOD("passParams_to_PCG", "isCPU_or_GPU", "SYNC_CPU_TO_GPU", "FOR_EACH_ENTITY", "CURRENT_ENTITY_LOCATION", "CURRENT_PLANET_POSITION", 
                                    "PASS_AMOUNT", "VOXELS_PER_CHUNK", "CHUNK_SIZE"), &PCG_Environment::passParams_to_PCG);

    ClassDB::bind_method(D_METHOD("initCompute", 
        "SEED", "MAXVERTs", "IS_STARTINGSCENE", 
        "SKIP_SVO", "SKIP_MORTON_CODE_LUT", 
        "MORTON_CODE_LUT_FileName", 
        "CHUNK_SIZE", "VOXELS_PER_CHUNK", 
        "CURRENT_ENTITY_LOCATION", "CURRENT_PLANET_POSITION", 
        "SVO_MAX_NODES_PER_CHUNK"), 
        &PCG_Environment::initCompute);
        
    ClassDB::bind_method(D_METHOD("GetRID"), &PCG_Environment::GetRID);

    godot::ClassDB::bind_method(
        godot::D_METHOD("SetCompiledShaders", 
            "planet_shader", 
            "svo_shader", 
            "dc_dense", 
            "dc_sparse", 
            "radix_prefix", 
            "radix_scatter"), 
        &PCG_Environment::SetCompiledShaders
    );

    godot::ClassDB::bind_method(godot::D_METHOD("SetSettings", 
                                                "initLocalRenderingServer", 
                                                "DEBUG",
                                                "DC_WORKGROUP_SIZE",
                                                "SVO_WORKGROUP_SIZE",
                                                "PLANET_GEN_WORKGROUP_SIZE"),
                                                &PCG_Environment::SetSettings,
                                                DEFVAL(8), DEFVAL(8), DEFVAL(8));
}

PCG_Environment::PCG_Environment(){

}

PCG_Environment::~PCG_Environment(){
    if(RenderingDevice_Local != nullptr)
        memdelete(RenderingDevice_Local);

// --- Shaders ---
    SAFE_FREE_RID(RenderingDevice_Local, compiled_shaders.CompiledShader);
    SAFE_FREE_RID(RenderingDevice_Local, compiled_shaders.CompiledShader_SVO);
    SAFE_FREE_RID(RenderingDevice_Local, compiled_shaders.CompiledShader_DualContour_Dense);
    SAFE_FREE_RID(RenderingDevice_Local, compiled_shaders.CompiledShader_DualContour_Sparse);
    SAFE_FREE_RID(RenderingDevice_Local, compiled_shaders.CompiledShader_Radix);
    SAFE_FREE_RID(RenderingDevice_Local, compiled_shaders.CompiledShader_Histogram);

    // --- Pipelines ---
    SAFE_FREE_RID(RenderingDevice_Local, pipelines.density);
    SAFE_FREE_RID(RenderingDevice_Local, pipelines.svo);
    SAFE_FREE_RID(RenderingDevice_Local, pipelines.dual_contour_dense);
    SAFE_FREE_RID(RenderingDevice_Local, pipelines.dual_contour_sparse);
    SAFE_FREE_RID(RenderingDevice_Local, pipelines.prefixsum);
    SAFE_FREE_RID(RenderingDevice_Local, pipelines.histogram);

    // --- Storage ---
    SAFE_FREE_RID(RenderingDevice_Local, storage.voxel_output);
    SAFE_FREE_RID(RenderingDevice_Local, storage.uniform_buffer);
    SAFE_FREE_RID(RenderingDevice_Local, storage.uniform_set);
    SAFE_FREE_RID(RenderingDevice_Local, storage.svo_storage);
    SAFE_FREE_RID(RenderingDevice_Local, storage.atomic_counter);
    SAFE_FREE_RID(RenderingDevice_Local, storage.atomic_counter2);
    SAFE_FREE_RID(RenderingDevice_Local, storage.svo_aux);
    SAFE_FREE_RID(RenderingDevice_Local, storage.prefixsum_offset);
    SAFE_FREE_RID(RenderingDevice_Local, storage.histogram_buffer);
    SAFE_FREE_RID(RenderingDevice_Local, storage.voxel_svo_storage_compiledShader);
    SAFE_FREE_RID(RenderingDevice_Local, storage.histogram_storage);
    SAFE_FREE_RID(RenderingDevice_Local, storage.morton_lookuptable_buffer);

    // --- Dual Contour Buffers ---
    SAFE_FREE_RID(RenderingDevice_Local, storage.dc_edge_mask_buffer);
    SAFE_FREE_RID(RenderingDevice_Local, storage.dc_normal_buffer);
    SAFE_FREE_RID(RenderingDevice_Local, storage.dc_UV_buffer);
    SAFE_FREE_RID(RenderingDevice_Local, storage.dc_index_buffer);
    SAFE_FREE_RID(RenderingDevice_Local, storage.dc_vertex_buffer);
    SAFE_FREE_RID(RenderingDevice_Local, storage.dc_vertex_index_buffer);

    // --- Arrays ---
    for(int i = 0; i < 2; i++) {
        SAFE_FREE_RID(RenderingDevice_Local, storage.dc_dense_storage[i]);
        SAFE_FREE_RID(RenderingDevice_Local, storage.dc_sparse_storage[i]);
    }
}

Variant PCG_Environment::GetRID()
{
    return RenderingDevice_Local;
}

void PCG_Environment::SetSettings(bool initLocalRenderingServer, bool DEBUG, 
                                  uint32_t DC_WORKGROUP_SIZE, uint32_t SVO_WORKGROUP_SIZE, uint32_t PLANET_GEN_WORKGROUP_SIZE)
{
    if(initLocalRenderingServer){
        RenderingServer_Local = RenderingServer::get_singleton();
        RenderingDevice_Local = RenderingServer_Local->get_rendering_device(); //RenderingServer::get_singleton()->create_local_rendering_device();
    }
    G_DEBUG = DEBUG;

    storage.WORKGROUP_SIZE_DUAL_CONTOUR = DC_WORKGROUP_SIZE;
    storage.WORKGROUP_SIZE_SVO = SVO_WORKGROUP_SIZE;
    storage.WORKGROUP_SIZE_PLANET = PLANET_GEN_WORKGROUP_SIZE;
}

RID PCG_Environment::DEPRECATED_loadGDShader(const String &path_to_compute_shader, const String &CompileTo, const bool doCompilation, 
                                    int MacroDefPos, const uint64_t WORKGROUP_SIZE, const bool DEBUG){
    RID ComplacentValue;

    if(doCompilation)
    {
        Ref<FileAccess> GDShader_File = FileAccess::open(path_to_compute_shader, FileAccess::READ);
        
        if(GDShader_File.is_valid()){
            String ShaderSource = GDShader_File->get_as_text();
        
            String MacroDefinition = String("#define WORKGROUP_SIZE ") + String::num_uint64(WORKGROUP_SIZE) + String("\n"); 
            
            int MacroInsertPosition = 0;
            if(ShaderSource.find("#version") != -1)
                MacroInsertPosition = ShaderSource.find("\n") + 1;
            ShaderSource = ShaderSource.insert(MacroInsertPosition, MacroDefinition);

            if(DEBUG)
                UtilityFunctions::print(ShaderSource);

            Ref<RDShaderSource> RenderDeviceShaderFile = memnew(RDShaderSource);
            
            RenderDeviceShaderFile->set_stage_source(RenderingDevice::SHADER_STAGE_COMPUTE, ShaderSource);

            Ref<RDShaderSPIRV> Shader_SPIRV = RenderingDevice_Local->shader_compile_spirv_from_source(RenderDeviceShaderFile);
            if(Shader_SPIRV.is_null()){
                String err = Shader_SPIRV->get_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE);
                ERR_PRINT(err);
            }
            if(Shader_SPIRV.is_valid()){
                PackedByteArray ShaderFile = Shader_SPIRV->get_stage_bytecode(RenderingDevice::SHADER_STAGE_COMPUTE);

                Ref<FileAccess> File = FileAccess::open(CompileTo, FileAccess::WRITE);
                if(File.is_valid())
                {
                    File->store_buffer(ShaderFile);
                    File->flush();
                    
                    if(DEBUG)
                        UtilityFunctions::print("File saved successfully.");
                }
                else
                    ERR_PRINT("File failed to save!");

                RID CompiledShader = RenderingDevice_Local->shader_create_from_spirv(Shader_SPIRV);
                return CompiledShader;
            }
            if(DEBUG)
                ERR_PRINT("The shader provided contains errors. The compiler has failed.");
            return ComplacentValue;
        }
        if(DEBUG)
            ERR_PRINT("The compute shader provided is not valid. FILE: " + path_to_compute_shader + "   " + "It is perhaps that the location provided doesn't exist.");
        return ComplacentValue;
    }

    Ref<RDShaderSPIRV> preCompiledShader = ResourceLoader::get_singleton()->load(CompileTo);
    RID CompiledShader = RenderingDevice_Local->shader_create_from_spirv(preCompiledShader);
    return CompiledShader;
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

void PCG_Environment::LoadLUT(const std::string &FileName, uint32_t *Buffer)
{
    std::ifstream File(FileName, std::ios::binary);
    if(File.is_open())
    {
        File.read(reinterpret_cast<char*>(Buffer), 1024);
        File.close();
        return;
    }
    UtilityFunctions::print("LUT .bin file not found.");
}

void PCG_Environment::Histogram_pass(int64_t &ComputeList,
                                     PackedInt32Array &VOXELS_PER_CHUNK, PackedInt32Array &CHUNK_SIZE)
{
    uint32_t CurrentDispatchDimension_X = ((VOXELS_PER_CHUNK[0] * CHUNK_SIZE[0]) + 256 - 1) / 256;

    RenderingDevice_Local->compute_list_dispatch(ComputeList, CurrentDispatchDimension_X, 0, 0);
    RenderingDevice_Local->compute_list_add_barrier(ComputeList);
}

void PCG_Environment::PrefixSum(int64_t &ComputeList,
                                PackedInt32Array &VOXELS_PER_CHUNK, PackedInt32Array &CHUNK_SIZE)
{

    RenderingDevice_Local->compute_list_dispatch(ComputeList, 1, 1, 1);
    RenderingDevice_Local->compute_list_add_barrier(ComputeList);
}

void PCG_Environment::Density_Generation_Pass(PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                              int64_t &ComputeList)
{
    uint32Vec3 VecObj;
    VecObj.x = VOXELS_PER_CHUNK[0] / CHUNK_SIZE[0];
    VecObj.y = VOXELS_PER_CHUNK[1] / CHUNK_SIZE[1];
    VecObj.z = VOXELS_PER_CHUNK[2] / CHUNK_SIZE[2];
    RenderingDevice_Local->compute_list_dispatch(ComputeList, VecObj.x, VecObj.y, VecObj.z);

    RenderingDevice_Local->compute_list_add_barrier(ComputeList);
}

void PCG_Environment::SVOPass(uint32Vec3 &VecObj, uint32Vec3 &CurrentDispatchDimension, int64_t &ComputeList, PackedInt32Array CHUNK_SIZE){
    RenderingDevice_Local->compute_list_dispatch(ComputeList, CurrentDispatchDimension.x, CurrentDispatchDimension.y, CurrentDispatchDimension.z);
    RenderingDevice_Local->compute_list_add_barrier(ComputeList);

    CurrentDispatchDimension.x = std::max(1u, CurrentDispatchDimension.x / 2);
    CurrentDispatchDimension.y = std::max(1u, CurrentDispatchDimension.y / 2);
    CurrentDispatchDimension.z = std::max(1u, CurrentDispatchDimension.z / 2);
}

void PCG_Environment::SVO_DualContour_Generation_pass(PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                                      int64_t &ComputeList)
{
    // set atomic counters to 0
    // pray to the GPU gods that the player's GPU doesn't melt


}

void PCG_Environment::SVO_Generation_Pass(uint32Vec3 &VecObj, PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                          int64_t &ComputeList)
{
    RenderingDevice_Local->compute_list_bind_compute_pipeline(ComputeList, pipelines.svo);
    RenderingDevice_Local->compute_list_bind_uniform_set(ComputeList, storage.voxel_svo_storage_compiledShader, 1);
    //RenderingDevice->compute_list_bind_uniform_set(ComputeList, storage.svo_storage,   2);

    uint32Vec3 CurrentDispatchDimension;
    CurrentDispatchDimension.x = ((VOXELS_PER_CHUNK[0] * CHUNK_SIZE[0]) + storage.WORKGROUP_SIZE_SVO - 1) / storage.WORKGROUP_SIZE_SVO; // faster than ceil()
    CurrentDispatchDimension.y = ((VOXELS_PER_CHUNK[1] * CHUNK_SIZE[1]) + storage.WORKGROUP_SIZE_SVO - 1) / storage.WORKGROUP_SIZE_SVO;
    CurrentDispatchDimension.z = ((VOXELS_PER_CHUNK[2] * CHUNK_SIZE[2]) + storage.WORKGROUP_SIZE_SVO - 1) / storage.WORKGROUP_SIZE_SVO;

    SVOPass(VecObj, CurrentDispatchDimension, ComputeList, CHUNK_SIZE);

    RenderingDevice_Local->compute_list_bind_compute_pipeline(ComputeList, pipelines.histogram);
    RenderingDevice_Local->compute_list_bind_uniform_set(ComputeList, storage.histogram_storage, 3);
    RenderingDevice_Local->compute_list_bind_uniform_set(ComputeList, storage.svo_storage, 2);

    // i < 9 because k = 8; k = 8 because 1024^3 = 10000000000 x 10000000000 x 10000000000 || 11 digits each, but because 0 is an index, 10. 30 bits. 
    // if you cover 4 bits every pass (which the radix sort algorithm does does), then k = ceil(30 / 4) = 8
    {
    uint32_t k = 0;
    uint32_t Value = VOXELS_PER_CHUNK[0]; 
    while(Value)
    {
        Value >>= 1u;
        k++;
    }

    k = ((k * 3) + 4 - 1) / 4;

    BasicPushConstant.PassStage = 1; // 1 is histogram; 2 is scatter (look at compute shader for reference)
    for(int i = 0; i < k; i++)
    {
        memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
        RenderingDevice_Local->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

        Histogram_pass(ComputeList, VOXELS_PER_CHUNK, CHUNK_SIZE);

        BasicPushConstant.PassNum++;
        BasicPushConstant.PassOffset += 4;
    }
    

    RenderingDevice_Local->compute_list_bind_compute_pipeline(ComputeList, pipelines.prefixsum);
    PrefixSum(ComputeList, VOXELS_PER_CHUNK, CHUNK_SIZE);

    BasicPushConstant.PassNum++;
    BasicPushConstant.PassOffset = 0;

    RenderingDevice_Local->compute_list_bind_compute_pipeline(ComputeList, pipelines.histogram);

    BasicPushConstant.PassStage = 2;
    for(int i = 0; i < k; i++)
    {
        memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
        RenderingDevice_Local->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

        Histogram_pass(ComputeList, VOXELS_PER_CHUNK, CHUNK_SIZE);

        BasicPushConstant.PassNum++;
        BasicPushConstant.PassOffset += 4;
    }
    }

    RenderingDevice_Local->compute_list_bind_compute_pipeline(ComputeList, pipelines.svo);

    BasicPushConstant.PassNum    = 1;
    BasicPushConstant.PassOffset = 0;
    BasicPushConstant.PassStage  = 1;

    while(CurrentDispatchDimension.x > 1 || CurrentDispatchDimension.y > 1 || CurrentDispatchDimension.z > 1)
    {
        if(BasicPushConstant.PassNum > 2)
        {
            BasicPushConstant.PassNum = 1;
            
        }

        switch(BasicPushConstant.PassNum)
        {
            case 1:
                RenderingDevice_Local->buffer_clear(storage.atomic_counter2, 0, sizeof(uint32_t));
                break;
            case 2:
                RenderingDevice_Local->buffer_clear(storage.atomic_counter, 0, sizeof(uint32_t));
                break;
            default:
                break;
        }    
        
        memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
        RenderingDevice_Local->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

        SVOPass(VecObj, CurrentDispatchDimension, ComputeList, CHUNK_SIZE);

        BasicPushConstant.PassNum++;
        BasicPushConstant.PassStage++;
    }
    BasicPushConstant.SVO_VoxelSize = (float(CHUNK_SIZE[0]) / float(VOXELS_PER_CHUNK[0])) * (2, BasicPushConstant.PassStage);
    BasicPushConstant.PassStage = ceil(BasicPushConstant.PassStage % 2);
    memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
    RenderingDevice_Local->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

    RenderingDevice_Local->compute_list_add_barrier(ComputeList);
}

void PCG_Environment::DualContour_Generation_Pass(PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                                  int64_t &ComputeList)
{
    RenderingDevice_Local->compute_list_bind_compute_pipeline(ComputeList, pipelines.dual_contour_dense);

    BasicPushConstant.PassNum = 4;
    BasicPushConstant.PassOffset = 0;
    
    memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
    RenderingDevice_Local->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

    RenderingDevice_Local->compute_list_bind_uniform_set(ComputeList, storage.dc_dense_storage[0], 0);
    RenderingDevice_Local->compute_list_bind_uniform_set(ComputeList, storage.dc_dense_storage[1], 1);

    uint32_t DispatchSize = (VOXELS_PER_CHUNK[0] * CHUNK_SIZE[0]) / storage.WORKGROUP_SIZE_DUAL_CONTOUR; // no floor because if the user inputs anything non-warp friendly, 
                                                                                                         // they might want to rethink their career path
                                                                                                         // okay, but for real though, computers like working in powers of two
    RenderingDevice_Local->compute_list_dispatch(ComputeList, DispatchSize, DispatchSize, DispatchSize);
    RenderingDevice_Local->compute_list_add_barrier(ComputeList);

    BasicPushConstant.PassOffset = 1;

    memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
    RenderingDevice_Local->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

    RenderingDevice_Local->compute_list_dispatch(ComputeList, DispatchSize, DispatchSize, DispatchSize);
    RenderingDevice_Local->compute_list_add_barrier(ComputeList);
}
/*
void PCG_Environment::RegisterLocalLocation(PackedInt32Array LocalEntityLocation, uint32_t &Stage)
{
    BasicPushConstant.ENTITY_LOCATION.x    = static_cast<uint32_t>(LocalEntityLocation[0]);
    BasicPushConstant.ENTITY_LOCATION.y    = static_cast<uint32_t>(LocalEntityLocation[1]);
    BasicPushConstant.ENTITY_LOCATION.z    = static_cast<uint32_t>(LocalEntityLocation[2]);
    BasicPushConstant.ENTITY_LOCATION_P2.x = static_cast<uint32_t>(LocalEntityLocation[0] >> 32);
    BasicPushConstant.ENTITY_LOCATION_P2.y = static_cast<uint32_t>(LocalEntityLocation[1] >> 32);
    BasicPushConstant.ENTITY_LOCATION_P2.z = static_cast<uint32_t>(LocalEntityLocation[2] >> 32);
}
*/
/*
void FEE_ChunkSize_CoordinateMath(PackedInt32Array LocalChunkSize[], PackedInt32Array LocalVoxelSize[], PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE, PackedInt32Array LocalEntityLocation,
                                  float &LocalSize_Offset, PackedInt32Array CURRENT_ENTITY_LOCATION, PackedInt32Array CURRENT_PLANET_LOCATION)
{
    for(int chunk_index = 0; chunk_index <= 2; chunk_index++){

        int32_t LocalChSz = CHUNK_SIZE[chunk_index] * (LocalSize_Offset * 0.5);
        LocalChunkSize[chunk_index].append(LocalChSz);

        int32_t LocalVxSz = VOXELS_PER_CHUNK[chunk_index] * LocalSize_Offset;
        LocalVoxelSize[chunk_index] = LocalVxSz;

        LocalSize_Offset *= 0.5;
    }

    for(int coordinate_index = 0; coordinate_index <= 2;){
        int64_t LocalEtLc = CURRENT_ENTITY_LOCATION[coordinate_index] - CURRENT_PLANET_LOCATION[coordinate_index];
        LocalEtLc += LocalChunkSize[coordinate_index];

        LocalEntityLocation.append(LocalEtLc);
        coordinate_index++;
    }

} // this shit doesn't work because PackedInt32Array is passed as a copy, not a pointer
*/
void PCG_Environment::LoopGenerationForEntity(const uint8_t FOR_EACH_ENTITY, PackedInt32Array CURRENT_ENTITY_LOCATION, PackedInt32Array CURRENT_PLANET_LOCATION,
                                              const uint8_t &PASS_AMOUNT,
                                              uint32Vec3 &VecObj, PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                              int64_t &ComputeList)
{
    PackedInt64Array GlobalPosition;

    uint32_t Stage = 0;
    
    PackedInt32Array LocalEntityLocation;

    PackedInt32Array LocalChunkSize;
    PackedInt32Array LocalVoxelSize;
    float LocalSize_Offset = 1.0;

    switch(FOR_EACH_ENTITY)
    {
        case 0:
        {
            RenderingDevice_Local->compute_list_bind_compute_pipeline(ComputeList, pipelines.density);
            /*
            for(int coordinate_index = 0; coordinate_index <= 2;){
                int64_t LocalEtLc = CURRENT_ENTITY_LOCATION[coordinate_index] - CURRENT_PLANET_LOCATION[coordinate_index];
                LocalEtLc += LocalChunkSize[coordinate_index]; // bro imagine indexing an empty local array. couldn't be me :laughing-emoji:

                LocalEntityLocation.append(LocalEtLc);
                coordinate_index++;
            }
            */
            //RegisterLocalLocation(LocalEntityLocation, Stage);
            
            memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
            RenderingDevice_Local->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());
            RenderingDevice_Local->compute_list_bind_uniform_set(ComputeList, storage.uniform_set, 0);
            RenderingDevice_Local->compute_list_bind_uniform_set(ComputeList, storage.voxel_svo_storage_compiledShader, 1);
  
            PCG_Environment::Density_Generation_Pass(VOXELS_PER_CHUNK, CHUNK_SIZE,
                                                        ComputeList);

            PCG_Environment::DualContour_Generation_Pass(VOXELS_PER_CHUNK, CHUNK_SIZE, ComputeList);

            
            break;
        }

        case 1: //todo: implement CPU side async on the server/build an entirely new system for the server
        {
            int ArraySize   = CURRENT_ENTITY_LOCATION.size() * 0.3333333333333333;
            int PassOffset  = 0;

            RenderingDevice_Local->compute_list_bind_compute_pipeline(ComputeList, pipelines.density);

            for(int entity_index = 0; entity_index < ArraySize;)
            {
                LocalSize_Offset = 1.0;

                for(int processing_pass = 0; processing_pass <= PASS_AMOUNT; processing_pass++){
                    
                    //RegisterLocalLocation(LocalEntityLocation, Stage);
                    memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
                    RenderingDevice_Local->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

                    PCG_Environment::Density_Generation_Pass(VOXELS_PER_CHUNK, CHUNK_SIZE,
                                                             ComputeList);
                }

                PassOffset += 3;
                entity_index++;
            }
            break;
        }

        case 2:
            RenderingDevice_Local->compute_list_bind_compute_pipeline(ComputeList, pipelines.density);

            //RegisterLocalLocation(LocalEntityLocation, Stage);
            memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
            RenderingDevice_Local->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

            PCG_Environment::Density_Generation_Pass(VOXELS_PER_CHUNK, CHUNK_SIZE,
                                                        ComputeList);

            PCG_Environment::SVO_Generation_Pass(VecObj, LocalVoxelSize, LocalChunkSize,
                                                  ComputeList);

            //PCG_Environment::SVO_DualContour_Generation_pass();
            break;
            // For each entity, but with SVO. would be better to do this inside of GDScript instead. makes async easier because the player might just look away from what we're trying to compute
        default:
            break;
    }
}

void PCG_Environment::SetCompiledShaders(RID PlanetShader, RID SVOShader, RID DualContouring_Dense, RID DualContouring_Sparse, 
                                        RID RadixSort_PrefixSum_Shader, RID RadixSort_Histogram_Scatter_Shader)
{
    compiled_shaders.CompiledShader = PlanetShader;
    compiled_shaders.CompiledShader_SVO = SVOShader;
    compiled_shaders.CompiledShader_DualContour_Dense = DualContouring_Dense;
    compiled_shaders.CompiledShader_DualContour_Sparse = DualContouring_Sparse;
    compiled_shaders.CompiledShader_Radix = RadixSort_PrefixSum_Shader;
    compiled_shaders.CompiledShader_Histogram = RadixSort_Histogram_Scatter_Shader;
}


// i love writing boilerplate i love writing boilerplate i love writing boilerplate i love writing boilerplate i love writing boilerplate
void PCG_Environment::initCompute(const uint32_t &SEED, const int32_t &MAXVERTs, const int32_t &IS_STARTINGSCENE,
                                const bool SKIP_SVO, const bool SKIP_MORTON_CODE_LUT,
                                const PackedStringArray MORTON_CODE_LUT_FileName,
                                const PackedInt32Array CHUNK_SIZE, const PackedInt32Array VOXELS_PER_CHUNK,
                                const PackedInt32Array CURRENT_ENTITY_LOCATION, const PackedInt32Array CURRENT_PLANET_POSITION,
                                const uint32_t &SVO_MAX_NODES_PER_CHUNK)
{ // implement a SKIP_SVO system so the player's PC doesn't wake up to beat them up
    pipelines.density            = RenderingDevice_Local->compute_pipeline_create(compiled_shaders.CompiledShader);
    pipelines.svo                = RenderingDevice_Local->compute_pipeline_create(compiled_shaders.CompiledShader_SVO);
    pipelines.dual_contour_dense = RenderingDevice_Local->compute_pipeline_create(compiled_shaders.CompiledShader_DualContour_Dense);
    if(!SKIP_SVO)
        pipelines.dual_contour_sparse= RenderingDevice_Local->compute_pipeline_create(compiled_shaders.CompiledShader_DualContour_Sparse);
    pipelines.prefixsum          = RenderingDevice_Local->compute_pipeline_create(compiled_shaders.CompiledShader_Radix);
    pipelines.histogram          = RenderingDevice_Local->compute_pipeline_create(compiled_shaders.CompiledShader_Histogram);

    UtilityFunctions::print(pipelines.density);

    BasicPushConstant.SEED = SEED;

    G_INITIALIZED          = true;
    G_SKIP_MORTON_CODE_LUT = SKIP_MORTON_CODE_LUT;
    G_SKIP_SVO             = SKIP_SVO;

    pushconst_buffer.resize(sizeof(PushConstant));

    ComputeUniformData UniformData;

    UniformData.SCENE_PROPERTIES.x = SEED;
    UniformData.SCENE_PROPERTIES.y = MAXVERTs;
    UniformData.SCENE_PROPERTIES.z = IS_STARTINGSCENE;
    UniformData.SCENE_PROPERTIES.w = 0;

    UniformData.NOISE_PARAMS.x = WORLD_SCALE;

    SetVector4i(UniformData.CHUNK_SIZE,      CHUNK_SIZE);
    SetVector4i(UniformData.VOXELS_PER_CHUNK, VOXELS_PER_CHUNK);

    UniformData.ENTITY_LOCATION.x = CURRENT_ENTITY_LOCATION[0];
    UniformData.ENTITY_LOCATION.y = CURRENT_ENTITY_LOCATION[1];
    UniformData.ENTITY_LOCATION.z = CURRENT_ENTITY_LOCATION[2];

    {
    PackedByteArray UniformDataArray;
    size_t UDA_Size = sizeof(ComputeUniformData);
    UniformDataArray.resize(UDA_Size);
    memcpy(UniformDataArray.ptrw(), &UniformData, UDA_Size);

    storage.uniform_buffer = RenderingDevice_Local->uniform_buffer_create(UniformDataArray.size(), UniformDataArray);
    }

    {
    uint32_t TotalDenseNodes = CHUNK_SIZE[0] * CHUNK_SIZE[1] * CHUNK_SIZE[2] * VOXELS_PER_CHUNK[0] * VOXELS_PER_CHUNK[1] * VOXELS_PER_CHUNK[2];
    BasicPushConstant.Dense_TotalNodes = TotalDenseNodes + 3;
    returnedVoxel VoxelBuffer;
    PackedByteArray VoxelBufferArray;
    int64_t OutputSize = sizeof(returnedVoxel) * TotalDenseNodes + (3 * sizeof(returnedVoxel)); // ghost padding
    VoxelBufferArray.resize(OutputSize);
    //memcpy(VoxelBufferArray.ptrw(), &VoxelBuffer, OutputSize);
    storage.voxel_output = RenderingDevice_Local->storage_buffer_create(VoxelBufferArray.size(), VoxelBufferArray);
    }

        
    auto create_storage_buffer = [&](auto& buffer_struct, int64_t count, float multiplier = 1.0f) {
        using BufferType = std::remove_reference_t<decltype(buffer_struct)>;
        
        PackedByteArray byteArray;
        int64_t size = static_cast<int64_t>(ceil(sizeof(BufferType) * count * multiplier));
        byteArray.resize(size);

        //memcpy(byteArray.ptrw(), &buffer_struct, size);
        return RenderingDevice_Local->storage_buffer_create(byteArray.size(), byteArray);
    };

    {
    {
        SVO_NodeBuffer SVO_Node_Data;
        if(!SKIP_SVO)
            storage.svo_storage = create_storage_buffer(SVO_Node_Data, SVO_MAX_NODES_PER_CHUNK);
        else
            storage.svo_storage = create_storage_buffer(SVO_Node_Data, 1);
    }

    {
        SVO_NodeBufferAux SVO_NodeAux_Data;
        if(!SKIP_SVO)
            storage.svo_aux = create_storage_buffer(SVO_NodeAux_Data, SVO_MAX_NODES_PER_CHUNK);
        else
            storage.svo_aux = create_storage_buffer(SVO_NodeAux_Data, 1);
    }

    {
    uint32_t AtomicCounter = 0;
    storage.atomic_counter = create_storage_buffer(AtomicCounter, 1);
    
    uint32_t AtomicCounter2 = 0;
    storage.atomic_counter2 = create_storage_buffer(AtomicCounter2, 1);
    }

    {
    Histogram HistogramBuffer;
    storage.histogram_buffer = create_storage_buffer(HistogramBuffer, 1);
    }

    {
    PSOffset PrefixsumOffset;
    storage.prefixsum_offset = create_storage_buffer(PrefixsumOffset, 1);
    }
    }

    {
    Morton_LUT MortonCode_LUT;
    PackedByteArray MortonCode_LUTArray;
    int64_t MLutSize = sizeof(Morton_LUT);
    MortonCode_LUTArray.resize(MLutSize);
    
    if(!SKIP_MORTON_CODE_LUT){
        LoadLUT(MORTON_CODE_LUT_FileName.get(0).utf8().get_data(), MortonCode_LUT.Interleave_Table);
        LoadLUT(MORTON_CODE_LUT_FileName.get(0).utf8().get_data(), MortonCode_LUT.De_Interleave_Table);
    }

    memcpy(MortonCode_LUTArray.ptrw(), &MortonCode_LUT, MLutSize);
    storage.morton_lookuptable_buffer = RenderingDevice_Local->storage_buffer_create(MortonCode_LUTArray.size(), MortonCode_LUTArray);
    }


    {
    {
    DC_VertexBuffer VertexBuffer;
    storage.dc_vertex_buffer = create_storage_buffer(VertexBuffer, MAXVERTs);
    }

    {
    DC_NormalBuffer NormalBuffer;
    storage.dc_normal_buffer = create_storage_buffer(NormalBuffer, MAXVERTs);
    }

    {
    DC_UVBuffer UVBuffer;
    storage.dc_UV_buffer = create_storage_buffer(UVBuffer, MAXVERTs);
    }

    {
    DC_VertexIndexBuffer VIndexBuffer;
    storage.dc_vertex_index_buffer = create_storage_buffer(VIndexBuffer, MAXVERTs);
    }

    {
    DC_IndexBuffer IndexBuffer;
    storage.dc_index_buffer = create_storage_buffer(IndexBuffer, MAXVERTs, 1.5f);
    }

    {
    DC_EdgeMaskBuffer EdgeMaskBuffer;
    storage.dc_edge_mask_buffer = create_storage_buffer(EdgeMaskBuffer, MAXVERTs);
    }
    }   

    Ref<RDUniform> UniformConstants_Ref          = RefWrapper(0, storage.uniform_buffer,            RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER);

    Ref<RDUniform> VoxelStorageBuffer_UniformRef = RefWrapper(0, storage.voxel_output,              RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> MortonCodeLUT_UniformRef      = RefWrapper(1, storage.morton_lookuptable_buffer, RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> SVO_Node_UniformRef           = RefWrapper(2, storage.svo_storage,               RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> SVO_NodeAux_UniformRef        = RefWrapper(3, storage.svo_aux,                   RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> AtomicCounter_UniformRef      = RefWrapper(4, storage.atomic_counter,            RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> Histogram_UniformRef          = RefWrapper(5, storage.histogram_buffer,          RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> PrefixsumOffset_UniformRef    = RefWrapper(6, storage.prefixsum_offset,          RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);

    Ref<RDUniform> DC_VertexBuffer_UniformRef    = RefWrapper(0, storage.dc_vertex_buffer,          RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> DC_NormalBuffer_UniformRef    = RefWrapper(1, storage.dc_normal_buffer,         RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> DC_UVBuffer_UniformRef        = RefWrapper(2, storage.dc_UV_buffer,             RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> DC_IndexBuffer_UniformRef     = RefWrapper(3, storage.dc_index_buffer,          RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform>DC_VertexIndexBuffer_UniformRef= RefWrapper(4, storage.dc_vertex_index_buffer,    RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> DC_EdgeMaskBuffer_UniformRef  = RefWrapper(5, storage.dc_edge_mask_buffer,      RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);

    
    TypedArray<Ref<RDUniform>> UniformsArray;
    UniformsArray.push_back(UniformConstants_Ref);

    TypedArray<Ref<RDUniform>> Voxel_SVO_Storage;
    Voxel_SVO_Storage.push_back(VoxelStorageBuffer_UniformRef);

    Voxel_SVO_Storage.push_back(MortonCodeLUT_UniformRef);

    Voxel_SVO_Storage.push_back(SVO_Node_UniformRef);
    Voxel_SVO_Storage.push_back(SVO_NodeAux_UniformRef);

    Voxel_SVO_Storage.push_back(AtomicCounter_UniformRef);
    //Voxel_SVO_Storage.push_back(AtomicCounter2_UniformRef); deprecated

    Voxel_SVO_Storage.push_back(Histogram_UniformRef);
    Voxel_SVO_Storage.push_back(PrefixsumOffset_UniformRef);

    TypedArray<Ref<RDUniform>> GeometryArray;
    GeometryArray.push_back(DC_VertexBuffer_UniformRef);
    GeometryArray.push_back(DC_NormalBuffer_UniformRef);
    GeometryArray.push_back(DC_UVBuffer_UniformRef);
    GeometryArray.push_back(DC_IndexBuffer_UniformRef);
    GeometryArray.push_back(DC_VertexIndexBuffer_UniformRef);
    GeometryArray.push_back(DC_EdgeMaskBuffer_UniformRef);

    /*
    TypedArray<Ref<RDUniform>> SVOStorage;
    SVOStorage.push_back(SVO_Node_UniformRef);
    SVOStorage.push_back(AtomicCounter_UniformRef);
    SVOStorage.push_back(AtomicCounter2_UniformRef);
    SVOStorage.push_back(SVO_NodeAux_UniformRef);

    TypedArray<Ref<RDUniform>> HistogramStorage;
    HistogramStorage.push_back(Histogram_UniformRef);
    HistogramStorage.push_back(PrefixsumOffset_UniformRef);
    */
    #ifndef PRODUCTION_BUILD
    if(G_DEBUG)
        WARN_PRINT("Creating for planet shader. NOT AN ERROR; IGNORE THIS.");
    #endif
    storage.uniform_set                      = RenderingDevice_Local->uniform_set_create(UniformsArray,     compiled_shaders.CompiledShader,                   0);
    storage.voxel_svo_storage_compiledShader = RenderingDevice_Local->uniform_set_create(Voxel_SVO_Storage, compiled_shaders.CompiledShader,                   1);

    #ifndef PRODUCTION_BUILD
    if(G_DEBUG)
        WARN_PRINT("Creating for SVO shader. NOT AN ERROR; IGNORE THIS.");
    #endif
    storage.svo_storage                      = RenderingDevice_Local->uniform_set_create(Voxel_SVO_Storage, compiled_shaders.CompiledShader_SVO,               0);
    storage.histogram_storage                = RenderingDevice_Local->uniform_set_create(Voxel_SVO_Storage, compiled_shaders.CompiledShader_Histogram,         0);

    #ifndef PRODUCTION_BUILD
    if(G_DEBUG)
        WARN_PRINT("Creating for DC dense shader. NOT AN ERROR; IGNORE THIS.");
    #endif
    storage.dc_dense_storage[0]              = RenderingDevice_Local->uniform_set_create(Voxel_SVO_Storage, compiled_shaders.CompiledShader_Histogram,         0);
    storage.dc_dense_storage[1]              = RenderingDevice_Local->uniform_set_create(GeometryArray,     compiled_shaders.CompiledShader_DualContour_Dense, 1);

    #ifndef PRODUCTION_BUILD
    if(G_DEBUG)
        WARN_PRINT("Creating for DC sparse shader. NOT AN ERROR; IGNORE THIS.");
    #endif
    storage.dc_sparse_storage[0]             = RenderingDevice_Local->uniform_set_create(Voxel_SVO_Storage, compiled_shaders.CompiledShader_DualContour_Sparse,0);
    storage.dc_sparse_storage[1]             = RenderingDevice_Local->uniform_set_create(GeometryArray,     compiled_shaders.CompiledShader_DualContour_Sparse,1);
    //storage.histogram_storage = RenderingDevice->uniform_set_create(HistogramStorage, compiled_shaders.CompiledShader_Histogram, 2);
    //storage.voxel_storage     = RenderingDevice->uniform_set_create(VoxelStorage,     compiled_shaders.CompiledShader,           1);
    //storage.svo_storage       = RenderingDevice->uniform_set_create(SVOStorage,       compiled_shaders.CompiledShader_SVO,       2);
}

/*
    (mostly) a note to self:
    the system is node based. this is not a monolithic end all be all for every single planet.
    it generates a single planet - it does not generate an entire galaxy. you still need to place galaxies, solar systems etc.,
    with an assignment function. this is only for generating local bodies. limited to, for now, planets.
*/

// There's no point in doing a centralized passParams_to_PCG anymore; the system is way too complex. Break it down in GDScript for easier to understand logic.

void PCG_Environment::passParams_to_PCG(const bool isCPU_or_GPU, const bool SYNC_CPU_TO_GPU,
                                        const uint8_t FOR_EACH_ENTITY,
                                        const PackedInt32Array CURRENT_ENTITY_LOCATION, 
                                        const PackedInt32Array CURRENT_PLANET_POSITION,
                                        const uint8_t &PASS_AMOUNT,
                                        const PackedInt32Array VOXELS_PER_CHUNK, 
                                        const PackedInt32Array CHUNK_SIZE){
    if(isCPU_or_GPU){
        CHECK_RENDERING_DEVICE();

        int64_t ComputeList = RenderingDevice_Local->compute_list_begin();
        COMPUTE_LIST_CHECK();
        uint32Vec3 VecObj;

        LoopGenerationForEntity(FOR_EACH_ENTITY, CURRENT_ENTITY_LOCATION, CURRENT_PLANET_POSITION,
                                PASS_AMOUNT,
                                VecObj, VOXELS_PER_CHUNK, CHUNK_SIZE,
                                ComputeList);

        RenderingDevice_Local->compute_list_end();

        #ifndef PRODUCTION_BUILD
        if(G_DEBUG)
            UtilityFunctions::print("Compute successful.");
        #endif

        RenderingDevice_Local->submit();
        
        if(SYNC_CPU_TO_GPU)
            RenderingDevice_Local->sync();

        #ifndef PRODUCTION_BUILD
        if(G_DEBUG)
            UtilityFunctions::print("Compute list recorded: ", ComputeList);
        #endif
    }

    // CPU-specific logic (intended for servers)
}