#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/shader_include.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/reg_ex.hpp>
#include <godot_cpp/classes/reg_ex_match.hpp>
#include <godot_cpp/classes/hashing_context.hpp>

#include <godot_cpp/variant/rid.hpp>

#include <godot_cpp/templates/hash_set.hpp>

namespace godot
{
    class String;

    class ShaderCompiler : public RefCounted {
        GDCLASS(ShaderCompiler, RefCounted);
    
        protected:
        static void _bind_methods();

        private:
        HashSet<String> IncludedFiles;
        bool G_DEBUG = 0;
        Ref<RegEx> RegEx_Local;

        public:
        ShaderCompiler();
        ~ShaderCompiler();

        RID LoadOrCompileShader(const String &path_to_compute_shader, const String &CompileTo, const bool doCompilation,
                                    RenderingDevice *RenderingDevice_Local, const int SHADER_STAGE,
                                    const Vector3i WORKGROUP_SIZE, const bool DEBUG);
        String PreProcessShader(String &Source);
    };
}