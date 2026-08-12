//! compile with: scons --target=ShaderComp --targetFolder='Shader compiler' --productionBuild=0
//! or, if using zig (lin, win):
//! zig build -DLibraryName="ShaderComp" -DCompileFromDirectory='Shader compiler'
//! -Dtarget=x86_64-linux zig build -DLibraryName="ShaderComp" -DCompileFromDirectory='Shader
//! compiler' -Dtarget=x86_64-windows

// IMPORTANT: This is legacy and for reference only. Refer to JSlang in tools/jslang for a more
// proper shader building utility.

#include "ShaderComp.h"
using namespace godot;

void ShaderCompiler::_bind_methods()
{
    ClassDB::bind_method(
        D_METHOD(
            "LoadOrCompileShader",
            "path_to_compute_shader",
            "compile_to",
            "do_compilation",
            "rendering_device_local",
            "shader_stage",
            "workgroup_size",
            "debug"),
        &ShaderCompiler::LoadOrCompileShader);
    // PreprocessUberShader(String &UberShader, String &CompositionShader, String &Arguments, String
    // &VariableName);
    ClassDB::bind_method(
        D_METHOD(
            "PreprocessUberShader", "UberShader", "CompositionShader", "Arguments", "VariableName"),
        &ShaderCompiler::PreprocessUberShader);
}

ShaderCompiler::ShaderCompiler()
{
    RegEx_Local.instantiate();
    RegEx_Local->compile("(?m)^#include\\s+\"([^\"]+)\""); // or, #include "res://x/y/z"
}

ShaderCompiler::~ShaderCompiler()
{
    RegEx_Local.unref();
}

RID ShaderCompiler::LoadOrCompileShader(
    const String    &path_to_compute_shader,
    const String    &CompileTo,
    const bool       doCompilation,
    RenderingDevice *RenderingDevice_Local,
    const int        SHADER_STAGE,
    const Vector3i   WORKGROUP_SIZE,
    const bool       DEBUG)
{
    RID ComplacentValue; // idk why i used this variable name but whatever ig
    G_DEBUG = DEBUG;
    if (doCompilation)
    {
        Ref<FileAccess> GDShader_File = FileAccess::open(path_to_compute_shader, FileAccess::READ);

        if (GDShader_File.is_valid())
        {
            String ShaderSource = GDShader_File->get_as_text();

            String MacroDefinition = String("#define WORKGROUP_SIZE_X ") +
                                     String::num_int64(WORKGROUP_SIZE.x) + String("\n") +
                                     String("#define WORKGROUP_SIZE_Y ") +
                                     String::num_int64(WORKGROUP_SIZE.y) + String("\n") +
                                     String("#define WORKGROUP_SIZE_Z ") +
                                     String::num_int64(WORKGROUP_SIZE.z) + String("\n");

            int64_t MacroInsertPosition = 0;
            int64_t VPosition           = ShaderSource.find("#version");
            if (VPosition != -1)
            {
                VPosition           = ShaderSource.find("\n", VPosition);
                MacroInsertPosition = VPosition + 1;
            }
            ShaderSource = ShaderSource.insert(MacroInsertPosition, MacroDefinition);

            ShaderSource = PreProcessShader(ShaderSource);
            ShaderSource = ShaderSource.replace("#[compute]", "").strip_edges();

            // if(DEBUG)
            // UtilityFunctions::print(ShaderSource);
            // return RID();
            Ref<RDShaderSource> RenderDeviceShaderFile = memnew(RDShaderSource);

            switch (SHADER_STAGE)
            {
            case 0:
                RenderDeviceShaderFile->set_stage_source(
                    RenderingDevice::SHADER_STAGE_COMPUTE, ShaderSource);
                break;
            case 1:
                RenderDeviceShaderFile->set_stage_source(
                    RenderingDevice::SHADER_STAGE_VERTEX, ShaderSource);
                break;
            case 2:
                RenderDeviceShaderFile->set_stage_source(
                    RenderingDevice::SHADER_STAGE_FRAGMENT, ShaderSource);
                break;
            }

            Ref<RDShaderSPIRV> Shader_SPIRV =
                RenderingDevice_Local->shader_compile_spirv_from_source(RenderDeviceShaderFile);
            if (DEBUG)
            {
                String err = String();

                switch (SHADER_STAGE)
                {
                case 0:
                    err = Shader_SPIRV->get_stage_compile_error(
                        RenderingDevice::SHADER_STAGE_COMPUTE);
                    break;
                case 1:
                    err =
                        Shader_SPIRV->get_stage_compile_error(RenderingDevice::SHADER_STAGE_VERTEX);
                    break;
                case 2:
                    err = Shader_SPIRV->get_stage_compile_error(
                        RenderingDevice::SHADER_STAGE_FRAGMENT);
                    break;
                }

                if (!err.is_empty())
                    ERR_PRINT(err);
            }
            if (Shader_SPIRV.is_valid())
            {
                PackedByteArray ShaderFile = PackedByteArray();

                switch (SHADER_STAGE)
                {
                case 0:
                    ShaderFile =
                        Shader_SPIRV->get_stage_bytecode(RenderingDevice::SHADER_STAGE_COMPUTE);
                    break;
                case 1:
                    ShaderFile =
                        Shader_SPIRV->get_stage_bytecode(RenderingDevice::SHADER_STAGE_VERTEX);
                    break;
                case 2:
                    ShaderFile =
                        Shader_SPIRV->get_stage_bytecode(RenderingDevice::SHADER_STAGE_FRAGMENT);
                    break;
                }

                Ref<FileAccess> File = FileAccess::open(CompileTo, FileAccess::WRITE);
                if (File.is_valid())
                {
                    File->store_buffer(ShaderFile);
                    File->flush();

#ifndef PRODUCTION_BUILD
                    if (DEBUG)
                        UtilityFunctions::print("File saved successfully.");
#endif
                }
                else
                    ERR_PRINT("File failed to save!");

                RID CompiledShader = RenderingDevice_Local->shader_create_from_spirv(Shader_SPIRV);
                return CompiledShader;
            }
            if (DEBUG)
                ERR_PRINT("The shader provided contains errors. The compiler has failed.");
            return ComplacentValue;
        }
        if (DEBUG)
            ERR_PRINT(
                "The compute shader provided is not valid. FILE: " + path_to_compute_shader +
                "   " + "It is perhaps that the location provided doesn't exist.");
        return ComplacentValue;
    }

    PackedByteArray PrecompiledShader = FileAccess::get_file_as_bytes(CompileTo);

    Ref<RDShaderSPIRV> ShaderSPIRV;
    ShaderSPIRV.instantiate();
    switch (SHADER_STAGE)
    {
    case 0:
        ShaderSPIRV->set_stage_bytecode(RenderingDevice::SHADER_STAGE_COMPUTE, PrecompiledShader);
        break;
    case 1:
        ShaderSPIRV->set_stage_bytecode(RenderingDevice::SHADER_STAGE_VERTEX, PrecompiledShader);
        break;
    case 2:
        ShaderSPIRV->set_stage_bytecode(RenderingDevice::SHADER_STAGE_FRAGMENT, PrecompiledShader);
        break;
    }

    RID CompiledShader = RenderingDevice_Local->shader_create_from_spirv(ShaderSPIRV);
    return CompiledShader;
}

String ShaderCompiler::PreProcessShader(String &Source)
{
    TypedArray<RegExMatch> Matches    = RegEx_Local->search_all(Source);
    String                 TempSource = Source;

    for (int i = Matches.size() - 1; i >= 0; i--)
    {
        Ref<RegExMatch> Match       = Matches[i];
        String          IncludePath = Match->get_string(1).strip_edges();

        if (IncludedFiles.has(IncludePath))
            continue;
        // UtilityFunctions::print(IncludePath);
        IncludedFiles.insert(IncludePath);
        String          IncludeDirectiveContent = "";
        Ref<FileAccess> File                    = FileAccess::open(IncludePath, FileAccess::READ);
        if (File.is_valid())
        {
            IncludeDirectiveContent = File->get_as_text();
            IncludeDirectiveContent = PreProcessShader(IncludeDirectiveContent);
        }
        else
            ERR_PRINT(
                UtilityFunctions::str(
                    "Include directive checks failed at",
                    __LINE__,
                    "! Last processed #include content: ",
                    IncludeDirectiveContent));

        TempSource = TempSource.left(Match->get_start()) + IncludeDirectiveContent +
                     TempSource.right(TempSource.length() - Match->get_end());
        // UtilityFunctions::print(IncludeDirectiveContent);
        Matches = RegEx_Local->search_all(
            TempSource); // it has to search it twice to account for offsets after an insert
    }

    return TempSource;
}

String ShaderCompiler::PreprocessUberShader(
    String Path_To_UberShader,
    String Path_To_CompositionShader,
    String Arguments,
    String VariableName)
{
    if (Arguments == "")
        Arguments = "Coordinates, Seed";
    if (VariableName == "")
        VariableName = "FinalDensity";
    String CompositionShader =
        FileAccess::open(Path_To_CompositionShader, FileAccess::READ)->get_as_text();

    Ref<RegEx> RegEx_TemplateComp;
    RegEx_TemplateComp.instantiate();
    RegEx_TemplateComp->compile("(\\w+)\\s*\\(\\s*\\)\\s*;");

    String Includes = "";

    TypedArray<RegExMatch> Matches = RegEx_Local->search_all(CompositionShader);
    for (int i = 0; i < Matches.size(); i++)
    {
        Ref<RegExMatch> Match = Matches[i];
        Includes += Match->get_string() + "\n";
    }

    CompositionShader = RegEx_Local->sub(CompositionShader, "", true);

    String ReplaceWith  = VariableName + " = $1(" + Arguments + "); break;";
    String ReplacedText = RegEx_TemplateComp->sub(CompositionShader, ReplaceWith, true);

    String UberShader = FileAccess::open(Path_To_UberShader, FileAccess::READ)->get_as_text();

    ReplacedText = UberShader.replace("#define _CASE_IMPORT_ 0", ReplacedText);

    return ReplacedText = ReplacedText.replace("#define _SHADER_IMPORT_ 0", Includes);
}
