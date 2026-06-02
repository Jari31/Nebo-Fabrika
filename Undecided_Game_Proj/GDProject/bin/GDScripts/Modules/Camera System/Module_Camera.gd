class_name Module_Camera
extends RefCounted

var MouseEscaped: bool = true
var Sensitivity: float = 0.008

var CameraBounds_Degrees: Vector2 = Vector2(-80.0, 80.0)

func CaptureMouse():
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	MouseEscaped = false
	
func EscapeMouse():
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	MouseEscaped = true

func ProcessMouseInput_3D(Event, Camera: Camera3D, ParentMesh):
	if(MouseEscaped): return
		
	ParentMesh.rotate_y(-Event.relative.x * Sensitivity)
	Camera.rotate_x(-Event.relative.y * Sensitivity)
	
	Camera.rotation.x = clamp(Camera.rotation.x, 
		deg_to_rad(CameraBounds_Degrees.x), deg_to_rad(CameraBounds_Degrees.y))
