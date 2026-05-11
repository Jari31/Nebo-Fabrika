extends PCG_Environment

var CurrentEntityLocation = PackedInt32Array()
var CurrentPlanetLocation = PackedInt32Array()
var CHUNK_SIZE = PackedInt32Array()
var VOXELS_PER_CHUNK = PackedInt32Array()
var MCLUT_FileName = PackedStringArray()

var Ready = false

var rendering_server_vertex_texture = RID()
var rendering_server_normal_texture = RID()
var rendering_server_index_texture = RID()

var Material_RID = RID()
var AlbedoTexture_RID = RID()
var NormalTexture_RID = RID()

@onready var MeshInstance = MeshInstance3D.new()

func AppendImagesArray(TextureArray, TexturePaths):
	for path in TexturePaths:
		var texture = ResourceLoader.load(path)
		if texture:
			TextureArray.append(texture.get_image())
	return TextureArray
			
func _ready():
	var MAXVERTS = 3999999
	var IDX_COEFFICIENT = 2.3;
	var MAXINDICES = ceil(MAXVERTS * IDX_COEFFICIENT);
	var SQRT_MAX_VERTS = ceil(sqrt(MAXVERTS))
	var IDX_SQRT_MAX_VERTS = ceil(sqrt(MAXINDICES))
	
	SetSettings(true, false, true)
	var ShaderCompObject = ShaderCompiler.new()
	var ShaderCompObject1 = ShaderCompiler.new()
	
	var CompileDense = true
	var CompilePlanet = true
	
	var ShaderComp_DEBUG = true
	var Workgroups = Vector3i(8, 8, 8)
	var RenderingDevice_Local = GetLocalRenderingDeviceRID()
	
	var PathToComputeShader = "res://bin/Shaders/Compute Shaders/Libs/Dual Contouring/DualContouring.glsl"
	var CompileTo = "res://bin/Shaders/Compute Shaders/Compiled/dual_contouring.spv"
	
	var DualContouring_Compiled = ShaderCompObject.LoadOrCompileShader(PathToComputeShader, CompileTo, CompileDense,
									RenderingDevice_Local, WorldManager.ShaderStages.COMPUTE_SHADER, 
									Workgroups, ShaderComp_DEBUG)
	
	PathToComputeShader = "res://bin/Shaders/Compute Shaders/Environment Generation GLSL/test_planet.glsl"
	CompileTo = "res://bin/Shaders/Compute Shaders/Compiled/test_planet.spv"
	
	var TestPlanet_Compiled = ShaderCompObject1.LoadOrCompileShader(PathToComputeShader, CompileTo, CompilePlanet,
									RenderingDevice_Local, WorldManager.ShaderStages.COMPUTE_SHADER, 
									Workgroups, ShaderComp_DEBUG)
									

	MCLUT_FileName.append("")
	for i in range(3):
		CurrentEntityLocation.append(0)
		CurrentPlanetLocation.append(0)
		CHUNK_SIZE.append(4)
		VOXELS_PER_CHUNK.append(64)
	var VP_Shader = ResourceLoader.load("res://bin/Shaders/Spatial Shaders/VertexPull.gdshader") as Shader;
	if not VP_Shader:
		print("nahhh you jit tweakin. ain't nothin there lil fella")
		return;
	
	#mesh setup
	var Mesh_local = ArrayMesh.new()
	var Vertices = PackedVector3Array()
	Vertices.resize(MAXINDICES)
	Vertices.fill(Vector3.ZERO)
	
	#var Indices = PackedInt32Array()
	#Indices.resize(MAXVERTS)
	#for i in range(MAXVERTS):
	#	Indices[i] = i
	
	var MeshArray = []
	MeshArray.resize(Mesh.ARRAY_MAX)
	MeshArray[Mesh.ARRAY_VERTEX] = Vertices
	#MeshArray[Mesh.ARRAY_INDEX] = Indices
	
	Mesh_local.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, MeshArray)
	
	add_child(MeshInstance)
	MeshInstance.mesh = Mesh_local
	MeshInstance.custom_aabb = AABB(Vector3(-1000, -1000, -1000), Vector3(2000, 2000, 2000))
	
	#textures (moved from C++ for better modularity)
	var TextureFormat = RDTextureFormat.new()
	var TextureView = RDTextureView.new()

	TextureFormat.height = SQRT_MAX_VERTS
	TextureFormat.width = SQRT_MAX_VERTS
	TextureFormat.format = RenderingDevice.DATA_FORMAT_R32G32B32A32_SFLOAT
	
	var RenderingDeviceUsage = RenderingDevice.TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice.TEXTURE_USAGE_STORAGE_BIT
	var RenderingDeviceUsage1 = RenderingDevice.TEXTURE_USAGE_CAN_COPY_FROM_BIT | RenderingDevice.TEXTURE_USAGE_CAN_COPY_TO_BIT
	TextureFormat.usage_bits = RenderingDeviceUsage | RenderingDeviceUsage1
	
	var VertexTexture = RenderingDevice_Local.texture_create(TextureFormat, TextureView)
	var NormalTexture = RenderingDevice_Local.texture_create(TextureFormat, TextureView)
	
	var VertexTexture_B = RenderingDevice_Local.texture_create(TextureFormat, TextureView)
	
	TextureFormat.height = IDX_SQRT_MAX_VERTS
	TextureFormat.width = IDX_SQRT_MAX_VERTS
	TextureFormat.format = RenderingDevice.DATA_FORMAT_R32_SFLOAT
	
	var IndexTexture = RenderingDevice_Local.texture_create(TextureFormat, TextureView)
	
	TextureFormat.height = 1024
	TextureFormat.width = 1024
	TextureFormat.format = RenderingDevice.DATA_FORMAT_R32G32B32_SFLOAT
	TextureFormat.texture_type = RenderingDevice.TEXTURE_TYPE_2D_ARRAY
	TextureFormat.usage_bits = RenderingDevice.TEXTURE_USAGE_CAN_UPDATE_BIT | RenderingDevice.TEXTURE_USAGE_SAMPLING_BIT
	
	var ALBEDO_Images = []
	var TexturePaths = ["res://bin/Environments/Development Only/Textures/rock_face_03_diff_1k.jpg"]
	
	for path in TexturePaths:
		var texture = ResourceLoader.load(path)
		if texture:
			ALBEDO_Images.append(texture.get_image())
	if(ALBEDO_Images.size() == 0):
		print("No images found. ALBEDO")
		return
	AlbedoTexture_RID = RenderingServer.texture_2d_layered_create(ALBEDO_Images, RenderingServer.TEXTURE_LAYERED_2D_ARRAY)
	
	var NORMAL_Images = []
	var NORMAL_TexturePaths = ["res://bin/Environments/Development Only/Textures/rock_face_03_nor_gl_1k.png"]
	
	for path in NORMAL_TexturePaths:
		var texture = ResourceLoader.load(path)
		if texture:
			NORMAL_Images.append(texture.get_image())
	
	NormalTexture_RID = RenderingServer.texture_2d_layered_create(NORMAL_Images, RenderingServer.TEXTURE_LAYERED_2D_ARRAY)
	
	rendering_server_vertex_texture = RenderingServer.texture_rd_create(VertexTexture)
	#rendering_server_vertex_texture = RenderingServer.texture_rd_create(VertexTexture_B)
	rendering_server_normal_texture = RenderingServer.texture_rd_create(NormalTexture)
	rendering_server_index_texture = RenderingServer.texture_rd_create(IndexTexture)
	
	Material_RID = RenderingServer.material_create()
	RenderingServer.material_set_shader(Material_RID, VP_Shader)
	RenderingServer.instance_geometry_set_material_override(MeshInstance.get_instance(), Material_RID)
	RenderingServer.material_set_param(Material_RID, "IndexTexture", rendering_server_index_texture);
	RenderingServer.material_set_param(Material_RID, "VertexTexture", rendering_server_vertex_texture);
	RenderingServer.material_set_param(Material_RID, "NormalTexture", rendering_server_normal_texture);
	RenderingServer.material_set_param(Material_RID, "AlbedoTextures", AlbedoTexture_RID)
	RenderingServer.material_set_param(Material_RID, "NormalTextures", NormalTexture_RID)
	RenderingServer.material_set_param(Material_RID, "GridSizeIndex", int(IDX_SQRT_MAX_VERTS));
	RenderingServer.material_set_param(Material_RID, "GridSizeVertex", int(SQRT_MAX_VERTS));
	
	print(int(IDX_SQRT_MAX_VERTS))
	
	SetRIDStorage(VertexTexture, NormalTexture, VertexTexture_B, IndexTexture)
	SetCompiledShaders(TestPlanet_Compiled, DualContouring_Compiled, VP_Shader)
	initCompute(13231, MAXVERTS, true,
				CHUNK_SIZE, VOXELS_PER_CHUNK,
				MeshInstance.get_instance(), IDX_COEFFICIENT)
	Ready = true
	
func _input(event):
	if event is InputEventKey and event.is_pressed():
		if event.keycode == KEY_F1:
			PassParamsToPCG(true, false, VOXELS_PER_CHUNK, CHUNK_SIZE)
	
	
func _process(_delta: float):
	#MeshInstance.rotation_degrees = lerp(MeshInstance.rotation_degrees, Vector3(0, 0, Time.get_ticks_msec()/16), 0.01)
	#if(Input.is_action_just_pressed("ui_accept")):
	#passParams_to_PCG(true, false, 0, CurrentEntityLocation, CurrentPlanetLocation, 1, VOXELS_PER_CHUNK, CHUNK_SIZE)
	if(Input.is_action_just_pressed("ui_text_backspace")):
		PrintGeneratedData()
		
func _exit_tree():
	if(rendering_server_vertex_texture.is_valid()):
		RenderingServer.free_rid(rendering_server_vertex_texture)
	if(rendering_server_normal_texture.is_valid()):
		RenderingServer.free_rid(rendering_server_normal_texture)
	if(rendering_server_index_texture.is_valid()):
		RenderingServer.free_rid(rendering_server_index_texture)
	if(Material_RID.is_valid()):
		RenderingServer.free_rid(Material_RID)
	if(AlbedoTexture_RID.is_valid()):
		RenderingServer.free_rid(AlbedoTexture_RID)
	if(NormalTexture_RID.is_valid()):
		RenderingServer.free_rid(NormalTexture_RID)
