extends Camera3D

@export var move_speed: float = 10.0
@export var mouse_sensitivity: float = 0.002

var rotation_x: float = 0.0
var rotation_y: float = 0.0

func _ready():
	# Hides the damn mouse so it don't get in the way
	Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)

func _input(event):
	# Lookin' around logic
	if event is InputEventMouseMotion:
		rotation_x -= event.relative.y * mouse_sensitivity
		rotation_y -= event.relative.x * mouse_sensitivity
		
		# Clamp the vertical look so you don't snap your neck
		rotation_x = clamp(rotation_x, deg_to_rad(-85), deg_to_rad(85))
		
		transform.basis = Basis.from_euler(Vector3(rotation_x, rotation_y, 0))

func _process(delta):
	var input_dir = Vector3.ZERO
	
	# Basic WASD movement
	if Input.is_key_pressed(KEY_W):
		input_dir -= transform.basis.z
	if Input.is_key_pressed(KEY_S):
		input_dir += transform.basis.z
	if Input.is_key_pressed(KEY_A):
		input_dir -= transform.basis.x
	if Input.is_key_pressed(KEY_D):
		input_dir += transform.basis.x
		
	if input_dir != Vector3.ZERO:
		input_dir = input_dir.normalized()
		global_position += input_dir * move_speed * delta

	# Let the cursor out if you hit escape
	if Input.is_key_pressed(KEY_ESCAPE):
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
