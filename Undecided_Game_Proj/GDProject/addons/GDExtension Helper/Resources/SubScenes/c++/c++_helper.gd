@tool
extends Control

#@export var ClassName: String
#@export var IsEditorPlugin: bool
#@export var GenerateMetaFileBoilerplate: bool

@export var Node_GenerateBoilerplate: Button 

@export var Node_ClassName: LineEdit
@export var Node_GodotVersion: SpinBox
@export var Node_IsEditorPlugin: CheckButton
@export var Node_GenerateMetaFileBoilerplate: CheckButton

func _ready() -> void:
	Node_GenerateBoilerplate.pressed.connect(_generate_boilerplate)
	
func _generate_boilerplate():
	
