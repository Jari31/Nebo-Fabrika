#include "PCG_Environment.h"

using namespace godot;

void PCG_Environment::_bind_methods(){
    ClassDB::bind_method(D_METHOD("PassParams_to_ProceduralContentGenerator", "CompiledShader", "isCPU_or_GPU", "WorldEditFileLocation", "IS_STARTINGSCENE", "SEED", "CHUNKSIZE[3]", "DEBUG"), &PCG_Environment::passParams_to_PCG);
    ClassDB::bind_method(D_METHOD("Compile", "Path_to_Compute_Shader", "CompileAndSaveTo_REQUIRED", "CompileShader_ifShaderIsNot_alreadyCompiled", "DEBUG"), &PCG_Environment::passParams_to_PCG);
}

PCG_Environment::PCG_Environment(){

}

PCG_Environment::~PCG_Environment(){

}

RID PCG_Environment::loadGDShader(String &path_to_compute_shader, String &CompileTo, const bool doCompilation, const bool DEBUG){
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
            return;
        }
        if(DEBUG)
            ERR_PRINT("The compute shader provided is not valid. FILE: " + path_to_compute_shader + "   " + "It is perhaps that the location provided doesn't exist.");
        return;
    }

    Ref<RDShaderSPIRV> preCompiledShader = ResourceLoader::get_singleton()->load(CompileTo);
    
    RID CompiledShader = RenderingDevice->shader_create_from_spirv(preCompiledShader);

    return CompiledShader;
}


// boilerplate galore
// I doubt the CPU logic will be used as remaking the entire pipeline just for the CPU is insane. I'll add it if later on people really request for it.
void PCG_Environment::passParams_to_PCG(RID &CompiledShader, bool isCPU_or_GPU, const String &EditFileLocation, const String SVO_VertexFileLocation, const bool IS_STARTINGSCENE, const uint32_t &SEED, const uint32_t paramMAXVERTs, const uint8_t (&CHUNK_SIZE)[3], const bool DEBUG){
    if(isCPU_or_GPU){
        RID Pipeline_RID = RenderingDevice->compute_pipeline_create(CompiledShader);

        int64_t ComputeList = RenderingDevice->compute_list_begin();

        RenderingDevice->compute_list_bind_compute_pipeline(ComputeList, Pipeline_RID);

        ComputeUniformData uniform_data;

        uniform_data.is_STARTING_SCENE = IS_STARTINGSCENE;
        uniform_data.seed = SEED;
        uniform_data.MAXVERTs = paramMAXVERTs;

        PackedByteArray uniform_data_array;
        uniform_data_array.resize(sizeof(ComputeUniformData));
        memcpy(uniform_data_array.ptrw(), &uniform_data, sizeof(ComputeUniformData));

        RID UniformBuffer_RID = RenderingDevice->uniform_buffer_create(uniform_data_array.size(), uniform_data_array);

        RDUniform UniformConstants;
        UniformConstants.set_uniform_type(RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER);
        UniformConstants.set_binding(0);
        UniformConstants.add_id(UniformBuffer_RID);

        int64_t OutputSize = paramMAXVERTs * sizeof(returnedVertex);
        RID VertexOutputBuffer_RID = RenderingDevice->storage_buffer_create(OutputSize);

        RDUniform StorageBufferUniform;
        StorageBufferUniform.set_uniform_type(RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER);
        StorageBufferUniform.set_binding(1);
        StorageBufferUniform.add_id(VertexOutputBuffer_RID);

        TypedArray<RDUniform> Uniforms;
        Uniforms.append(ComputeList);
        Uniforms.append(StorageBufferUniform);

        RID UniformSet_RID = RenderingDevice->uniform_set_create(Uniforms, CompiledShader, 0);

        RenderingDevice->compute_list_bind_uniform_set(ComputeList, UniformSet_RID, 0);

        uint32_t x = CHUNK_SIZE[0];
        uint32_t y = CHUNK_SIZE[1];
        uint32_t z = CHUNK_SIZE[2];
        RenderingDevice->compute_list_dispatch(ComputeList, x, y, z);

        RenderingDevice->compute_list_end();

        RenderingDevice->submit();
        RenderingDevice->sync();

        PackedByteArray ReturnedVerts = RenderingDevice->buffer_get_data(VertexOutputBuffer_RID);

        RenderingDevice->free_rid(VertexOutputBuffer_RID);
        RenderingDevice->free_rid(UniformBuffer_RID);
        RenderingDevice->free_rid(UniformSet_RID);
        RenderingDevice->free_rid(CompiledShader);
        RenderingDevice->free_rid(Pipeline_RID);
        return;
    }

    // CPU specific logic (meant for servers)
}