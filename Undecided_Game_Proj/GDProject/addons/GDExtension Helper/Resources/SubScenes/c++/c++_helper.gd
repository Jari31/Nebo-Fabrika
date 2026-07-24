@tool
extends Control

# a very sloppy build utility I made because manually writing all of this tedious boilerplate got hella boring

@onready var ProjectPath = ProjectSettings.globalize_path("res://")

@export var Node_GenerateBoilerplate: Button 

@export var Node_ClassName: LineEdit
@export var Node_GodotVersion: SpinBox
@export var Node_IsEditorPlugin: CheckButton
@export var Node_GenerateMetaFileBoilerplate: CheckButton
@export var Node_ClassTypes: OptionButton

@export var Node_DirectorySuffix: LineEdit

@export var MetaFileLocation: String = "../src/meta"
var GlobalMetaFileLocation: String

enum CPPBoilerplateTypes {
	Standard,
	Resource_t, 
	Refcounted
}

var CPPBoilerplateType: CPPBoilerplateTypes = 0

func _ready() -> void:
	Node_ClassTypes.add_item(str(CPPBoilerplateTypes.keys()[0]), 0)
	Node_ClassTypes.add_item(str(CPPBoilerplateTypes.keys()[1]), 1)
	Node_ClassTypes.add_item(str(CPPBoilerplateTypes.keys()[2]), 2)
	
	Node_ClassTypes.item_selected.connect(_set_cpp_boilerplate_type)
	
	Node_GenerateBoilerplate.pressed.connect(_generate_boilerplate)
	
	#print(_globalize_relative_path_and_get_contents())
	
	_display_last_used_parameters(_globalize_relative_path_and_get_contents())
	
func _set_cpp_boilerplate_type(Index: int):
	CPPBoilerplateType = CPPBoilerplateTypes.keys()[Index]

enum FileTypes {
	ClassCpp,
	ClassH,
	ClassRegisterCpp,
	ClassRegisterH,
	ClassGDExtension
}

func _generate_boilerplate():
	var ClassName = Node_ClassName.text.strip_edges()
	if not ClassName.length() > 0:
		print("Failed to generate boilerplate. No class name provided.")
		return
	
	if FileAccess.file_exists(GlobalMetaFileLocation):
		var file = FileAccess.open(GlobalMetaFileLocation, FileAccess.READ_WRITE)
		
		if file.get_as_text().contains(Node_ClassName.text):
			print("Pre-existing meta file definition. Culling insertion attempt.")
		else:
			file.seek_end()
			file.store_line("\n" + Node_DirectorySuffix.text + "/" + Node_ClassName.text + ":" + Node_ClassName.text)
			file.close()
	else:
		print("Failed to find Meta file.")
		return
	#no static tool to replace this name with something better because fuck me i guess
	var directory = GlobalMetaFileLocation + "/../" + Node_DirectorySuffix.text + "/" + ClassName + "/"
	

	print(directory)
	
	if not DirAccess.dir_exists_absolute(directory):
		var result = DirAccess.make_dir_recursive_absolute(directory)
		print(result)
	else:
		print("Directory exists.")
	
	var cpp_extensions_folder = ProjectPath + "bin/C++ Extensions" + "/" + ClassName + "/"
	
	if not DirAccess.dir_exists_absolute(cpp_extensions_folder):
		DirAccess.make_dir_absolute(cpp_extensions_folder)
		
	
	#if FileAccess.file_exists(directory + ClassName + ".cpp"):
	#	print("Boilerplate .cpp file already exists. Exiting to avoid overwriting work.")
	#	return
	
	var class_cpp = ""
	var class_h = ""
	var class_register_cpp = ""
	var class_register_h = ""
	var class_gdextension = ""
	
	var header_guard = ClassName.to_upper()
	
	match CPPBoilerplateType:
		_:
			class_h = "#ifndef {header_guard}_H\n#define {header_guard}_H\n\n#include <godot_cpp/classes/{base_class_lowercase}.hpp>\n\nnamespace godot {\n\nclass {class_name} : public {base_class} {\n    GDCLASS({class_name}, {base_class});\n\nprotected:\n    static void _bind_methods();\n\npublic:\n    {class_name}();\n    ~{class_name}();\n};\n\n}\n\n#endif".format({"base_class": "Node3D", "base_class_lowercase": "node3d", "header_guard": header_guard, "class_name": ClassName})
			
			class_cpp = "#include \"{class_name_lowercase}.h\"\n\nnamespace godot {\n\nvoid {class_name}::_bind_methods() {\n}\n\n{class_name}::{class_name}() {\n}\n\n{class_name}::~{class_name}() {\n}\n\n}".format({"class_name": ClassName, "class_name_lowercase": ClassName})
			
			class_register_h = "#pragma once\n\n#include \"{class_name}.h\"\n\n#include <gdextension_interface.h>\n#include <godot_cpp/core/defs.hpp>\n#include <godot_cpp/core/class_db.hpp>\n\nusing namespace godot;\n\nvoid initialize{module_name}(ModuleInitializationLevel level);\nvoid uninitialize{module_name}(ModuleInitializationLevel level);".format({"class_name": ClassName, "module_name":  ClassName})
			
			class_register_cpp = "#include \"Register_{class_name}.h\"\n\nusing namespace godot;\n\nvoid initialize{module_name}(ModuleInitializationLevel level) {\n    if (level != MODULE_INITIALIZATION_LEVEL_SCENE)\n        return;\n\n    GDREGISTER_RUNTIME_CLASS({class_name});\n}\n\nvoid uninitialize{module_name}(ModuleInitializationLevel level) {\n    if (level != MODULE_INITIALIZATION_LEVEL_SCENE)\n        return;\n}\n\nextern \"C\" {\nGDExtensionBool GDE_EXPORT init_{module_name}(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {\n    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);\n\n    init_obj.register_initializer(initialize{module_name});\n    init_obj.register_terminator(uninitialize{module_name});\n    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);\n\n    return init_obj.init();\n}\n}".format({"class_name": ClassName, "module_name":  ClassName})
			
			class_gdextension = "[configuration]\nentry_symbol = \"init_{module_name}\"\ncompatibility_minimum = \"{compat_min}\"\nreloadable = true\nload_in_editor = {editor_tool}\n\n[libraries]\nwindows.x86_64 = \"res://bin/build/{class_name}.dll\"\nlinux.x86_64 = \"res://bin/build/{class_name}.so\"".format({"class_name": ClassName, "module_name":  ClassName, "editor_tool": true if Node_IsEditorPlugin.toggle_mode == true else false, "compat_min": Node_GodotVersion.value})
	
	#function inlining? never heard of it
	var lambda_create_and_write_to_file = func(Pattern: String, FileType: FileTypes):
		if FileAccess.file_exists(Pattern):
			print("File already defined. Exiting to avoid overwrites. ", FileTypes.keys()[FileType], " | ", Pattern)
			return
			
		var File = FileAccess.open(Pattern, FileAccess.WRITE)
	
		print(File)
	
		if File:
			match(FileType): 
				FileTypes.ClassH: File.store_string(class_h)
				FileTypes.ClassRegisterCpp: File.store_string(class_register_cpp)
				FileTypes.ClassRegisterH: File.store_string(class_register_h)
				FileTypes.ClassGDExtension: File.store_string(class_gdextension)
				_: File.store_string(class_cpp) # my damn mistake to think this shit would compile to a jump table
			File.close()
		else:
			print("Failed to create ", ClassName, " ", FileTypes.keys()[FileType], " ")
	
	lambda_create_and_write_to_file.call(directory + ClassName + ".cpp", FileTypes.ClassCpp)
		
	lambda_create_and_write_to_file.call(directory + ClassName + ".h", FileTypes.ClassH)
	
	lambda_create_and_write_to_file.call(directory + "Register_" + ClassName + ".cpp", FileTypes.ClassRegisterCpp)
	
	lambda_create_and_write_to_file.call(directory + "Register_" + ClassName + ".h", FileTypes.ClassRegisterH)
	
	lambda_create_and_write_to_file.call(cpp_extensions_folder + ClassName + ".gdextension", FileTypes.ClassGDExtension)
	
		
func _display_last_used_parameters(MetaFileText: String):
	var lines: PackedStringArray = MetaFileText.split("\n")
	var lines_size = lines.size() - 1
	if lines_size > 0:
		var last_line: String = lines[-1].strip_edges()
		
		if last_line.begins_with("#") or last_line == "":
			for index in lines_size: # funky recursion
				last_line = lines[lines_size - index - 1]
				
				if not last_line.strip_edges().begins_with("#") and not last_line == "":
					break
		
		if last_line.contains("#"):
			last_line.erase(last_line.find("#"), last_line.length())
			print("Erased comment, result: ", last_line)
		
		var name_of_class: PackedStringArray = last_line.split(":")
		if not name_of_class.size() == 2:
			print("The last valid line (non comment) in the meta file has an improper class name. name_of_class: ", name_of_class)
			return
		
		var cleaned_directory_string: String = name_of_class[1]
		
		if not name_of_class[1] == name_of_class[0]:
			cleaned_directory_string = name_of_class[0].replace(name_of_class[0], "")

		Node_ClassName.text = name_of_class[1]
		Node_DirectorySuffix.text = cleaned_directory_string


func _globalize_relative_path_and_get_contents() -> String:
	GlobalMetaFileLocation = ProjectPath + MetaFileLocation
	
	if FileAccess.file_exists(GlobalMetaFileLocation):
		return FileAccess.open(GlobalMetaFileLocation, FileAccess.READ).get_as_text()
	
	print("Failed to find Meta file at folder: ", GlobalMetaFileLocation)
	return "Null|FileNotFound"
