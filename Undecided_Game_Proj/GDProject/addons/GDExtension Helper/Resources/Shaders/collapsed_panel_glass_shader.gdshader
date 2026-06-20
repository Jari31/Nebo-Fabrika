shader_type canvas_item;

uniform sampler2D ScreenTexture : hint_screen_texture, filter_linear_mipmap;

uniform float BlurAmount : hint_range(0.0, 5.0) = 2.5;
uniform vec4 GlassTint : source_color = vec4(1.0, 1.0, 1.0, 0.1);
uniform vec4 EdgeColor : source_color = vec4(1.0, 1.0, 1.0, 0.4);
uniform vec4 EdgeMasks_1 : source_color = vec4(0.0, 0.02, 1.0, 0.98);
uniform vec4 EdgeMasks_2 : source_color = vec4(0.0, 0.02, 1.0, 0.98);

void vertex() {
	// Called for every vertex the material is visible on.
}

void fragment() {
	vec4 default_texture = texture(TEXTURE, UV);
	
	vec4 background = textureLod(ScreenTexture, SCREEN_UV, BlurAmount);
	vec4 glass_base = mix(background, GlassTint, GlassTint.a);
	
	vec2 border = smoothstep(vec2(EdgeMasks_1.x, EdgeMasks_2.x), 
				vec2(EdgeMasks_1.y, EdgeMasks_2.y), UV) 
				* smoothstep(vec2(EdgeMasks_1.z, EdgeMasks_2.z), 
				vec2(EdgeMasks_1.w, EdgeMasks_2.w), UV);
    float edge_mask = 1.0 - (border.x * border.y);
    
    COLOR = mix(glass_base, EdgeColor, edge_mask * EdgeColor.a);
}

//void light() {
//	// Called for every pixel for every light affecting the CanvasItem.
//	// Uncomment to replace the default light processing function with this one.
//}
