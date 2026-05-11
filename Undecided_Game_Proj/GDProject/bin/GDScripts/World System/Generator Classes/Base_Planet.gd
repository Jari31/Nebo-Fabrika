extends UB_CelestialGenerationBody
class_name Base_Planet
## An extension of its parent class, Base_Planet serves as the base for planet driven systems.
## It assumes more things, making it less flexible.
##
## Don't let the planet name fool you. It can act as multitudes of different generation bodies.
## Such as asteroids.

@export var NormalTextureArray_RID: RID = RID()
@export var AlbedoTextureArray_RID: RID = RID()

var TestArray = []

@export var AutoClearRIDs: bool = true

static func _load_image_array(Paths: Array) -> Array:
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

func InitPlanet(InitTextures: bool, InitMesh: bool, AlbedoMaps: Array, NormalMaps: Array):
	InitMesher(InitMesh)
	if InitMesh: InitVertexPullShader()
	if(InitTextures):
		AlbedoTextureArray_RID = RenderingServer.texture_2d_layered_create(_load_image_array(AlbedoMaps), RenderingServer.TEXTURE_LAYERED_2D_ARRAY)
		NormalTextureArray_RID = RenderingServer.texture_2d_layered_create(_load_image_array(NormalMaps), RenderingServer.TEXTURE_LAYERED_2D_ARRAY)
		TestArray.append(str(AlbedoTextureArray_RID))
		
	RenderingServer.material_set_param(Material_RID, "AlbedoTextures", AlbedoTextureArray_RID)
	RenderingServer.material_set_param(Material_RID, "NormalTextures", NormalTextureArray_RID)
	
func DeInitPlanet():
	if(NormalTextureArray_RID.is_valid()):
		RenderingServer.free_rid(NormalTextureArray_RID)
	if(AlbedoTextureArray_RID.is_valid()):
		RenderingServer.free_rid(AlbedoTextureArray_RID)

func GenerateTerrain(LoD0: int = 0, LoD1: int = 0, LoD2: int = 0):
	PassParamsToPCG(true, UseLocalRenderingDevice, VOXELS_PER_CHUNK, CHUNK_SIZE)
	
func _notification(what):
	if(what == NOTIFICATION_PREDELETE and AutoClearRIDs):
		DeInitPlanet()
