extends RefCounted
class_name ShaderLoader
## A basic, base celestial body class that handles basic shader loading, compilation and initialization.

#var Lock = Mutex.new()

@export var UseLocalRenderingDevice: bool = false

@export var ComputeShaderLocation: Dictionary = {
	"DualContouring"                   : "res://bin/Shaders/Compute Shaders/Compiled/dual_contouring.spv",
	"UberShader"                       : "res://bin/Shaders/Compute Shaders/Compiled/test_planet.spv"
}

## File path of the saved file containing the locations of the compiled shader.
@export var ShaderLocationFilePath: String

var rd: RenderingDevice = RenderingServer.get_rendering_device()

#var LoadedShaders = {}

#func LoadAllComputeShaders():
#	var ShaderCompObj = ShaderCompiler.new()
#	for Shaders in ComputeShaderLocation:
#		LoadedShaders[Shaders] = ShaderCompObj.LoadOrCompileShader(ComputeShaderLocation[Shaders], "", false, rd, 
#												 WorldManager.ShaderStages.COMPUTE_SHADER, Vector3i(8, 8, 8), GlobalFlags.DEBUG)

#func LoadComputeShader(ShaderAlias: String):
#	var ShaderCompObj = ShaderCompiler.new()
#	LoadedShaders[ShaderAlias] = ShaderCompObj.LoadOrCompileShader(ComputeShaderLocation[ShaderAlias], "", false, rd, 
#												 WorldManager.ShaderStages.COMPUTE_SHADER, Vector3i(8, 8, 8), GlobalFlags.DEBUG)
#												
func SaveShaderLocations(SaveFilePath: String, SaveDictionary: Dictionary):
	var SaveFile = FileAccess.open(SaveFilePath, FileAccess.WRITE)
	SaveFile.store_var(SaveDictionary)
	SaveFile.close()

func LoadComputeShaderLocations(SaveFilePath: String, SaveDictionary: Dictionary):
	var ShaderCompObj = ShaderCompiler.new();
	if(FileAccess.file_exists(SaveFilePath)):
		var SaveFile = FileAccess.open(SaveFilePath, FileAccess.READ)
		var LocationDictonary: Dictionary
		LocationDictonary = SaveFile.get_var()
		SaveFile.close()
		
		for path in LocationDictonary:
			SaveDictionary[path] = ShaderCompObj.LoadOrCompileShader("", LocationDictonary[path],
			false, rd, WorldManager.ShaderStages.COMPUTE_SHADER, Vector3i(8, 8, 8), GlobalFlags.DEBUG)
		return
	printerr("Shader load operation failed. The file location provided, ", SaveFilePath,  " is most likely invalid.")
	
func CompileComputeShader(PathToComputeShader: String, CompileTo: String):
	var ShaderCompObj = ShaderCompiler.new();
	
	return ShaderCompObj.LoadOrCompileShader(PathToComputeShader, CompileTo, true, rd, 
									WorldManager.ShaderStages.COMPUTE_SHADER, 
									Vector3i(8, 8, 8), GlobalFlags.DEBUG)
	
#func CompileAndLoadComputeShaders(ShaderDict: Dictionary, CompileTo_Dict: Dictionary):
#	for Shaders in ShaderDict:
#		CompileComputeShader(Shaders, ShaderDict[Shaders], CompileTo_Dict[Shaders])
#	LoadAllComputeShaders()

func Load2DTextureArray(Paths: Array) -> RID: # this shit doesn't work because fuck me I guess
	var ImageArray = []                # fuck garbage collection
	
	for Path in Paths:
		var texture = ResourceLoader.load(Path)
		texture = texture.get_image()
		if(texture.get_format() != 5):
			texture.convert(Image.FORMAT_RGBA8)
		if(texture):
			ImageArray.append(texture)
	
	if(ImageArray.is_empty()):
		printerr("The images provided do not exist.")
		return RID()
	'''
	var Width = ImageArray[0].get_width()
	var Height = ImageArray[0].get_height()
	var Layers = ImageArray.size()
	
	if(Layers <= 1):
		printerr("The given texture array does not have enough layers to be able to be used.")
	
	var Format = RDTextureFormat.new()
	Format.width = Width
	Format.height = Height
	Format.array_layers = Layers
	Format.format = RenderingDevice.DATA_FORMAT_R8G8B8A8_UNORM
	Format.texture_type = RenderingDevice.TEXTURE_TYPE_2D_ARRAY
	Format.usage_bits = RenderingDevice.TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice.TEXTURE_USAGE_CAN_UPDATE_BIT
	
	var View = RDTextureView.new()
	var Data = []
	for image in ImageArray:
		Data.append(image.get_data())
	
	var rid = RenderingServer.texture_rd_create(rd.texture_create(Format, View, Data))
	'''
	
	var rid = RenderingServer.texture_2d_layered_create(ImageArray, RenderingServer.TEXTURE_LAYERED_2D_ARRAY)
	return rid
	
func _notification(what):
	if(what == NOTIFICATION_PREDELETE):
		RenderingServer.free_rid(rd)
