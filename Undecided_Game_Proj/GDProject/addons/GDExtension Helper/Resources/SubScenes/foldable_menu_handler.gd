extends FoldableContainer

@export var ChildPanel: Panel
@export var ChildVBox: VBoxContainer

var IsOpen: bool = false;
var tween: Tween

func toggle_container() -> void:
	if tween and tween.is_running():
		tween.kill()
		
	tween = create_tween().set_parallel(false)
	
	if not IsOpen:
		
