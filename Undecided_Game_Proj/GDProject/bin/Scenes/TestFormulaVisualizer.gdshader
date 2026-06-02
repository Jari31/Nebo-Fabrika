shader_type canvas_item;

void vertex() {
	// Called for every vertex the material is visible on.
}

float smooth_line(float Distance, float LineWidth, float AntiAliasFactor)
{
	float half_width = LineWidth * 0.5;
	return 1.0 - smoothstep(half_width - AntiAliasFactor, half_width + AntiAliasFactor, Distance);
}

void fragment() {
	float jump_velocity = 5.0;
	float gravity = 9.8;
	float peak_damping = 0.5;
	
	vec2 st = UV;
	st.y = 1.0 - st.y;
	
	vec3 color = vec3(0.1, 0.1, 0.11);
	
	float t = st.x;
	
	float curve = 12.0 + gravity * t;
	float line_thickness = 0.01;
	if(abs(st.y - curve) < line_thickness)
	{
		color = vec3(0.0, 1.0, 0.0);
	}
	
	
	
	COLOR = vec4(color, 1.0);
}

//void light() {
//	// Called for every pixel for every light affecting the CanvasItem.
//	// Uncomment to replace the default light processing function with this one.
//}
