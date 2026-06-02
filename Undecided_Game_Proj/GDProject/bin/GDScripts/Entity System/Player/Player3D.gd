extends CharacterBody3D
class_name Player3D

const SERVER_POS_LERP_SPEED = 10
const SERVER_ROT_LERP_SPEED = 5
const SERVER_SNAP_CORRECTION_DISTANCE = 5
const MOVEMENT_SPEED = 2

var PlayerID: int = 0

var Camera: Camera3D
var CameraModule: Module_Camera

@export var PlayerRotation = Vector3.ZERO

var VisualState: bool = false

var MovementModule: Module_Movement
@export var GlobalPosition: Vector3 = Vector3(0,0,0)

var SpoofedRPC_Count: int = 0
var MaxSpeed: float = MOVEMENT_SPEED + 10

@export var Server: NetworkManager

func _ready():
	if DisplayServer.get_name() == "headless":
		$Visuals.queue_free()
	else:
		_setup_visuals()
	
	Camera = $PlayerCamera
	
	CameraModule = Module_Camera.new()
	CameraModule.CaptureMouse()	
	
	MovementModule = Module_Movement.new()
	
	PlayerID = name.to_int()

	set_multiplayer_authority(PlayerID)

func _setup_visuals():
	VisualState = true

func _input(Event) -> void:
	if Event is InputEventMouseMotion:
		if PlayerID != 1: CameraModule.ProcessMouseInput_3D(Event, Camera, self)
		rpc_id(1, "_server_process_camera", Event)
		
	if Event is InputEventMouseButton:
		if Event.button_index == MOUSE_BUTTON_LEFT and Event.pressed:
			CameraModule.CaptureMouse()
	
	if Event.is_action_pressed("Escape"):
		CameraModule.EscapeMouse()
		
	if Event.is_action_pressed("DEBUG_JoinServer"):
		Server.StartClient(Server.Address, Server.Port)
			
func _physics_process(Delta: float) -> void:
	if is_multiplayer_authority() and DisplayServer.get_name() != "headless":
		_client_process_input(Delta)
	
	if DisplayServer.get_name() != "headless":
		move_and_slide()

func _client_process_input(Delta: float):
	if is_multiplayer_authority():
		var direction = MovementModule.CaptureInputs() 
		
		if PlayerID != 1: _move_player(Delta, direction)
		
		rpc_id(1, "_server_process_input", direction, Delta)
		var distance_to_server_truth = global_position.distance_to(GlobalPosition)
		if distance_to_server_truth < SERVER_SNAP_CORRECTION_DISTANCE:
			global_position = global_position.lerp(GlobalPosition, Delta * MOVEMENT_SPEED)
		else:
			global_position = GlobalPosition
		
		return
	
	global_position = global_position.lerp(GlobalPosition, Delta * SERVER_POS_LERP_SPEED)
	if not is_multiplayer_authority(): rotation = rotation.lerp(PlayerRotation, Delta * SERVER_ROT_LERP_SPEED)


func _check_ownership() -> bool:
	if multiplayer.get_remote_sender_id() != get_multiplayer_authority() || not multiplayer.is_server():
		return false
	return true

func _move_player(Delta: float, Direction: Vector3, Speed: float = 2):
	velocity = MovementModule.ProcessInput_Horizontal_Delta3D(Delta, Camera.global_transform, Direction, Speed)
	velocity += MovementModule.ProcessJump_Delta_Character3D(Delta, self, Direction)
	
	move_and_slide()

@rpc("any_peer", "call_local", "unreliable")
func _server_process_input(Direction: Vector3, Delta: float, Speed: float = 2):
	if not _check_ownership(): return 
	
	_move_player(Delta, Direction, Speed)
	
	GlobalPosition = position
	
@rpc("any_peer", "call_local", "unreliable")
func _server_process_camera(Event):
	if not _check_ownership(): return
	
	CameraModule.ProcessMouseInput_3D(Event, Camera, self)
	
	if multiplayer.is_server(): PlayerRotation = global_rotation 
