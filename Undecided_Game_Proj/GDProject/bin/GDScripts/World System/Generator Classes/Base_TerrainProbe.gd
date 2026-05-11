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

var LoDVertexCounts: Array = WorldManager.VisualThreadLoDVertexCount
var LoDCounts: Array = WorldManager.VisualThreadLoDCounts

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
func _get_index(LoD: int, GetUncountedIndex: bool = false) -> int:
	if(GetUncountedIndex):
		return LastCheckedIndex[LoD]
		
	if LastCheckedIndex[LoD] < LoDCounts[LoD]:
		LastCheckedIndex[LoD] += 1
		return LastCheckedIndex[LoD]
	LastCheckedIndex[LoD] = 0
	return 0

var _job_result: bool = false
func Callback(result: bool):
	_job_result = result
	_semaphore.post()	

func _set_LoD(Serial: String, Index: int, LoD: int):
	var index
	if(LoD > 0):
		index = LoDCounts[LoD - 1] + Index
	else: index = Index
	LoDTracker[Serial] = true
	LoDCounter[index] = Serial
			
func GenerateTerrain(SerialString: String, UpdateTerrain: bool, WorldPosition: Vector3, LoD: int):
	var CurrentIndex
	SCPB_Server.AttemptTaskSubmit(GenerationRequest)
	_semaphore.wait()
	if(_job_result == false):
		printerr("DDA failed. Job data is invalid.")
		TerrainGenerated.emit()
		return
		
	_job_result = false
	var Index
	if(!UpdateTerrain): # update terrain = update prexisting terrain
		Index = _get_index(LoD)
		_set_LoD(SerialString, Index, LoD)
	
	TerrainGenerated.emit()

var LoDTracker: Dictionary
var LoDCounter: Array

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
		if LoDTracker.has(SerialString):
			HIT = true
			GenerateTerrain(SerialString, true, WorldSpaceCoordinate, 0)
			continue
		
		GenerateTerrain(SerialString, false, WorldSpaceCoordinate, 0)
