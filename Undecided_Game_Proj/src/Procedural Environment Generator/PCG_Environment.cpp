#include "PCG_Environment.h"

using namespace godot;

void PCG_Environment::_bind_methods(){
    ClassDB::bind_method(D_METHOD("PassParams_to_ProceduralContentGenerator", "CompiledShader", "isCPU_or_GPU", "WorldEditFileLocation", "LoDFileLocation", "IS_STARTINGSCENE", "SEED", "paramMAXVERTS", "CHUNKSIZE[3]", "CLEAR_RIDs", "DEBUG"), &PCG_Environment::passParams_to_PCG);
    ClassDB::bind_method(D_METHOD("CompileGDShader", "Path_to_Compute_Shader", "CompileAndSaveTo_REQUIRED", "CompileShader_ifShaderIsNot_alreadyCompiled", "DEBUG"), &PCG_Environment::passParams_to_PCG);
}

PCG_Environment::PCG_Environment(){

}

PCG_Environment::~PCG_Environment(){
    RenderingDevice->free_rid(compiled_shaders.CompiledShader);
    RenderingDevice->free_rid(compiled_shaders.CompiledShader_SVO);
    RenderingDevice->free_rid(compiled_shaders.CompiledShader_DualContour);
    RenderingDevice->free_rid(compiled_shaders.CompiledShader_Radix);
    RenderingDevice->free_rid(compiled_shaders.CompiledShader_Histogram);

    RenderingDevice->free_rid(pipelines.density);
    RenderingDevice->free_rid(pipelines.svo);
    RenderingDevice->free_rid(pipelines.dual_contour);
    RenderingDevice->free_rid(pipelines.prefixsum);
    RenderingDevice->free_rid(pipelines.histogram);

    RenderingDevice->free_rid(storage.voxel_output);
    RenderingDevice->free_rid(storage.uniform_buffer);
    RenderingDevice->free_rid(storage.uniform_set);
    RenderingDevice->free_rid(storage.svo_storage);
    RenderingDevice->free_rid(storage.atomic_counter);
    RenderingDevice->free_rid(storage.atomic_counter2);
    RenderingDevice->free_rid(storage.svo_aux);
    RenderingDevice->free_rid(storage.prefixsum_offset);
    RenderingDevice->free_rid(storage.histogram_buffer);
    RenderingDevice->free_rid(storage.voxel_storage);
    RenderingDevice->free_rid(storage.svo_storage);
    RenderingDevice->free_rid(storage.histogram_storage);
}

RID PCG_Environment::loadGDShader(String &path_to_compute_shader, String &CompileTo, const bool doCompilation, const uint64_t WORKGROUP_SIZE, const bool DEBUG){
    RID ComplacentValue;

    if(doCompilation)
    {
        Ref<FileAccess> GDShader_File = FileAccess::open(path_to_compute_shader, FileAccess::READ);
        
        if(GDShader_File.is_valid()){
            String ShaderSource = GDShader_File->get_as_text();

            String MacroDefinition = String("#define WORKGROUP_SIZE") + String::num_uint64(WORKGROUP_SIZE) + String("/n"); 
            ShaderSource = MacroDefinition.insert(0, MacroDefinition);

            Ref<RDShaderSource> RenderDeviceShaderFile = memnew(RDShaderSource);
            RenderDeviceShaderFile->set_stage_source(RenderingDevice::SHADER_STAGE_COMPUTE, ShaderSource);

            Ref<RDShaderSPIRV> Shader_SPIRV = RenderingDevice->shader_compile_spirv_from_source(RenderDeviceShaderFile);

            if(Shader_SPIRV.is_valid()){
                Error Result = ResourceSaver::get_singleton()->save(Shader_SPIRV, CompileTo);
                if(Result != Error::OK && DEBUG)
                    ERR_PRINT("The compute compilation file failed to save.");

                RID CompiledShader = RenderingDevice->shader_create_from_spirv(Shader_SPIRV);
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
    RID CompiledShader = RenderingDevice->shader_create_from_spirv(preCompiledShader);
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
    if(InVector[3])
        Vector.w = InVector[3];
};

void PCG_Environment::Histogram_pass(int64_t &ComputeList,
                                     PackedInt32Array &VOXELS_PER_CHUNK, PackedInt32Array &CHUNK_SIZE)
{
    uint32_t CurrentDispatchDimension_X = ((VOXELS_PER_CHUNK[0] * CHUNK_SIZE[0]) + 256 - 1) / 256;

    RenderingDevice->compute_list_dispatch(ComputeList, CurrentDispatchDimension_X, 0, 0);
    RenderingDevice->compute_list_add_barrier(ComputeList);
}

void PCG_Environment::PrefixSum(int64_t &ComputeList,
                                PackedInt32Array &VOXELS_PER_CHUNK, PackedInt32Array &CHUNK_SIZE)
{

    RenderingDevice->compute_list_dispatch(ComputeList, 1, 1, 1);
    RenderingDevice->compute_list_add_barrier(ComputeList);
}

void PCG_Environment::Density_Generation_Pass(uint32Vec3 &VecObj, PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                              int64_t &ComputeList)
{
    VecObj.x = VOXELS_PER_CHUNK[0] / CHUNK_SIZE[0];
    VecObj.y = VOXELS_PER_CHUNK[1] / CHUNK_SIZE[1];
    VecObj.z = VOXELS_PER_CHUNK[2] / CHUNK_SIZE[2];
    RenderingDevice->compute_list_dispatch(ComputeList, VecObj.x, VecObj.y, VecObj.z);

    RenderingDevice->compute_list_add_barrier(ComputeList);
}

void PCG_Environment::SVOPass(uint32Vec3 &VecObj, uint32Vec3 &CurrentDispatchDimension, int64_t &ComputeList, PackedInt32Array CHUNK_SIZE){
    RenderingDevice->compute_list_dispatch(ComputeList, CurrentDispatchDimension.x, CurrentDispatchDimension.y, CurrentDispatchDimension.z);
    RenderingDevice->compute_list_add_barrier(ComputeList);

    CurrentDispatchDimension.x = std::max(1u, CurrentDispatchDimension.x / 2);
    CurrentDispatchDimension.y = std::max(1u, CurrentDispatchDimension.y / 2);
    CurrentDispatchDimension.z = std::max(1u, CurrentDispatchDimension.z / 2);
}

void PCG_Environment::SVO_Generation_Pass(uint32Vec3 &VecObj, PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                          int64_t &ComputeList)
{
    RenderingDevice->compute_list_bind_compute_pipeline(ComputeList, pipelines.svo);
    RenderingDevice->compute_list_bind_uniform_set(ComputeList, storage.voxel_storage, 1);
    RenderingDevice->compute_list_bind_uniform_set(ComputeList, storage.svo_storage,   2);

    uint32Vec3 CurrentDispatchDimension;
    CurrentDispatchDimension.x = ((VOXELS_PER_CHUNK[0] * CHUNK_SIZE[0]) + storage.WORKGROUP_SIZE_SVO - 1) / storage.WORKGROUP_SIZE_SVO; // faster than ceil()
    CurrentDispatchDimension.y = ((VOXELS_PER_CHUNK[1] * CHUNK_SIZE[1]) + storage.WORKGROUP_SIZE_SVO - 1) / storage.WORKGROUP_SIZE_SVO;
    CurrentDispatchDimension.z = ((VOXELS_PER_CHUNK[2] * CHUNK_SIZE[2]) + storage.WORKGROUP_SIZE_SVO - 1) / storage.WORKGROUP_SIZE_SVO;

    SVOPass(VecObj, CurrentDispatchDimension, ComputeList, CHUNK_SIZE);

    RenderingDevice->compute_list_bind_compute_pipeline(ComputeList, pipelines.histogram);
    RenderingDevice->compute_list_bind_uniform_set(ComputeList, storage.histogram_storage, 3);
    RenderingDevice->compute_list_bind_uniform_set(ComputeList, storage.svo_storage, 2);

    // i < 9 because k = 8; k = 8 because 1024^3 = 10000000000 x 10000000000 x 10000000000 || 11 digits each, but because 0 is an index, 10. 30 bits. 
    // if you cover 4 bits every pass (which the radix sort algorithm does does), then k = ceil(30 / 4) = 8
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
        RenderingDevice->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

        Histogram_pass(ComputeList, VOXELS_PER_CHUNK, CHUNK_SIZE);

        BasicPushConstant.PassNum++;
        BasicPushConstant.PassOffset += 4;
    }

    RenderingDevice->compute_list_bind_compute_pipeline(ComputeList, pipelines.prefixsum);
    PrefixSum(ComputeList, VOXELS_PER_CHUNK, CHUNK_SIZE);

    BasicPushConstant.PassNum++;
    BasicPushConstant.PassOffset = 0;

    RenderingDevice->compute_list_bind_compute_pipeline(ComputeList, pipelines.histogram);

    BasicPushConstant.PassStage = 2;
    for(int i = 0; i < k; i++)
    {
        memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
        RenderingDevice->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

        Histogram_pass(ComputeList, VOXELS_PER_CHUNK, CHUNK_SIZE);

        BasicPushConstant.PassNum++;
        BasicPushConstant.PassOffset += 4;
    }

    RenderingDevice->compute_list_bind_compute_pipeline(ComputeList, pipelines.svo);

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
                RenderingDevice->buffer_clear(storage.atomic_counter2, 0, sizeof(uint32_t));
                break;
            case 2:
                RenderingDevice->buffer_clear(storage.atomic_counter, 0, sizeof(uint32_t));
                break;
            default:
                break;
        }    
        
        memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
        RenderingDevice->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

        SVOPass(VecObj, CurrentDispatchDimension, ComputeList, CHUNK_SIZE);

        BasicPushConstant.PassNum++;
        BasicPushConstant.PassStage++;
    }

    BasicPushConstant.PassStage = ceil(BasicPushConstant.PassStage % 2);
    memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
    RenderingDevice->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

    RenderingDevice->compute_list_add_barrier(ComputeList);
}

void PCG_Environment::DualContour_Generation_Pass(uint32Vec3 &VecObj, PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                                  int64_t &ComputeList)
{
    RenderingDevice->compute_list_bind_compute_pipeline(ComputeList, pipelines.dual_contour);
    RenderingDevice->compute_list_bind_uniform_set(ComputeList, storage.uniform_set, 0);

    VecObj.x = std::max(1u, (unsigned)((VOXELS_PER_CHUNK[0] - 1) / CHUNK_SIZE[0]));
    VecObj.y = std::max(1u, (unsigned)((VOXELS_PER_CHUNK[1] - 1) / CHUNK_SIZE[1]));
    VecObj.z = std::max(1u, (unsigned)((VOXELS_PER_CHUNK[2] - 1) / CHUNK_SIZE[2]));
    RenderingDevice->compute_list_dispatch(ComputeList, VecObj.x, VecObj.y, VecObj.z);

    RenderingDevice->compute_list_add_barrier(ComputeList);

    VecObj.x = VOXELS_PER_CHUNK[0] / CHUNK_SIZE[0];
    VecObj.y = VOXELS_PER_CHUNK[1] / CHUNK_SIZE[1];
    VecObj.z = VOXELS_PER_CHUNK[2] / CHUNK_SIZE[2];
    RenderingDevice->compute_list_dispatch(ComputeList, VecObj.x, VecObj.y, VecObj.z);
}

void PCG_Environment::RegisterLocalLocation(PackedInt64Array LocalEntityLocation, uint32_t &Stage)
{
    BasicPushConstant.ENTITY_LOCATION.x    = static_cast<uint32_t>(LocalEntityLocation[0]);
    BasicPushConstant.ENTITY_LOCATION.y    = static_cast<uint32_t>(LocalEntityLocation[1]);
    BasicPushConstant.ENTITY_LOCATION.z    = static_cast<uint32_t>(LocalEntityLocation[2]);
    BasicPushConstant.ENTITY_LOCATION_P2.x = static_cast<uint32_t>(LocalEntityLocation[0] >> 32);
    BasicPushConstant.ENTITY_LOCATION_P2.y = static_cast<uint32_t>(LocalEntityLocation[1] >> 32);
    BasicPushConstant.ENTITY_LOCATION_P2.z = static_cast<uint32_t>(LocalEntityLocation[2] >> 32);
    BasicPushConstant.ENTITY_LOCATION.w    = Stage;
}

void FEE_ChunkSize_CoordinateMath(PackedInt32Array LocalChunkSize, PackedInt32Array LocalVoxelSize, PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE, PackedInt64Array LocalEntityLocation,
                                  float &LocalSize_Offset, PackedInt64Array CURRENT_ENTITY_LOCATION, PackedInt64Array CURRENT_PLANET_LOCATION)
{
    for(int chunk_index = 0; chunk_index <= 2; chunk_index++){

        int32_t localChSz = CHUNK_SIZE[chunk_index] * (LocalSize_Offset * 0.5);
        LocalChunkSize.append(localChSz);

        int32_t LocalVxSz = VOXELS_PER_CHUNK[chunk_index] * LocalSize_Offset;
        LocalVoxelSize.append(LocalVxSz);

        LocalSize_Offset *= 0.5;
    }

    for(int coordinate_index = 0; coordinate_index <= 2;){
        int64_t LocalEtLc = CURRENT_ENTITY_LOCATION[coordinate_index] - CURRENT_PLANET_LOCATION[coordinate_index];
        LocalEtLc += LocalChunkSize[coordinate_index];

        LocalEntityLocation.append(LocalEtLc);
        coordinate_index++;
    }

}

void PCG_Environment::LoopGenerationForEntity(const uint8_t FOR_EACH_ENTITY, PackedInt64Array CURRENT_ENTITY_LOCATION, PackedInt64Array CURRENT_PLANET_LOCATION,
                                              const uint8_t &PASS_AMOUNT,
                                              uint32Vec3 &VecObj, PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                              int64_t &ComputeList)
{
    PackedInt64Array GlobalPosition;

    uint32_t Stage = 0;
    
    PackedInt64Array LocalEntityLocation;

    PackedInt32Array LocalChunkSize;
    PackedInt32Array LocalVoxelSize;
    float LocalSize_Offset = 1.0;

    switch(FOR_EACH_ENTITY)
    {
        case 0:
        {
            RenderingDevice->compute_list_bind_compute_pipeline(ComputeList, pipelines.density);

            FEE_ChunkSize_CoordinateMath(LocalChunkSize, LocalVoxelSize, VOXELS_PER_CHUNK, CHUNK_SIZE, LocalEntityLocation, 
                                            LocalSize_Offset, CURRENT_ENTITY_LOCATION, CURRENT_PLANET_LOCATION);

            RegisterLocalLocation(LocalEntityLocation, Stage);
            memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
            RenderingDevice->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

            PCG_Environment::Density_Generation_Pass(VecObj, LocalVoxelSize, LocalChunkSize,
                                                        ComputeList);
        }

        case 1:
        {
            int ArraySize   = CURRENT_ENTITY_LOCATION.size() * 0.3333333333333333;
            int PassOffset  = 0;

            RenderingDevice->compute_list_bind_compute_pipeline(ComputeList, pipelines.density);

            for(int entity_index = 0; entity_index < ArraySize;)
            {
                LocalSize_Offset = 1.0;

                for(int processing_pass = 0; processing_pass <= PASS_AMOUNT; processing_pass++){
                    FEE_ChunkSize_CoordinateMath(LocalChunkSize, LocalVoxelSize, VOXELS_PER_CHUNK, CHUNK_SIZE, LocalEntityLocation, 
                                                 LocalSize_Offset, CURRENT_ENTITY_LOCATION, CURRENT_PLANET_LOCATION);

                    RegisterLocalLocation(LocalEntityLocation, Stage);
                    memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
                    RenderingDevice->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

                    PCG_Environment::Density_Generation_Pass(VecObj, LocalVoxelSize, LocalChunkSize,
                                                             ComputeList);
                }

                PassOffset += 3;
                entity_index++;
            }
            break;
        }

        case 2:
            RenderingDevice->compute_list_bind_compute_pipeline(ComputeList, pipelines.density);

            for(int processing_pass = 0; processing_pass <= PASS_AMOUNT; processing_pass++){
                FEE_ChunkSize_CoordinateMath(LocalChunkSize, LocalVoxelSize, VOXELS_PER_CHUNK, CHUNK_SIZE, LocalEntityLocation, 
                                             LocalSize_Offset, CURRENT_ENTITY_LOCATION, CURRENT_PLANET_LOCATION);

                RegisterLocalLocation(LocalEntityLocation, Stage);
                memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
                RenderingDevice->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

                PCG_Environment::Density_Generation_Pass(VecObj, LocalVoxelSize, LocalChunkSize,
                                                         ComputeList);
            }

            PCG_Environment::SVO_Generation_Pass(VecObj, LocalVoxelSize, LocalChunkSize,
                                                  ComputeList);
            break;
            // For each entity, but with SVO. would be better to do this inside of GDScript instead. makes async easier because the player might just look away from what we're trying to compute
            break;

        default:
            break;
    }
}

// i love writing boilerplate i love writing boilerplate i love writing boilerplate i love writing boilerplate i love writing boilerplate
void PCG_Environment::initCompute(const int32_t &SEED, const int32_t &MAXVERTs, const int32_t &IS_STARTINGSCENE,
                                  const PackedInt32Array CHUNK_SIZE, const PackedInt32Array VOXELS_PER_CHUNK,
                                  const PackedInt64Array CURRENT_ENTITY_LOCATION, const PackedInt64Array CURRENT_PLANET_POSITION,
                                  const uint32_t &SVO_MAX_NODES_PER_CHUNK)
{
    pipelines.density      = RenderingDevice->compute_pipeline_create(compiled_shaders.CompiledShader);
    pipelines.svo          = RenderingDevice->compute_pipeline_create(compiled_shaders.CompiledShader_SVO);
    pipelines.dual_contour = RenderingDevice->compute_pipeline_create(compiled_shaders.CompiledShader_DualContour);
    pipelines.prefixsum    = RenderingDevice->compute_pipeline_create(compiled_shaders.CompiledShader_Radix);
    pipelines.histogram    = RenderingDevice->compute_pipeline_create(compiled_shaders.CompiledShader_Histogram);

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

    PackedByteArray UniformDataArray;
    size_t UDA_Size = sizeof(ComputeUniformData);
    UniformDataArray.resize(UDA_Size);
    memcpy(UniformDataArray.ptrw(), &UniformData, UDA_Size);

    storage.uniform_buffer = RenderingDevice->uniform_buffer_create(UniformDataArray.size(), UniformDataArray);

    returnedVoxel VoxelBuffer;
    PackedByteArray VoxelBufferArray;
    int64_t OutputSize = sizeof(returnedVoxel) * CHUNK_SIZE[0] * CHUNK_SIZE[1] * CHUNK_SIZE[2] * VOXELS_PER_CHUNK[0] * VOXELS_PER_CHUNK[1] * VOXELS_PER_CHUNK[2];
    VoxelBufferArray.resize(OutputSize);
    memcpy(VoxelBufferArray.ptrw(), &VoxelBuffer, OutputSize);
    storage.voxel_output = RenderingDevice->storage_buffer_create(VoxelBufferArray.size(), VoxelBufferArray);

    SVO_NodeBuffer SVO_Node_Data;
    PackedByteArray SVO_NodePool;
    int64_t SVO_NodePoolSize = sizeof(SVO_NodeBuffer) * SVO_MAX_NODES_PER_CHUNK;
    SVO_NodePool.resize(SVO_NodePoolSize);
    memcpy(SVO_NodePool.ptrw(), &SVO_Node_Data, SVO_NodePoolSize);
    storage.svo_storage = RenderingDevice->storage_buffer_create(SVO_NodePool.size(), SVO_NodePool);

    SVO_NodeBufferAux SVO_NodeAux_Data;
    PackedByteArray SVO_NodePoolAux;
    int64_t SVO_NodePoolSizeAux = sizeof(SVO_NodeBufferAux) * SVO_MAX_NODES_PER_CHUNK;
    SVO_NodePoolAux.resize(SVO_NodePoolSizeAux);
    memcpy(SVO_NodePoolAux.ptrw(), &SVO_NodeAux_Data, SVO_NodePoolSizeAux);
    storage.svo_aux = RenderingDevice->storage_buffer_create(SVO_NodePoolAux.size(), SVO_NodePoolAux);

    uint32_t AtomicCounter = 0;
    PackedByteArray AtomicCounterArray;
    int64_t AtomicCounter_Size = sizeof(uint32_t);
    AtomicCounterArray.resize(AtomicCounter_Size);
    memcpy(AtomicCounterArray.ptrw(), &AtomicCounter, AtomicCounter_Size);
    storage.atomic_counter = RenderingDevice->storage_buffer_create(AtomicCounterArray.size(), AtomicCounterArray);

    uint32_t AtomicCounter2 = 0;
    PackedByteArray AtomicCounterArray2;
    AtomicCounterArray2.resize(AtomicCounter_Size);
    memcpy(AtomicCounterArray2.ptrw(), &AtomicCounter2, AtomicCounter_Size);
    storage.atomic_counter2 = RenderingDevice->storage_buffer_create(AtomicCounterArray2.size(), AtomicCounterArray2);

    Histogram HistogramBuffer;
    PackedByteArray HistogramBuffer_Array;
    int64_t HistoSize = sizeof(Histogram);
    HistogramBuffer_Array.resize(HistoSize);
    memcpy(HistogramBuffer_Array.ptrw(), &HistogramBuffer, HistoSize);
    storage.histogram_buffer = RenderingDevice->storage_buffer_create(HistogramBuffer_Array.size(), HistogramBuffer_Array);

    PSOffset PrefixsumOffset;
    PackedByteArray PrefixsumOffset_Array;
    int64_t PrefixSize = sizeof(PSOffset);
    PrefixsumOffset_Array.resize(PrefixSize);
    memcpy(PrefixsumOffset_Array.ptrw(), &PrefixsumOffset, PrefixSize);
    storage.prefixsum_offset = RenderingDevice->storage_buffer_create(PrefixsumOffset_Array.size(), PrefixsumOffset_Array);

    uint64_t MAXVERTS64 = MAXVERTs ? MAXVERTs
                                : (uint64_t)VOXELS_PER_CHUNK[0] * VOXELS_PER_CHUNK[1] * VOXELS_PER_CHUNK[2];

    uint32_t CubeGridSize = (VOXELS_PER_CHUNK[0] - 1) * (VOXELS_PER_CHUNK[1] - 1) * (VOXELS_PER_CHUNK[2] - 1);
    RID ActiveCubes_RID = RenderingDevice->storage_buffer_create(sizeof(uint32_t) * CubeGridSize);

    Ref<RDUniform> UniformConstants_Ref          = RefWrapper(0, storage.uniform_buffer,    RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER);
    Ref<RDUniform> VoxelStorageBuffer_UniformRef = RefWrapper(1, storage.voxel_output,      RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> SVO_Node_UniformRef           = RefWrapper(2, storage.svo_storage,       RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> AtomicCounter_UniformRef      = RefWrapper(3, storage.atomic_counter,    RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> SVO_NodeAux_UniformRef        = RefWrapper(4, storage.svo_aux,           RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> Histogram_UniformRef          = RefWrapper(5, storage.histogram_buffer,  RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> PrefixsumOffset_UniformRef    = RefWrapper(6, storage.prefixsum_offset,  RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
    Ref<RDUniform> AtomicCounter2_UniformRef     = RefWrapper(7, storage.atomic_counter2,   RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);

    TypedArray<Ref<RDUniform>> UniformsArray;
    UniformsArray.push_back(UniformConstants_Ref);

    TypedArray<Ref<RDUniform>> VoxelStorage;
    VoxelStorage.push_back(VoxelStorageBuffer_UniformRef);

    TypedArray<Ref<RDUniform>> SVOStorage;
    SVOStorage.push_back(SVO_Node_UniformRef);
    SVOStorage.push_back(AtomicCounter_UniformRef);
    SVOStorage.push_back(AtomicCounter2_UniformRef);
    SVOStorage.push_back(SVO_NodeAux_UniformRef);

    TypedArray<Ref<RDUniform>> HistogramStorage;
    HistogramStorage.push_back(Histogram_UniformRef);
    HistogramStorage.push_back(PrefixsumOffset_UniformRef);

    storage.uniform_set       = RenderingDevice->uniform_set_create(UniformsArray,    compiled_shaders.CompiledShader,           0);
    storage.voxel_storage     = RenderingDevice->uniform_set_create(VoxelStorage,     compiled_shaders.CompiledShader,           1);
    storage.voxel_storage     = RenderingDevice->uniform_set_create(VoxelStorage,     compiled_shaders.CompiledShader_SVO,       1);
    storage.svo_storage       = RenderingDevice->uniform_set_create(SVOStorage,       compiled_shaders.CompiledShader_SVO,       2);
    storage.svo_storage       = RenderingDevice->uniform_set_create(SVOStorage,       compiled_shaders.CompiledShader_Histogram, 2);
    storage.histogram_storage = RenderingDevice->uniform_set_create(HistogramStorage, compiled_shaders.CompiledShader_Histogram, 3);

    
}

/*
    (mostly) a note to self:
    the system is node based. this is not a monolithic end all be all for every single planet.
    it generates a single planet - it does not generate an entire galaxy. you still need to place galaxies, solar systems etc.,
    with an assignment function. this is only for generating local bodies. limited to, for now, planets.
*/

// There's no point in doing a centralized passParams_to_PCG anymore; the system is way too complex. Break it down in GDScript for easier to understand logic.

void PCG_Environment::passParams_to_PCG(const bool isCPU_or_GPU,
                                        const uint8_t FOR_EACH_ENTITY,
                                        const PackedInt64Array CURRENT_ENTITY_LOCATION, 
                                        const PackedInt64Array CURRENT_PLANET_POSITION,
                                        const uint8_t &PASS_AMOUNT,
                                        const PackedInt32Array VOXELS_PER_CHUNK, 
                                        const PackedInt32Array CHUNK_SIZE,
                                        const bool DEBUG){
    if(isCPU_or_GPU){
        int64_t ComputeList = RenderingDevice->compute_list_begin();
        uint32Vec3 VecObj;

        LoopGenerationForEntity(FOR_EACH_ENTITY, CURRENT_ENTITY_LOCATION, CURRENT_PLANET_POSITION,
                                PASS_AMOUNT,
                                VecObj, VOXELS_PER_CHUNK, CHUNK_SIZE,
                                ComputeList);

        PCG_Environment::DualContour_Generation_Pass(VecObj, VOXELS_PER_CHUNK, CHUNK_SIZE,
                                                     ComputeList);

        RenderingDevice->compute_list_end();
        RenderingDevice->submit();
        RenderingDevice->sync();

        if(DEBUG)
            UtilityFunctions::print("Compute successful.");

    }

    // CPU-specific logic (intended for servers)
}