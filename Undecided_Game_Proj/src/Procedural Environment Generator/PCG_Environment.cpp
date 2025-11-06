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

RID PCG_Environment::loadGDShader(String &path_to_compute_shader, String &CompileTo, const bool doCompilation, const bool DEBUG){
    RID ComplacentValue;

    if(doCompilation)
    {
        Ref<FileAccess> GDShader_File = FileAccess::open(path_to_compute_shader, FileAccess::READ);
        
        if(GDShader_File.is_valid()){
            String ShaderSource = GDShader_File->get_as_text();

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

void PCG_Environment::SVOPass(uint32Vec &VecObj, uint32Vec &CurrentDispatchDimension, int64_t &ComputeList, PackedByteArray LOCAL_CHUNK_SIZE){
    VecObj.x = std::max(1u, CurrentDispatchDimension.x / LOCAL_CHUNK_SIZE[0]);
    VecObj.y = std::max(1u, CurrentDispatchDimension.y / LOCAL_CHUNK_SIZE[1]);
    VecObj.z = std::max(1u, CurrentDispatchDimension.z / LOCAL_CHUNK_SIZE[2]);

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

/* 
    (mostly) a note to self:
    the system is node based. this is not a monolithic end all be all for every single planet. 
    it generates a single planet, it does not generate a galaxy. you still need to place galaxies, solar systems etc. 
    -with an assignment function, this is only for generating local bodies, limited to, for now, planets.
    though it might be able to handle an entire universe, as in my early stages I did not focus on building a node based system.
*/

// boilerplate galore
// I doubt the CPU logic will be used as remaking the entire pipeline just for the CPU is insane. I'll add it if later on people really request for it.
void PCG_Environment::passParams_to_PCG(RID CompiledShader, bool isCPU_or_GPU, 
                                        const String &EditFileLocation, const String &SVO_VertexFileLocation, 
                                        const bool IS_STARTINGSCENE, const uint32_t &SEED, const uint32_t paramMAXVERTs,
                                        const PackedByteArray CHUNK_SIZE, const PackedByteArray VOXELS_PER_CHUNK, const PackedByteArray LOCAL_CHUNK_SIZE, 
                                        const uint32_t SVO_MAX_NODES_PER_CHUNK,  
                                        const PackedByteArray CURRENT_ENTITY_LOCATION,
                                        const bool CLEAR_RIDs, const bool DEBUG){
    if(isCPU_or_GPU){
        RID Pipeline_RID = RenderingDevice->compute_pipeline_create(CompiledShader);

        int64_t ComputeList = RenderingDevice->compute_list_begin();

        RenderingDevice->compute_list_bind_compute_pipeline(ComputeList, Pipeline_RID);

        ComputeUniformData UniformData;

        UniformData.SCENE_PROPERTIES.x = SEED;
        UniformData.SCENE_PROPERTIES.y = paramMAXVERTs;
        UniformData.SCENE_PROPERTIES.z = IS_STARTINGSCENE;
        UniformData.SCENE_PROPERTIES.w = 0;

        UniformData.NOISE_PARAMS.x = WORLD_SCALE;

        UniformData.CHUNK_SIZE.x = CHUNK_SIZE[0];
        UniformData.CHUNK_SIZE.y = CHUNK_SIZE[1];
        UniformData.CHUNK_SIZE.z = CHUNK_SIZE[2];

        UniformData.VOXELS_PER_CHUNK.x = VOXELS_PER_CHUNK[0];
        UniformData.VOXELS_PER_CHUNK.y = VOXELS_PER_CHUNK[1];
        UniformData.VOXELS_PER_CHUNK.z = VOXELS_PER_CHUNK[2];

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
        if(paramMAXVERTs){
            MAXVERTS = paramMAXVERTs;
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
        // this is where the actual code begins, the first bit of it was just boilerplate :skull:
        RID UniformSet_RID = RenderingDevice->uniform_set_create(UniformsArray, CompiledShader, 0);
        if(!CLEAR_RIDs){
            RenderingDevice->compute_list_bind_uniform_set(ComputeList, UniformSet_RID, 0);


            uint32Vec VecObj;
            // I could probably turn this into a function so I don't have to repeat it, but at the same time I'm too lazy so yeah
            VecObj.x = VOXELS_PER_CHUNK[0] / LOCAL_CHUNK_SIZE[0];
            VecObj.y = VOXELS_PER_CHUNK[1] / LOCAL_CHUNK_SIZE[1];
            VecObj.z = VOXELS_PER_CHUNK[2] / LOCAL_CHUNK_SIZE[2];
            RenderingDevice->compute_list_dispatch(ComputeList, VecObj.x, VecObj.y, VecObj.z);

            RenderingDevice->compute_list_add_barrier(ComputeList);
            
            UniformData.SCENE_PROPERTIES.w = 1; // this is a stage indicator. I'm too lazy to refactor it to a reference.
            memcpy(UniformDataArray.ptrw(), &UniformData, sizeof(ComputeUniformData));
            RenderingDevice->buffer_update(UniformBuffer_RID, 0, UDA_Size, UniformDataArray);

            uint32Vec CurrentDispatchDimension;
            CurrentDispatchDimension.x = VOXELS_PER_CHUNK[0] / 2;
            CurrentDispatchDimension.y = VOXELS_PER_CHUNK[1] / 2;
            CurrentDispatchDimension.z = VOXELS_PER_CHUNK[2] / 2;

            SVOPass(VecObj, CurrentDispatchDimension, ComputeList, LOCAL_CHUNK_SIZE);
            
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

                SVOPass(VecObj, CurrentDispatchDimension, ComputeList, LOCAL_CHUNK_SIZE);

                if(CurrentDispatchDimension.x == 1 && CurrentDispatchDimension.y == 1 && CurrentDispatchDimension.z == 1){
                    if(VecObj.x == 1 && VecObj.y == 1 && VecObj.z == 1) {
                        break;
                    }
                }
                BasicPushConstant.PassNum += 1;
            }

            RenderingDevice->compute_list_add_barrier(ComputeList);

            UniformData.SCENE_PROPERTIES.w = 3;
            memcpy(UniformDataArray.ptrw(), &UniformData, UDA_Size);
            RenderingDevice->buffer_update(UniformBuffer_RID, 0, UDA_Size, UniformDataArray);

            VecObj.x = std::max(1u, (unsigned)((VOXELS_PER_CHUNK[0] - 1) / LOCAL_CHUNK_SIZE[0]));
            VecObj.y = std::max(1u, (unsigned)((VOXELS_PER_CHUNK[1] - 1) / LOCAL_CHUNK_SIZE[1]));
            VecObj.z = std::max(1u, (unsigned)((VOXELS_PER_CHUNK[2] - 1) / LOCAL_CHUNK_SIZE[2]));
            RenderingDevice->compute_list_dispatch(ComputeList, VecObj.x, VecObj.y, VecObj.z);

            RenderingDevice->compute_list_add_barrier(ComputeList);

            UniformData.SCENE_PROPERTIES.w = 4;
            memcpy(UniformDataArray.ptrw(), &UniformData, UDA_Size);
            RenderingDevice->buffer_update(UniformBuffer_RID, 0, UDA_Size, UniformDataArray);

            VecObj.x = VOXELS_PER_CHUNK[0] / LOCAL_CHUNK_SIZE[0];
            VecObj.y = VOXELS_PER_CHUNK[1] / LOCAL_CHUNK_SIZE[1];
            VecObj.z = VOXELS_PER_CHUNK[2] / LOCAL_CHUNK_SIZE[2];
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

