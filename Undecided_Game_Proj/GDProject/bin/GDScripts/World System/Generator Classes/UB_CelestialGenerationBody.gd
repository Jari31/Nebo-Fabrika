extends PCG_Environment
class_name UB_CelestialGenerationBody
## Class that handles the generation of noise-based bodies. (e.g., asteroids, planets, etc.)
##
## It's more in-depth than its base parent, allowing for mesher initialization.
## Though, it is far less flexible than its parent. Intended for terrain meshes based on geometry.
##
## *It is not suited for bodies like stars, black-holes, etc. 

@export var UseLocalRenderingDevice: bool = false

## Name of the uber shader to look up in the ShaderLocation dict.
@export var UberShaderName: String

## Compiled location of the uber shader.
@export var UberShaderLocation: String

@export var MesherShaderName: String

## Compiled location of the meshing algorithm. 
@export var MesherShaderLocation: String

@export var VertexPullShaderName: String

## Vertex pulling spatial GDShader that handles things like triplanar projection, triangle generation, etc.
@export var VertexPullShaderLocation: String

## The (max) vertex count of the terrain. 
@export var MeshVertexCount: int = 2294914

## The (max) verex count for the implicit triangles. 
## (Don't worry about what that means. Just know: 
## reference mesh lower than terrain mesh.)
@export var RefMeshVertexCount: int = 30000

## Dictates how many GPU threads to launchs to emit triangles through index buffers. 
## If too low, there will be holes in the mesh. 
## If too high, then the performance cost will skyrocket. 
## Must be multiples of three, as per triangle primitives.
@export var ThreadCount: int = 33294915 # I have no clue why this specific number works for 256^3 noise grids (matching indices doesn't result in enough threads)
										# and I don't have the time to look at the engine code for Godot to figure it out
										# Matching the index count should work, since thread spawns -> takes index -> spawns vertex
										# BUT GUESS WHAT? IT DOESN'T. BECAUSE WHY SHOULD THING WORK LOGICALLY? FUCK MY CHUD LIF-
										# The rendering device API is quite inconsistent, so this makes sense. Try setting a texture RID in another function. See where that gets you.
## Coefficient for index textures. Each triangle has 3 indices. 
## As the mesh becomes complex, the index count must increase to match it as well. 
## Again, too low and you will see holes in the mesh.
## Too high and VRAM costs will skyrocket.
@export var IndexCoefficient: float = 3.6

## Culling bounding box
@export var BoundingBoxSize_Min: Vector3 = Vector3(0, 0, 0)
@export var BoundingBoxSize_Max: Vector3 = Vector3(256, 256, 256)

## Dictates whether to use a ping-pong (second) buffer to do post processing work.
## Post processing work as in vertex displacement, smoothing, etc.
@export var UsePostProcessVertexBuffer: bool = true

@export var Seed: int = 129
@export var ChunkSize: int = 4
@export var VoxelsPerChunk: int = 64

var RenderingDevice_Local: RenderingDevice

var LoadedShaders: Dictionary

var MeshInstance

var SQRT_MAX_VERTS
var IDX_SQRT_MAX_VERTS

var VertexTexture: RID = RID()
var NormalTexture: RID = RID()
var VertexTexture_B: RID = RID()
var IndexTexture: RID = RID()

var CHUNK_SIZE: PackedInt32Array
var VOXELS_PER_CHUNK: PackedInt32Array

## rids; require manual management
var rendering_server_vertex_texture = RID()
var rendering_server_normal_texture = RID()
var rendering_server_index_texture = RID()

var Material_RID = RID()

@onready var LoDCounts: Array = WorldManager.VisualThreadLoDCounts
@onready var LoDVertexCounts: Array = WorldManager.VisualThreadLoDVertexCount
@export var GenerateLoDs = false

func _init_mesher_textures():
	var TextureFormat = RDTextureFormat.new()
	var TextureView = RDTextureView.new()	

	TextureFormat.height = SQRT_MAX_VERTS
	TextureFormat.width = SQRT_MAX_VERTS
	TextureFormat.format = RenderingDevice.DATA_FORMAT_R32G32B32A32_SFLOAT

	var RenderingDeviceUsage = RenderingDevice.TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice.TEXTURE_USAGE_STORAGE_BIT
	var RenderingDeviceUsage1 = RenderingDevice.TEXTURE_USAGE_CAN_COPY_FROM_BIT | RenderingDevice.TEXTURE_USAGE_CAN_COPY_TO_BIT
	TextureFormat.usage_bits = RenderingDeviceUsage | RenderingDeviceUsage1

	VertexTexture = RenderingDevice_Local.texture_create(TextureFormat, TextureView)
	NormalTexture = RenderingDevice_Local.texture_create(TextureFormat, TextureView)

	if(UsePostProcessVertexBuffer):
		VertexTexture_B = RenderingDevice_Local.texture_create(TextureFormat, TextureView)

	TextureFormat.height = IDX_SQRT_MAX_VERTS
	TextureFormat.width = IDX_SQRT_MAX_VERTS
	TextureFormat.format = RenderingDevice.DATA_FORMAT_R32_SFLOAT

	IndexTexture = RenderingDevice_Local.texture_create(TextureFormat, TextureView)

	rendering_server_vertex_texture = RenderingServer.texture_rd_create(VertexTexture)
	rendering_server_normal_texture = RenderingServer.texture_rd_create(NormalTexture)
	rendering_server_index_texture = RenderingServer.texture_rd_create(IndexTexture)

func ResetChunkSize():
	initCompute(Seed, MeshVertexCount, true,
				CHUNK_SIZE, VOXELS_PER_CHUNK, MeshInstance.get_instance(), IndexCoefficient, 
				WorldManager.VisualThreadLoDVertexCount, RefMeshVertexCount)

func InitMesher(InitMesh: bool):
	RenderingDevice_Local = RenderingServer.get_rendering_device()
	SetSettings(true, UseLocalRenderingDevice, GlobalFlags.DEBUG)
	SetCompiledShaders(LoadedShaders.get(UberShaderName), 
						LoadedShaders.get(MesherShaderName), 
						LoadedShaders.get(VertexPullShaderName))
	
	for i in range(3):
		VOXELS_PER_CHUNK.append(VoxelsPerChunk)
		CHUNK_SIZE.append(ChunkSize)
	
	if InitMesh:
		MeshInstance = MeshInstance3D.new()
		
		var Mesh_Local = ArrayMesh.new()
		var VertexArray = PackedVector3Array()
		VertexArray.resize(ThreadCount)
		VertexArray.fill(Vector3.ZERO)
		
		var MeshArray = []
		MeshArray.resize(Mesh.ARRAY_MAX)
		MeshArray[Mesh.ARRAY_VERTEX] = VertexArray
		
		Mesh_Local.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, MeshArray)
		
		add_child(MeshInstance)
		MeshInstance.mesh = Mesh_Local
		MeshInstance.custom_aabb = AABB(BoundingBoxSize_Min, BoundingBoxSize_Max)
	
	if(GenerateLoDs):
		MeshVertexCount = 0
		var i = 0
		for LoD in LoDCounts:
			MeshVertexCount += LoD * LoDVertexCounts[i]
			i += 1
	SQRT_MAX_VERTS = ceil(sqrt(MeshVertexCount)) # because we can't use buffers. We gotta use textures because idk, ask Godot devs, man.
												 # something something Mobile and Browser support. A vertex texture is something I thought I'd never see    
	IDX_SQRT_MAX_VERTS = ceil(sqrt(MeshVertexCount * IndexCoefficient))
	
	_init_mesher_textures()
	SetRIDStorage(VertexTexture, NormalTexture, VertexTexture_B, IndexTexture)
	initCompute(Seed, MeshVertexCount, true,
				CHUNK_SIZE, VOXELS_PER_CHUNK, MeshInstance.get_instance(), 
				IndexCoefficient, WorldManager.VisualThreadLoDVertexCount, RefMeshVertexCount)

func InitVertexPullShader():
	Material_RID = RenderingServer.material_create()
	RenderingServer.material_set_shader(Material_RID, LoadedShaders.get(VertexPullShaderName))
	RenderingServer.instance_geometry_set_material_override(MeshInstance.get_instance(), Material_RID)
	RenderingServer.material_set_param(Material_RID, "IndexTexture", rendering_server_index_texture)
	RenderingServer.material_set_param(Material_RID, "VertexTexture", rendering_server_vertex_texture)
	RenderingServer.material_set_param(Material_RID, "NormalTexture", rendering_server_normal_texture)
	RenderingServer.material_set_param(Material_RID, "GridSizeIndex", int(IDX_SQRT_MAX_VERTS))
	RenderingServer.material_set_param(Material_RID, "GridSizeVertex", int(SQRT_MAX_VERTS))

func DeInitMesher():
	if(VertexTexture.is_valid()):
		RenderingServer.free_rid(VertexTexture)
		RenderingServer.free_rid(IndexTexture)
		RenderingServer.free_rid(NormalTexture)
		
	if(VertexTexture_B.is_valid()):
		RenderingServer.free_rid(VertexTexture_B)
		
	if(rendering_server_vertex_texture.is_valid()):
		RenderingServer.free_rid(rendering_server_vertex_texture)
	if(rendering_server_normal_texture.is_valid()):
		RenderingServer.free_rid(rendering_server_normal_texture)
	if(rendering_server_index_texture.is_valid()):
		RenderingServer.free_rid(rendering_server_index_texture)
	
	if(Material_RID.is_valid()):
		RenderingServer.free_rid(Material_RID)
	
	RenderingServer.free_rid(RenderingDevice_Local)

func _exit_tree() -> void:
	DeInitMesher()
