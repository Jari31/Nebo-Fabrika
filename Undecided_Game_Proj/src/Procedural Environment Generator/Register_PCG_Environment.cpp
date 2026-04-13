#include "Register_PCG_Environment.h"

using namespace godot;

void initializePCGENV(ModuleInitializationLevel level){
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE)
        return;
    
    GDREGISTER_RUNTIME_CLASS(PCG_Environment)
}

void uninitializePCGENV(ModuleInitializationLevel level){
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE)
        return;
}

// boilerplate from the documentation
extern "C"{
GDExtensionBool GDE_EXPORT init_PCGENV(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initializePCGENV);
	init_obj.register_terminator(uninitializePCGENV);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}