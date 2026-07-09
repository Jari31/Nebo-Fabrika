#include "Register_EnvironmentGenerator.h"

using namespace godot;

void initializeEnvironmentGenerator(ModuleInitializationLevel level) {
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE)
        return;

    GDREGISTER_RUNTIME_CLASS(EnvironmentGenerator);
}

void uninitializeEnvironmentGenerator(ModuleInitializationLevel level) {
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE)
        return;
}

extern "C" {
GDExtensionBool GDE_EXPORT init_EnvironmentGenerator(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initializeEnvironmentGenerator);
    init_obj.register_terminator(uninitializeEnvironmentGenerator);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}
}