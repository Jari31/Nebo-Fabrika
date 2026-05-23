extends Node3D


func _ready() -> void:
	var ShaderComp = ShaderCompiler.new()
	var UberShaderLocation = "res://bin/Shaders/Compute Shaders/Environment Generation GLSL/UberShaderTemplate.glsl"
	var CompositionShaderLocation = "res://bin/Shaders/Compute Shaders/Environment Generation GLSL/Biomes/Compositions/Biome_Compositon_Template.glsl"
	print(ShaderComp.PreprocessUberShader(UberShaderLocation, CompositionShaderLocation, "", ""))
