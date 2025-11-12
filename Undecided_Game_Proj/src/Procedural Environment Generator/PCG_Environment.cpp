#include "PCG_Environment.h"

using namespace godot;

void PCG_Environment::_bind_methods(){
    ClassDB::bind_method(D_METHOD("PassParams_to_ProceduralContentGenerator", "CompiledShader", "isCPU_or_GPU", "WorldEditFileLocation", "LoDFileLocation", "IS_STARTINGSCENE", "SEED", "paramMAXVERTS", "CHUNKSIZE[3]", "CLEAR_RIDs", "DEBUG"), &PCG_Environment::passParams_to_PCG);
    ClassDB::bind_method(D_METHOD("CompileGDShader", "Path_to_Compute_Shader", "CompileAndSaveTo_REQUIRED", "CompileShader_ifShaderIsNot_alreadyCompiled", "DEBUG"), &PCG_Environment::passParams_to_PCG);
}

PCG_Environment::PCG_Environment(){

}

PCG_Environment::~PCG_Environment(){

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

            Ref<RDShaderSPIRV> Shader_SPIRV = RenderingDevice->shader_compile_spirv_from_source(RenderDeviceShaderFile); // (SPIRV is Vulkan's GLSL. Shader language, if you will)

            if(Shader_SPIRV.is_valid()){
                Error Result = ResourceSaver::get_singleton()->save(Shader_SPIRV, CompileTo);
                if(Result != Error::OK && DEBUG)
                    ERR_PRINT("The compute compilation file failed to save.");

                RID CompiledShader = RenderingDevice->shader_create_from_spirv(Shader_SPIRV);
                return CompiledShader;
            }
            if(DEBUG)
                ERR_PRINT("The shader provided includes errors. The compiler has failed.");
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

void PCG_Environment::SVOPass(uint32Vec3 &VecObj, uint32Vec3 &CurrentDispatchDimension, int64_t &ComputeList, PackedInt32Array CHUNK_SIZE){
    VecObj.x = std::max(1u, CurrentDispatchDimension.x / CHUNK_SIZE[0]);
    VecObj.y = std::max(1u, CurrentDispatchDimension.y / CHUNK_SIZE[1]);
    VecObj.z = std::max(1u, CurrentDispatchDimension.z / CHUNK_SIZE[2]);

    RenderingDevice->compute_list_dispatch(ComputeList, VecObj.x, VecObj.y, VecObj.z);
    RenderingDevice->compute_list_add_barrier(ComputeList);

    CurrentDispatchDimension.x = std::max(1u, CurrentDispatchDimension.x / 2);
    CurrentDispatchDimension.y = std::max(1u, CurrentDispatchDimension.y / 2);
    CurrentDispatchDimension.z = std::max(1u, CurrentDispatchDimension.z / 2);
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

void PCG_Environment::Active_Passive_Generation_Pass(uint32Vec3 &VecObj, PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                            int64_t &ComputeList, ComputeUniformData &UniformData, size_t &UDA_Size, RID UniformBuffer_RID,
                                            PackedByteArray UniformDataArray, const bool SKIP_SVO){
    RenderingDevice->buffer_update(UniformBuffer_RID, 0, UDA_Size, UniformDataArray);

    VecObj.x = VOXELS_PER_CHUNK[0] / CHUNK_SIZE[0];
    VecObj.y = VOXELS_PER_CHUNK[1] / CHUNK_SIZE[1];
    VecObj.z = VOXELS_PER_CHUNK[2] / CHUNK_SIZE[2];
    RenderingDevice->compute_list_dispatch(ComputeList, VecObj.x, VecObj.y, VecObj.z);

    RenderingDevice->compute_list_add_barrier(ComputeList);
    
    if(!SKIP_SVO){
        UniformData.SCENE_PROPERTIES.w = 1; // it's a stage indicator. I'm too lazy to refactor it; magic numbers are my beloved
        memcpy(UniformDataArray.ptrw(), &UniformData, sizeof(ComputeUniformData));
        RenderingDevice->buffer_update(UniformBuffer_RID, 0, UDA_Size, UniformDataArray);

        uint32Vec3 CurrentDispatchDimension;
        CurrentDispatchDimension.x = VOXELS_PER_CHUNK[0] / 2;
        CurrentDispatchDimension.y = VOXELS_PER_CHUNK[1] / 2;
        CurrentDispatchDimension.z = VOXELS_PER_CHUNK[2] / 2;

        SVOPass(VecObj, CurrentDispatchDimension, ComputeList, CHUNK_SIZE);
                    
        UniformData.SCENE_PROPERTIES.w = 2;
        memcpy(UniformDataArray.ptrw(), &UniformData, sizeof(ComputeUniformData));
        RenderingDevice->buffer_update(UniformBuffer_RID, 0, UDA_Size, UniformDataArray);

        PushConstant BasicPushConstant;
        BasicPushConstant.PassNum = 2;
        godot::PackedByteArray pushconst_buffer;
        pushconst_buffer.resize(sizeof(uint32_t));
        
        while (CurrentDispatchDimension.x || CurrentDispatchDimension.y || CurrentDispatchDimension.z){
            memcpy(pushconst_buffer.ptrw(), &BasicPushConstant, sizeof(PushConstant));
            RenderingDevice->compute_list_set_push_constant(ComputeList, pushconst_buffer, pushconst_buffer.size());

            SVOPass(VecObj, CurrentDispatchDimension, ComputeList, CHUNK_SIZE);

            BasicPushConstant.PassNum += 1;
        }

        RenderingDevice->compute_list_add_barrier(ComputeList);
    }
}

void PCG_Environment::RegisterLocalLocation(ComputeUniformData &UniformData, PackedInt64Array LocalEntityLocation, uint32_t &Stage){
    UniformData.ENTITY_LOCATION.x = static_cast<uint32_t>(LocalEntityLocation[0]);
    UniformData.ENTITY_LOCATION.y = static_cast<uint32_t>(LocalEntityLocation[1]);
    UniformData.ENTITY_LOCATION.z = static_cast<uint32_t>(LocalEntityLocation[2]);
    // add the latter 32 bits (that's why it has '>>' 32)
    UniformData.ENTITY_LOCATION_P2.x = static_cast<uint32_t>(LocalEntityLocation[0] >> 32);
    UniformData.ENTITY_LOCATION_P2.y = static_cast<uint32_t>(LocalEntityLocation[1] >> 32);
    UniformData.ENTITY_LOCATION_P2.z = static_cast<uint32_t>(LocalEntityLocation[2] >> 32);
    
    UniformData.ENTITY_LOCATION.w = Stage;
}

void PCG_Environment::LoopGenerationForEntity(const bool FOR_EACH_ENTITY, PackedInt64Array CURRENT_ENTITY_LOCATION, PackedInt64Array CURRENT_PLANET_LOCATION,
                                            const uint8_t PASS_AMOUNT,
                                            uint32Vec3 &VecObj,  PackedInt32Array VOXELS_PER_CHUNK, PackedInt32Array CHUNK_SIZE,
                                            int64_t &ComputeList, ComputeUniformData &UniformData, size_t &UDA_Size, RID UniformBuffer_RID,
                                            PackedByteArray UniformDataArray)
{
    PackedInt64Array GlobalPosition;

    uint32_t Stage = 0;
    
    PackedInt64Array LocalEntityLocation;

    PackedInt32Array LocalChunkSize;
    PackedInt32Array LocalVoxelSize;
    float LocalSize_Offset = 1.0;

    if(FOR_EACH_ENTITY){
        int ArraySize = CURRENT_ENTITY_LOCATION.size() * 0.3333333333333333;

        int PassOffset = 0;

        for (int entity_index = 0; entity_index < ArraySize;)
        {   
            for (int processing_pass = 0; processing_pass <= PASS_AMOUNT; processing_pass++){     
                for(int chunk_index = 0; chunk_index <= 2;){
                    LocalChunkSize[chunk_index] = CHUNK_SIZE[chunk_index] * (LocalSize_Offset * 0.5); 
                    LocalVoxelSize[chunk_index] = VOXELS_PER_CHUNK[chunk_index] * LocalSize_Offset;
                    
                    LocalSize_Offset *= 0.5;

                    chunk_index++;
                }

                for (int coordinate_index = 0; coordinate_index <= 2;){
                    LocalEntityLocation[coordinate_index] = CURRENT_ENTITY_LOCATION[coordinate_index + PassOffset] - CURRENT_PLANET_LOCATION[coordinate_index + PassOffset];
                    LocalEntityLocation[coordinate_index] += LocalChunkSize[coordinate_index];

                    coordinate_index++;
                }

                RegisterLocalLocation(UniformData, LocalEntityLocation, Stage);

                memcpy(UniformDataArray.ptrw(), &UniformData, sizeof(ComputeUniformData));
                RenderingDevice->buffer_update(UniformBuffer_RID, 0, UDA_Size, UniformDataArray);

                PCG_Environment::Active_Passive_Generation_Pass(VecObj, LocalVoxelSize, LocalChunkSize,
                                                    ComputeList, UniformData, UDA_Size, UniformBuffer_RID,
                                                    UniformDataArray, FOR_EACH_ENTITY);
            }

            PassOffset += 3;

            entity_index++;
        }
    }
    else
    {
        for (int processing_pass = 0; processing_pass <= PASS_AMOUNT; processing_pass++){     
            for(int chunk_index = 0; chunk_index <= 2;){
                LocalChunkSize[chunk_index] = CHUNK_SIZE[chunk_index] * (LocalSize_Offset * 0.5); 
                LocalVoxelSize[chunk_index] = VOXELS_PER_CHUNK[chunk_index] * LocalSize_Offset;
                
                LocalSize_Offset *= 0.5;

                chunk_index++;
            }

            for (int coordinate_index = 0; coordinate_index <= 2;){
                LocalEntityLocation[coordinate_index] = CURRENT_ENTITY_LOCATION[coordinate_index] - CURRENT_PLANET_LOCATION[coordinate_index];
                LocalEntityLocation[coordinate_index] += LocalChunkSize[coordinate_index];

                coordinate_index++;
            }

            RegisterLocalLocation(UniformData, LocalEntityLocation, Stage);

            memcpy(UniformDataArray.ptrw(), &UniformData, sizeof(ComputeUniformData));
            RenderingDevice->buffer_update(UniformBuffer_RID, 0, UDA_Size, UniformDataArray);

            PCG_Environment::Active_Passive_Generation_Pass(VecObj, LocalVoxelSize, LocalChunkSize,
                                                            ComputeList, UniformData, UDA_Size, UniformBuffer_RID,
                                                            UniformDataArray, FOR_EACH_ENTITY);
        }
    }

    
}

/* 
    (mostly) a note to self:
    the system is node based. this is not a monolithic end all be all for every single planet. 
    it generates a single planet, it does not generate a galaxy. you still need to place galaxies, solar systems etc. 
    -with an assignment function, this is only for generating local bodies; limited to, for now, planets.
    though it might be able to handle an entire universe, as in my early stages I did not focus on building a node based system.
*/

// boilerplate galore
// I doubt the CPU logic will be used as remaking the entire pipeline just for the CPU is insane. I'll add it if later on people really request for it.
// Also, this function is supposed to be abstracted, that's why it's so ambiguous.
void PCG_Environment::passParams_to_PCG(RID CompiledShader, bool isCPU_or_GPU, 
                                        const String &EditFileLocation, const String &SVO_VertexFileLocation, 
                                        const bool IS_STARTINGSCENE, const uint32_t &SEED, const uint32_t MAXVERTs, 
                                        const PackedInt32Array CHUNK_SIZE, const PackedInt32Array VOXELS_PER_CHUNK, 
                                        const uint32_t SVO_MAX_NODES_PER_CHUNK,  const uint8_t PASS_AMOUNT,
                                        const PackedInt64Array CURRENT_ENTITY_LOCATION, const PackedInt64Array CURRENT_PLANET_POSITION,
                                        const uint8_t GLOBAL_PASS_AMOUNT,
                                        const bool FOR_EACH_ENTITY,
                                        const bool CLEAR_RIDs, const bool DEBUG){
    if(isCPU_or_GPU){
        RID Pipeline_RID = RenderingDevice->compute_pipeline_create(CompiledShader);

        int64_t ComputeList = RenderingDevice->compute_list_begin();

        RenderingDevice->compute_list_bind_compute_pipeline(ComputeList, Pipeline_RID);

        ComputeUniformData UniformData;

        UniformData.SCENE_PROPERTIES.x = SEED;
        UniformData.SCENE_PROPERTIES.y = MAXVERTs;
        UniformData.SCENE_PROPERTIES.z = IS_STARTINGSCENE;
        UniformData.SCENE_PROPERTIES.w = 0;

        UniformData.NOISE_PARAMS.x = WORLD_SCALE;

        SetVector4i(UniformData.CHUNK_SIZE, CHUNK_SIZE);

        SetVector4i(UniformData.VOXELS_PER_CHUNK, VOXELS_PER_CHUNK);

        UniformData.ENTITY_LOCATION.x = CURRENT_ENTITY_LOCATION[0];
        UniformData.ENTITY_LOCATION.y = CURRENT_ENTITY_LOCATION[1];
        UniformData.ENTITY_LOCATION.z = CURRENT_ENTITY_LOCATION[2];

        PackedByteArray UniformDataArray;
        size_t UDA_Size = sizeof(ComputeUniformData);
        UniformDataArray.resize(UDA_Size);
        memcpy(UniformDataArray.ptrw(), &UniformData, UDA_Size);

        RID UniformBuffer_RID = RenderingDevice->uniform_buffer_create(UniformDataArray.size(), UniformDataArray);

        int64_t OutputSize = sizeof(returnedVoxel) * CHUNK_SIZE[0] * CHUNK_SIZE[1] * CHUNK_SIZE[2];
        RID VoxelOutputBuffer_RID = RenderingDevice->storage_buffer_create(OutputSize);    


        int64_t SVO_NodePoolSize = sizeof(uint32_t) * 2 * SVO_MAX_NODES_PER_CHUNK;
        RID SVO_NodePool_RID = RenderingDevice->storage_buffer_create(SVO_NodePoolSize);

        AtomicCounters AtomicCountersObj;
        AtomicCountersObj.NextAvailableNodeIndex = 1;

        PackedByteArray AtomicCountersArray;
        size_t ACA_Size = sizeof(AtomicCounters);
        AtomicCountersArray.resize(ACA_Size);
        memcpy(AtomicCountersArray.ptrw(), &AtomicCountersObj, ACA_Size);

        RID AtomicCountersArray_RID = RenderingDevice->uniform_buffer_create(AtomicCountersArray.size(), AtomicCountersArray);

        uint32_t IntermediateGridSize = (VOXELS_PER_CHUNK[0] / 2) * (VOXELS_PER_CHUNK[1] / 2) * (VOXELS_PER_CHUNK[2] / 2);
        RID NodePointerGridA_RID = RenderingDevice->storage_buffer_create(sizeof(uint32_t) * IntermediateGridSize);
        RID NodePointerGridB_RID = RenderingDevice->storage_buffer_create(sizeof(uint32_t) * IntermediateGridSize);

        uint64_t MAXVERTS = 0;
        if(MAXVERTs){
            MAXVERTS = MAXVERTs;
        } else {
            MAXVERTS = VOXELS_PER_CHUNK[0] * VOXELS_PER_CHUNK[1] * VOXELS_PER_CHUNK[2]; 
        }
        RID VertexBuffer_RID = RenderingDevice->storage_buffer_create(sizeof(VertexBuffer) * MAXVERTS);

        uint64_t MAX_INDICIES = MAXVERTS * 3;
        RID IndicesBuffer_RID = RenderingDevice->storage_buffer_create(sizeof(uint32_t) * MAX_INDICIES);

        uint32_t CubeGridSize = (VOXELS_PER_CHUNK[0] - 1) * (VOXELS_PER_CHUNK[1] - 1) * (VOXELS_PER_CHUNK[2] - 1);
        RID ActiveCubes_RID = RenderingDevice->storage_buffer_create(sizeof(uint32_t) * CubeGridSize);

        // for whoever is editing this afterwards, please, PLEASE order it like this. it saves so much time.
        Ref<RDUniform> UniformConstants_Ref          = RefWrapper(0, UniformBuffer_RID, RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER);
        Ref<RDUniform> VoxelStorageBuffer_UniformRef = RefWrapper(1, VoxelOutputBuffer_RID, RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        Ref<RDUniform> SVO_NodePool_UniformRef       = RefWrapper(2, SVO_NodePool_RID, RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        Ref<RDUniform> AtomicCounter_UniformRef      = RefWrapper(3, AtomicCountersArray_RID, RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        Ref<RDUniform> NodePointerGridA_UniformRef   = RefWrapper(4, NodePointerGridA_RID, RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        Ref<RDUniform> NodePointerGridB_UniformRef   = RefWrapper(5, NodePointerGridB_RID, RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        Ref<RDUniform> VertexBuffer_UniformRef       = RefWrapper(6, VertexBuffer_RID, RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        Ref<RDUniform> IndicesBuffer_UniformRef      = RefWrapper(7, IndicesBuffer_RID, RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        Ref<RDUniform> ActiveCubes_UniformRef        = RefWrapper(8, ActiveCubes_RID, RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);

        TypedArray<Ref<RDUniform>> UniformsArray;
        UniformsArray.push_back(UniformConstants_Ref);
        UniformsArray.push_back(VoxelStorageBuffer_UniformRef);
        UniformsArray.push_back(SVO_NodePool_UniformRef);
        UniformsArray.push_back(AtomicCounter_UniformRef);
        UniformsArray.push_back(NodePointerGridA_UniformRef);
        UniformsArray.push_back(NodePointerGridB_UniformRef);
        UniformsArray.push_back(VertexBuffer_UniformRef);
        UniformsArray.push_back(IndicesBuffer_UniformRef);
        UniformsArray.push_back(ActiveCubes_UniformRef);
        
        RID UniformSet_RID = RenderingDevice->uniform_set_create(UniformsArray, CompiledShader, 0);
        if(!CLEAR_RIDs){ // the actual game logic
            uint32Vec3 VecObj;

            LoopGenerationForEntity(FOR_EACH_ENTITY, CURRENT_ENTITY_LOCATION, CURRENT_PLANET_POSITION,
                                    PASS_AMOUNT,
                                    VecObj,  VOXELS_PER_CHUNK, CHUNK_SIZE,
                                    ComputeList, UniformData, UDA_Size, UniformBuffer_RID,
                                    UniformDataArray);

            UniformData.SCENE_PROPERTIES.w = 3;
            memcpy(UniformDataArray.ptrw(), &UniformData, UDA_Size);
            RenderingDevice->buffer_update(UniformBuffer_RID, 0, UDA_Size, UniformDataArray);

            VecObj.x = std::max(1u, (unsigned)((VOXELS_PER_CHUNK[0] - 1) / CHUNK_SIZE[0]));
            VecObj.y = std::max(1u, (unsigned)((VOXELS_PER_CHUNK[1] - 1) / CHUNK_SIZE[1]));
            VecObj.z = std::max(1u, (unsigned)((VOXELS_PER_CHUNK[2] - 1) / CHUNK_SIZE[2]));
            RenderingDevice->compute_list_dispatch(ComputeList, VecObj.x, VecObj.y, VecObj.z);

            RenderingDevice->compute_list_add_barrier(ComputeList);

            UniformData.SCENE_PROPERTIES.w = 4;
            memcpy(UniformDataArray.ptrw(), &UniformData, UDA_Size);
            RenderingDevice->buffer_update(UniformBuffer_RID, 0, UDA_Size, UniformDataArray);

            VecObj.x = VOXELS_PER_CHUNK[0] / CHUNK_SIZE[0];
            VecObj.y = VOXELS_PER_CHUNK[1] / CHUNK_SIZE[1];
            VecObj.z = VOXELS_PER_CHUNK[2] / CHUNK_SIZE[2];
            RenderingDevice->compute_list_dispatch(ComputeList, VecObj.x, VecObj.y, VecObj.z);

            RenderingDevice->compute_list_end();

            RenderingDevice->submit();
            RenderingDevice->sync();

            if(DEBUG)
                UtilityFunctions::print("Compute successful.");
        }
        
        if(CLEAR_RIDs){
            RenderingDevice->free_rid(AtomicCountersArray_RID);
            RenderingDevice->free_rid(NodePointerGridA_RID);
            RenderingDevice->free_rid(NodePointerGridB_RID);
            RenderingDevice->free_rid(VoxelOutputBuffer_RID);
            RenderingDevice->free_rid(UniformBuffer_RID);
            RenderingDevice->free_rid(UniformSet_RID);
            RenderingDevice->free_rid(CompiledShader);
            RenderingDevice->free_rid(Pipeline_RID);
            RenderingDevice->free_rid(SVO_NodePool_RID);
        }
        return;
    }

    // CPU specific logic (meant for servers)
}