extends Node3D
class_name VisualGenerationManager

@export var CompileShaders: bool = true

var UberShaderLocation = "res://bin/Shaders/Compute Shaders/Environment Generation GLSL/test_planet.glsl"
var CompileUShaderTo = "res://bin/Shaders/Compute Shaders/Compiled/test_planet.spv"

var MesherShaderLocation = "res://bin/Shaders/Compute Shaders/Libs/Dual Contouring/DualContouring.glsl"
var CompileMesherShaderTo = "res://bin/Shaders/Compute Shaders/Compiled/mesher_shader.spv"

var VertexPullShaderLocation = "res://bin/Shaders/Spatial Shaders/VertexPull.gdshader"

var SaveShadersTo = "res://bin/Environments/Development Only/comp_shaders.dat"

var AlbedoMaps = ["res://bin/Environments/Development Only/Textures/rock_face_03_diff_1k.jpg", "res://bin/Environments/Development Only/Textures/rock_face_03_diff_1k.jpg"]
var NormalMaps = ["res://bin/Environments/Development Only/Textures/rock_face_03_nor_gl_1k.png", "res://bin/Environments/Development Only/Textures/rock_face_03_nor_gl_1k.png"]

var Active_Visual_Thread: Base_Planet
var Passive_Visual_Thread: Base_Planet

@onready var PlanetManager = Base_PlanetManager.new()

var Camera: Camera3D
var TerrainProbe: Base_TerrainProbe
	
func _ready():
	Camera = self.get_parent()
	if(CompileShaders):
		PlanetManager.CompileAndSaveShader(UberShaderLocation, CompileUShaderTo,
										MesherShaderLocation, CompileMesherShaderTo, 
										VertexPullShaderLocation,
										SaveShadersTo)
	else:
		PlanetManager.LoadComputeShaders(SaveShadersTo)
	
	Active_Visual_Thread = PlanetManager.InitPlanet(64, true, true, AlbedoMaps, NormalMaps)
	Passive_Visual_Thread = PlanetManager.InitPlanet(32, true)
	add_child(Active_Visual_Thread)
	
	TerrainProbe = Base_TerrainProbe.new()
	TerrainProbe.InitDDA(PlanetManager.GenerateTerrain, Camera)
	TerrainProbe.InitMeshRingBuffer(ResourceLoader.load(VertexPullShaderLocation))
	
func _result(_x):
	print("Terrain generated.")
	
func _input(event):
	if event is InputEventKey and event.is_pressed():
		if event.keycode == KEY_F1:
			var Wrapper = GenerationThreadWrapper.new()
			Wrapper.Init(Active_Visual_Thread.GenerateTerrain, _result, 0)
			SCPB_Server.PriorityTaskSubmit(Wrapper)
			print(PlanetManager.AlbedoTextureArray_RID)

var _terrain_probe_occupied: bool = false

func _on_terrain_generated():
	print("Terrain generated.")
	_terrain_probe_occupied = false
	
func _dispatch_generation():
	if not _terrain_probe_occupied:
		TerrainProbe.TerrainGenerated.connect(_on_terrain_generated)
		TerrainProbe.RaymarchDDA()
		_terrain_probe_occupied = true
		
func _process(_delta: float):
	_dispatch_generation()
