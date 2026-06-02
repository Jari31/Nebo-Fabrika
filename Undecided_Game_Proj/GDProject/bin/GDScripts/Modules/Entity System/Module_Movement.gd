class_name Module_Movement
extends RefCounted

func ProcessInput_Horizontal_Delta3D(Delta: float, CameraTransform, Direction, Speed: float = 1) -> Vector3:
	var Velocity = Vector3.ZERO
	
	var camera_basis = CameraTransform.basis
	var forward = camera_basis.z.normalized()
	var right = camera_basis.x.normalized()
	
	forward.y = 0
	right.y = 0
	forward = forward.normalized()
	right = right.normalized()
	
	Direction = (forward * Direction.z) + (right * Direction.x)

	if Direction:
		Velocity.x = Direction.x * Speed
		Velocity.z = Direction.z * Speed
	else:
		Velocity.x = move_toward(Velocity.x, 0, Speed)
		Velocity.z = move_toward(Velocity.z, 0, Speed)
		
	return Velocity

func ProcessJump_Delta_Character3D(Delta: float, Character: CharacterBody3D, Direction: Vector3, 
				JumpHeight: Vector3 = Vector3(0, 300.0, 0), GravityMultiplier: float = 12) -> Vector3:
	var Velocity: Vector3 = Vector3.ZERO
	var gravity = Character.get_gravity() * GravityMultiplier
	if not Character.is_on_floor():
		if Character.velocity.y < 100:
			Velocity += gravity * Delta
		else:
			Velocity += gravity * Delta * 10.0
		return Velocity
	
	if Character.is_on_floor() and Direction.y != 0:
		Velocity = JumpHeight

	return Velocity
	
func CaptureInputs():
	var input_direction = Input.get_vector("Left", "Right", "Forward", "Backward")
	var jump = 0
	if Input.is_action_just_pressed("Jump"):
		jump = 1
		
	var Direction = Vector3(input_direction.x, jump, input_direction.y)

	return Direction
