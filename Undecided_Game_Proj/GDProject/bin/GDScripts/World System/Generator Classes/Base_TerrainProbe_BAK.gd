'''
extends Node
class_name Base_TerrainProbe

var Camera: Camera3D
var RayDensity: int = 8
var MAX_DISTANCE: int = 25600

signal TerrainGenerated

var CHUNK_SIZE: float = WorldManager.TRUTH_GRID_SIZE

var VertexTexture_RID: RID 
var IndexTexture_RID: RID
var NormalTexture_RID: RID

#var _thread: Thread
var _semaphore: Semaphore

class TerrainObject:
	extends MeshInstance3D
	
	@onready var RenderingDevice_Local: RenderingDevice = RenderingServer.get_rendering_device()
	
	var VertexTexture: RID
	var IndexTexture: RID
	var VertexTexture_B: RID
	var NormalTexture: RID
	
	var rendering_server_vertex_texture = RID()
	var rendering_server_normal_texture = RID()
	var rendering_server_index_texture = RID()
	
	var NormalHTextureArray_RID: RID
	var AlbedoATextureArray_RID: RID
	var REAMTextureArray_RID: RID
	
	var Material_RID: RID
	
	var SQRT_MAX_VERTS
	var IDX_SQRT_MAX_VERTS
	
	var BoundingBoxSize_Min: Vector3 = Vector3(0, 0, 0)
	var BoundingBoxSize_Max: Vector3 = Vector3(256, 256, 256)
	var BaseTerrainThreadCoefficient: float = WorldManager.BaseTerrainThreadCoefficient
	
	func InitVertexPullShader(VertexPullShader: Shader):
		Material_RID = RenderingServer.material_create()
		RenderingServer.material_set_shader(Material_RID, VertexPullShader)
		RenderingServer.instance_geometry_set_material_override(self.get_instance(), Material_RID)
		RenderingServer.material_set_param(Material_RID, "IndexTexture", rendering_server_index_texture)
		RenderingServer.material_set_param(Material_RID, "VertexTexture", rendering_server_vertex_texture)
		RenderingServer.material_set_param(Material_RID, "NormalTexture", rendering_server_normal_texture)
		RenderingServer.material_set_param(Material_RID, "GridSizeIndex", int(IDX_SQRT_MAX_VERTS))
		RenderingServer.material_set_param(Material_RID, "GridSizeVertex", int(SQRT_MAX_VERTS))
	
	func Init(VertexAmount: int, IndexCoefficient: float = 3.6, 
			UsePostProcessVertexBuffer: bool = true):
		SQRT_MAX_VERTS = ceil(sqrt(VertexAmount))
		IDX_SQRT_MAX_VERTS = ceil(sqrt(VertexAmount * IndexCoefficient)) 
		
		var Mesh_Local = ArrayMesh.new()
		var VertexArray = PackedVector3Array()
		VertexArray.resize(ceil(VertexAmount * BaseTerrainThreadCoefficient))
		VertexArray.fill(Vector3.ZERO)
		
		var MeshArray = []
		MeshArray.resize(Mesh.ARRAY_MAX)
		MeshArray[Mesh.ARRAY_VERTEX] = VertexArray
		
		Mesh_Local.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, MeshArray)
		
		self.mesh = Mesh_Local
		self.custom_aabb = AABB(BoundingBoxSize_Min, BoundingBoxSize_Max)
		
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
	
	func UpdateTextures(VertexTexture_RID: RID, IndexTexture_RID: RID, NormalTexture_RID: RID):
		var offset = Vector3(0, 0, 0)
		var region = Vector3(SQRT_MAX_VERTS, SQRT_MAX_VERTS, 1)
		
		RenderingDevice_Local.texture_copy(
		VertexTexture_RID, VertexTexture, 
		offset, offset, region, 
		0, 0, 0, 0)
		RenderingDevice_Local.texture_copy(
		NormalTexture_RID, NormalTexture, 
		offset, offset, region, 
		0, 0, 0, 0)
		
		region = Vector3(IDX_SQRT_MAX_VERTS, IDX_SQRT_MAX_VERTS, 1)
		RenderingDevice_Local.texture_copy(
		IndexTexture_RID, IndexTexture, 
		offset, offset, region, 
		0, 0, 0, 0)
	
	func _load_image_array(Paths: Array) -> Array:
		var ImageArray = []
		
		for Path in Paths:
			var texture = ResourceLoader.load(Path)
			texture = texture.get_image()
			if(texture.get_format() != 5):
				texture.convert(Image.FORMAT_RGBA8)
			if(texture):
				ImageArray.append(texture)
		
		if(ImageArray.is_empty()):
			printerr("The images provided do not exist.")
			return []

		return ImageArray
	
	var AlbedoTextureArray_RID: RID
	var NormalTextureArray_RID: RID
	
	func InitVisualTextures(InitTextures: bool, AlbedoAMaps, NormalHMaps):
		if(InitTextures):
			AlbedoTextureArray_RID = RenderingServer.texture_2d_layered_create(_load_image_array(AlbedoAMaps), RenderingServer.TEXTURE_LAYERED_2D_ARRAY)
			NormalTextureArray_RID = RenderingServer.texture_2d_layered_create(_load_image_array(NormalHMaps), RenderingServer.TEXTURE_LAYERED_2D_ARRAY)
			
		RenderingServer.material_set_param(Material_RID, "AlbedoTextures", AlbedoTextureArray_RID)
		RenderingServer.material_set_param(Material_RID, "NormalTextures", NormalTextureArray_RID)
		
	func _exit_tree() -> void:
		if(VertexTexture.is_valid()):
			RenderingServer.free_rid(VertexTexture)
			RenderingServer.free_rid(IndexTexture)
			RenderingServer.free_rid(NormalTexture)	
		if(VertexTexture_B.is_valid()):
			RenderingServer.free_rid(VertexTexture_B)
			
		if(AlbedoATextureArray_RID.is_valid()):
			RenderingServer.free_rid(AlbedoATextureArray_RID)
			RenderingServer.free_rid(NormalHTextureArray_RID)
			
		if(rendering_server_vertex_texture.is_valid()):
			RenderingServer.free_rid(rendering_server_vertex_texture)
		if(rendering_server_normal_texture.is_valid()):
			RenderingServer.free_rid(rendering_server_normal_texture)
		if(rendering_server_index_texture.is_valid()):
			RenderingServer.free_rid(rendering_server_index_texture)

			
var MeshRingBuffer_LoD0: Dictionary # hi-fi
var MeshRingBuffer_LoD0_Ref: Array
var MeshRingBuffer_LoD1: Dictionary # mid-fi
var MeshRingBuffer_LoD1_Ref: Array
var MeshRingBuffer_LoD2: Dictionary # low-fi
var MeshRingBuffer_LoD2_Ref: Array

var LoDVertexCounts: Array = [60000]
var LoDCounts: Array = [4, 32, 64]

func InitMeshRingBuffer(VertexPullShader: Shader):
	for i in range(LoDCounts[0]):
		var HighResObject =  TerrainObject.new()
		HighResObject.Init(LoDVertexCounts[0])
		HighResObject.InitVertexPullShader(VertexPullShader)
		MeshRingBuffer_LoD0[i] = HighResObject

func SetRingBuffer(LoD: int, Serial: String, Value: TerrainObject, Index: int):
	match LoD:
		0:
			MeshRingBuffer_LoD0[Serial] = Value
			MeshRingBuffer_LoD0_Ref[Index] = Serial
		1:
			MeshRingBuffer_LoD1[Serial] = Value
			MeshRingBuffer_LoD1_Ref[Index] = Serial
		2:
			MeshRingBuffer_LoD2[Serial] = Value
			MeshRingBuffer_LoD2_Ref[Index] = Serial
		
func GetRingBuffer(LoD: int, Index: int) -> TerrainObject:
	var Value: TerrainObject = null
	var Serial: String
	match LoD:
		0:
			Serial = MeshRingBuffer_LoD0_Ref[Index]
			Value = MeshRingBuffer_LoD0[Serial]
		1:
			Serial = MeshRingBuffer_LoD1_Ref[Index]
			Value = MeshRingBuffer_LoD1[Serial]
		2:
			Serial = MeshRingBuffer_LoD2_Ref[Index]
			Value = MeshRingBuffer_LoD2[Serial]
	return Value 

var GenerationRequest: GenerationThreadWrapper
func InitDDA(GPU_QueryFunc: Callable, p_Camera: Camera3D):
	#_thread = Thread.new()
	_semaphore = Semaphore.new()
	Camera = p_Camera
	GenerationRequest = GenerationThreadWrapper.new()
	GenerationRequest.Func = GPU_QueryFunc
	GenerationRequest.CallbackFunc = Callback
	
func _ready():
	pass
	
func _get_ray_direction(camera_origin: Vector3, screen_pos: Vector2, inv_view_proj: Projection) -> Vector3:
	var farf = inv_view_proj * Vector4(screen_pos.x, -screen_pos.y, 1, 1)
	var world_point = Vector3(farf.xyz) / farf.w
	return (world_point - camera_origin).normalized()

func _calculate_side_distances(Cell: float, RayOriginAxis: float) -> float:
	if RayOriginAxis < 0:
		return RayOriginAxis / CHUNK_SIZE - Cell
	return Cell + 1.0 - RayOriginAxis / CHUNK_SIZE
	
var LastCheckedIndex: Array = [0, 0, 0]
func _get_index(LoD: int) -> int:
	if LastCheckedIndex[LoD] < LoDCounts[LoD]:
		LastCheckedIndex[LoD] += 1
		return LastCheckedIndex[LoD]
	LastCheckedIndex[LoD] = 0
	return 0

var _job_result: bool = false
func Callback(result: bool):
	_job_result = result
	_semaphore.post()	

func GenerateTerrain(SerialString: String, UpdateTerrain: bool, WorldPosition: Vector3):
	var CurrentIndex
	SCPB_Server.AttemptTaskSubmit(GenerationRequest)
	_semaphore.wait()
	if(_job_result == false):
		printerr("DDA failed. Job data is invalid.")
		TerrainGenerated.emit()
		return
		
	_job_result = false
	var TerrainMesh: TerrainObject
	if(UpdateTerrain): # update terrain = update prexisting terrain
		TerrainMesh = MeshRingBuffer_LoD0[SerialString]
	else:
		CurrentIndex = _get_index(0)
		TerrainMesh = GetRingBuffer(0, CurrentIndex)
	
	TerrainMesh.UpdateTextures(VertexTexture_RID, 
	IndexTexture_RID, NormalTexture_RID)
	
	if(!UpdateTerrain):
		TerrainMesh.position = WorldPosition
	
	TerrainGenerated.emit()

func RaymarchDDA():
	var InverseViewProjection = Camera.get_camera_projection() * Projection(Camera.get_camera_transform().affine_inverse())
	InverseViewProjection = InverseViewProjection.inverse()

	var ScreenSpacePosition = Vector2(
		pow(randf_range(-1, 1), 3),
		pow(randf_range(-1, 1), 3)
	)
	
	var RayOrigin = Camera.get_camera_transform().origin
	var RayDirection = _get_ray_direction(RayOrigin, ScreenSpacePosition, InverseViewProjection)

	var Cell = (RayOrigin / CHUNK_SIZE).floor()
	var StepDirection = Vector3(sign(RayDirection.x), sign(RayDirection.y), sign(RayDirection.z))
	
	var Delta = Vector3(
		abs(CHUNK_SIZE / RayDirection.x) if RayDirection.x != 0 else INF,
		abs(CHUNK_SIZE / RayDirection.y) if RayDirection.y != 0 else INF,
		abs(CHUNK_SIZE / RayDirection.z) if RayDirection.z != 0 else INF
	)

	var SideDistance = Vector3(
		_calculate_side_distances(Cell.x, RayOrigin.x),
		_calculate_side_distances(Cell.y, RayOrigin.y),
		_calculate_side_distances(Cell.z, RayOrigin.z)
	) * Delta
	
	var DistanceTraveled = 0.0
	
	var HIT: bool = false
	
	while !HIT && DistanceTraveled < MAX_DISTANCE:
		if SideDistance.x < SideDistance.y && SideDistance.x < SideDistance.z:
			SideDistance.x += Delta.x
			Cell.x += StepDirection.x
		elif SideDistance.y < SideDistance.z:
			SideDistance.y += Delta.x
			Cell.y += StepDirection.y
		else:	
			SideDistance.z += Delta.z
			Cell.z += StepDirection.z
			
		DistanceTraveled = len(SideDistance)
		var WorldSpaceCoordinate = Cell * CHUNK_SIZE
		
		var SerialString = str(floor(Cell.x)) + "_" + str(floor(Cell.y)) + "_" + str(floor(Cell.z))
		
		GenerationRequest.Priority = 1
		if MeshRingBuffer_LoD0.has(SerialString):	
			HIT = true
			GenerateTerrain(SerialString, true, WorldSpaceCoordinate)
			continue
		
		GenerateTerrain(SerialString, false, WorldSpaceCoordinate)
'''
