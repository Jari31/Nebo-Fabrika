class_name Module_Movement

func ProcessInput_Delta3D(Delta: float, Speed: float) -> Vector3:
	var Velocity = Vector3.ZERO
	
	var input_direction = Input.get_vector("Left", "Right", "Forward", "Backward")
	Velocity += input_direction
	
	return Velocity.normalized() * Speed
