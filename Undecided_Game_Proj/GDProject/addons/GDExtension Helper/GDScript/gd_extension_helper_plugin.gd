@tool
extends EditorPlugin

const WORKSPACE_SCENE = preload("../Resources/GDExtension Helper.tscn")
var WorkspaceInstance: Control

func _has_main_screen() -> bool:
	return true
	
func _enter_tree() -> void:
	WorkspaceInstance = WORKSPACE_SCENE.instantiate()
	
	print("Filling screen...")
	#WorkspaceInstance.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	#WorkspaceInstance.size_flags_vertical = Control.SIZE_EXPAND_FILL
	
	EditorInterface.get_editor_main_screen().add_child(WorkspaceInstance)
	_make_visible(false)
	
func _make_visible(Visible: bool) -> void:
	if WorkspaceInstance:
		WorkspaceInstance.visible = Visible

func _get_plugin_name() -> String:
	return "GDExtension Helper"
	
func _exit_tree():
	if WorkspaceInstance:
		WorkspaceInstance.queue_free()
