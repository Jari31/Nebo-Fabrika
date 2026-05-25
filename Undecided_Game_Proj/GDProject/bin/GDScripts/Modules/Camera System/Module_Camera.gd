class_name Module_Camera

var IsInitialized: bool = false
var Sensitivity: float = 0.008

var CameraBounds_Degrees: Vector2 = Vector2(-80.0, 80.0)

func Initialize():
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	IsInitialized = true

func InputProcessMouse_3D(Camera: Camera3D, Event):
	if(!IsInitialized):
		print("The camera has not been initialized. Exiting...")
		return
	
	if Event is InputEventMouseMotion:
		Camera.rotate_x(-Event.relative.y * Sensitivity)
		Camera.rotate_y(-Event.relative.x * Sensitivity)
		
		Camera.rotation.x = clamp(Camera.rotation.x, 
			deg_to_rad(CameraBounds_Degrees.x), deg_to_rad(CameraBounds_Degrees.y))
