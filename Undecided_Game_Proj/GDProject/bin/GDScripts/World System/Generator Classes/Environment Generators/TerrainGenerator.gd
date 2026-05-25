extends TerrainMeshGenerator
class_name TerrainGenerator
## An extension of its parent class, Base_TerrainGenerator serves as the base for planet driven systems.
## It assumes more things, making it less flexible.
##
## DO NOT use this to define planets with this. 
## This is simply a wrapper around its base class.
## i.e., this generates terrain by looking at parameters you feed it.
## This is to keep things modular, so this class can be used to generate practically anything.
## Define planets as particles.

@export var NormalTextureArray_RID: RID = RID()
@export var AlbedoTextureArray_RID: RID = RID()

@export var AutoClearRIDs: bool = true

static func _load_image_array(Paths: Array) -> Array:
	var ImageArray = []
	
	for Path in Paths:
		var texture# = ResourceLoader.load(Path)
		texture = Path.get_image()
		if(texture.get_format() != 5):
			texture.convert(Image.FORMAT_RGBA8)
		if(texture):
			ImageArray.append(texture)
	
	if(ImageArray.is_empty()):
		printerr("The images provided do not exist.")
		return []

	return ImageArray

func InitPlanet(InitTextures: bool, InitMesh: bool, AlbedoMaps: Array, NormalMaps: Array):
	InitMesher(InitMesh)
	if InitMesh: InitVertexPullShader()
	if(InitTextures):
		AlbedoTextureArray_RID = RenderingServer.texture_2d_layered_create(_load_image_array(AlbedoMaps), RenderingServer.TEXTURE_LAYERED_2D_ARRAY)
		NormalTextureArray_RID = RenderingServer.texture_2d_layered_create(_load_image_array(NormalMaps), RenderingServer.TEXTURE_LAYERED_2D_ARRAY)
		
	RenderingServer.material_set_param(Material_RID, "AlbedoTextures", AlbedoTextureArray_RID)
	RenderingServer.material_set_param(Material_RID, "NormalTextures", NormalTextureArray_RID)
	
func DeInitPlanet():
	if(NormalTextureArray_RID.is_valid()):
		RenderingServer.free_rid(NormalTextureArray_RID)
	if(AlbedoTextureArray_RID.is_valid()):
		RenderingServer.free_rid(AlbedoTextureArray_RID)

func GenerateTerrain(VertexOffset: Vector3i, LoD: int = 0):
	var VOffsetBuffer = PackedInt32Array([VertexOffset.x, VertexOffset.y, VertexOffset.z])
	PassParamsToPCG(true, UseLocalRenderingDevice, VOXELS_PER_CHUNK, CHUNK_SIZE, VOffsetBuffer, LoD)
	
func _exit_tree() -> void:
	if(AutoClearRIDs):
		DeInitPlanet()
