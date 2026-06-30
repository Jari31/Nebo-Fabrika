@tool
extends VBoxContainer

@export var ChildPanel: Panel
@export var ChildButton: Button

@export var IconOpen: Texture2D
@export var IconClosed: Texture2D

var IsOpen: bool = false
var tween: Tween

func _ready() -> void:
	ChildPanel.offset_transform_enabled = true
	ChildPanel.offset_transform_visual_only = false
	ChildPanel.resized.connect(_on_child_panel_resized)
	ChildButton.pressed.connect(_on_button_pressed)
	

func _on_child_panel_resized():
	ChildPanel.offset_transform_position_ratio = Vector2(0.0, 1.0)
	#ChildPanel.modulate.a = 0.0
	ChildButton.icon = IconClosed
	ChildPanel.resized.disconnect(_on_child_panel_resized)
	
func _on_button_pressed():
	if tween:
		tween.kill()
	
	tween = create_tween();
	
	if not IsOpen:
		IsOpen = true
		tween.tween_property(ChildPanel, "offset_transform_position_ratio"\
		, Vector2.ZERO, 0.5)\
		.set_trans(Tween.TRANS_CUBIC)\
		.set_ease(Tween.EASE_OUT)
		
		ChildButton.icon = IconOpen
	else:
		IsOpen = false
		tween.tween_property(ChildPanel, "offset_transform_position_ratio"\
		, Vector2(0, 1.0), 0.5)\
		.set_trans(Tween.TRANS_CUBIC)\
		.set_ease(Tween.EASE_IN)

		ChildButton.icon = IconClosed
