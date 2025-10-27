#include "registerTest.h"
#include "test.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>

using namespace godot;

void initialize_test(ModuleInitializationLevel level){
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE)
        return;
    
    GDREGISTER_RUNTIME_CLASS(GDTest)
}

void uninitialize_test(ModuleInitializationLevel level){
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE)
        return;
}

// boilerplate from the documentation
extern "C"{
GDExtensionBool GDE_EXPORT test_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_test);
	init_obj.register_terminator(uninitialize_test);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}