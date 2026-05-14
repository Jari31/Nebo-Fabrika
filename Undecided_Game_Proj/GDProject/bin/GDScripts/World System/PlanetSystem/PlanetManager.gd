extends RefCounted
class_name Base_PlanetManager
## The building block of actual planets. This class handles multi-threaded generation
## and deals with planet generation logic. This is a base that abstracts away the more
## atomic units.

var NormalTextureArray_RID: RID
var AlbedoTextureArray_RID: RID

var LoadedShaders: Dictionary

var UberShaderName = "UberShader"
var MesherShaderName = "MesherShader"
var VertexPullShaderName = "VertexPullShader"
var TruthGridSize = WorldManager.TRUTH_GRID_SIZE

var BaseVertexCount: float = 150000
var BaseChunkSize: float = 4
var BaseIndexCoefficient: float = WorldManager.BaseTerrainThreadCoefficient

func InitPlanet(VoxelsPerChunk: int, InitMesh: bool, 
				InitTextures: bool = false, AlbedoMaps = [], 
				NormalMaps = []) -> Base_TerrainGenerator:
	var Planet = Base_TerrainGenerator.new()
	Planet.AutoClearRIDs = false
	Planet.LoadedShaders = LoadedShaders
	Planet.UberShaderName = UberShaderName
	Planet.MesherShaderName = MesherShaderName
	Planet.VertexPullShaderName = VertexPullShaderName
	Planet.ChunkSize = BaseChunkSize
	Planet.VoxelsPerChunk = VoxelsPerChunk
	
	var GridOffset = TruthGridSize / (BaseChunkSize * VoxelsPerChunk)
	var VertexCount = ceil(BaseVertexCount / GridOffset)

	var IndexCount = ceil(VertexCount * BaseIndexCoefficient)
	
	#Planet.MeshVertexCount = VertexCount
	Planet.ThreadCount = IndexCount
	#Planet.IndexCoefficient = BaseIndexCoefficient
	
	Planet.InitPlanet(InitTextures, InitMesh, AlbedoMaps, NormalMaps)
	
	AlbedoTextureArray_RID = Planet.AlbedoTextureArray_RID
	print(AlbedoTextureArray_RID)
	
	#AlbedoTextureArray_RID = Planet.AlbedoTextureArray_RID
	
	return Planet
	
func CompileAndSaveShader(UberShaderLocation: String, CompileUShaderTo: String, 
						MesherShaderLocation: String, CompileMShaderTo: String, 
						VertexPullShaderLocation: String,
						SaveShadersTo: String):
	var _shader_loader = ShaderLoader.new()
	
	LoadedShaders[UberShaderName] = _shader_loader.CompileComputeShader(UberShaderLocation, 
								CompileUShaderTo)
	LoadedShaders[MesherShaderName] = _shader_loader.CompileComputeShader(MesherShaderLocation, 
									CompileMShaderTo)
	
	LoadedShaders[VertexPullShaderName] = ResourceLoader.load(VertexPullShaderLocation)
	
	var ShaderLocations: Dictionary
	
	ShaderLocations[UberShaderName] = CompileUShaderTo
	ShaderLocations[MesherShaderName] = CompileMShaderTo
									
	_shader_loader.SaveShaderLocations(SaveShadersTo, ShaderLocations)

func LoadComputeShaders(SaveShadersTo: String):
	var _shader_loader = ShaderLoader.new()
	_shader_loader.LoadComputeShaderLocations(SaveShadersTo, LoadedShaders)
	
#func LoadTextures(p_AlbedoMaps: Array, p_NormalMaps: Array):
	#var _shader_loader = ShaderLoader.new()
	#NormalTextureArray_RID = _shader_loader.Load2DTextureArray(NormalMaps)
	#AlbedoTextureArray_RID = _shader_loader.Load2DTextureArray(AlbedoMaps)
	
	#AlbedoMaps = p_AlbedoMaps
	#NormalMaps = p_NormalMaps
	
func GenerateTerrain(Planet: Base_TerrainGenerator):
	Planet.GenerateTerrain(Vector3(0, 0, 0))
	return true

func _notification(what: int) -> void:
	if(what == NOTIFICATION_PREDELETE):
		if(NormalTextureArray_RID.is_valid()):
			RenderingServer.free_rid(NormalTextureArray_RID)
		if(AlbedoTextureArray_RID.is_valid()):
			RenderingServer.free_rid(AlbedoTextureArray_RID)
