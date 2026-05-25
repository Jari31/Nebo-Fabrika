extends Node3D

var Port = 29013
var Address = "127.0.0.1" 

@export var scene: PackedScene

func _ready() -> void:
	if DisplayServer.get_name() == "headless":
		print("Server launched in headless mode, attempting startup sequence...")
		_init_server()
		
func _init_server() -> void:
	var peer = ENetMultiplayerPeer.new()
	var err = peer.create_server(Port)
	if err != OK:
		print("Server failed to launch. Exiting with error: ", err)
		return
	
	multiplayer.multiplayer_peer = peer
	multiplayer.peer_connected.connect(_on_player_connected)
	multiplayer.peer_disconnected.connect(_on_player_disconnected)
	
	print("Server bootup process successful. Port: ", Port)

func StartClient() -> void:
	var peer = ENetMultiplayerPeer.new()
	var err = peer.create_client(Address, Port)
	if err != OK:
		print("Failed to connect to server with error code: ", err)
		return
	
	multiplayer.multiplayer_peer = peer
	print("Connecting to server...")

func _on_player_connected(ID: int) -> void:
	print("Player connected, ID: ", ID)
	
	var player = scene.instantiate()
	player.name = str(ID)
	player.set_multiplayer_authority(ID)
	add_child(player)

func _on_player_disconnected(ID: int) -> void:
	print("Player disconnected, ID: ", ID)
	
	var player = get_node_or_null(str(ID))
	if player:
		player.queue_free()
