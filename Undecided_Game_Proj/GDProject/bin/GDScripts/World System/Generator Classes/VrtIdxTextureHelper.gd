class_name VrtIdxTextureHelper

class VrtIdxObject:
	var VertexTexture: RID
	var NormalTexture: RID
	var VertexTexture_B: RID
	var IndexTexture: RID
	
	var rendering_server_vertex_texture: RID
	var rendering_server_index_texture: RID
	var rendering_server_normal_texture: RID

static func _init_mesher_textures(SQRT_MAX_VERTS: int, IDX_SQRT_MAX_VERTS: int,
						UsePostProcessVertexBuffer: bool = true) -> VrtIdxObject:
	var RenderingDevice_Local = RenderingServer.get_rendering_device()
	var TextureFormat = RDTextureFormat.new()
	var TextureView = RDTextureView.new()	
	
	var VIdxObject = VrtIdxObject.new()

	TextureFormat.height = SQRT_MAX_VERTS
	TextureFormat.width = SQRT_MAX_VERTS
	TextureFormat.format = RenderingDevice.DATA_FORMAT_R32G32B32A32_SFLOAT
	
	var RenderingDeviceUsage = RenderingDevice.TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice.TEXTURE_USAGE_STORAGE_BIT
	var RenderingDeviceUsage1 = RenderingDevice.TEXTURE_USAGE_CAN_COPY_FROM_BIT | RenderingDevice.TEXTURE_USAGE_CAN_COPY_TO_BIT
	TextureFormat.usage_bits = RenderingDeviceUsage | RenderingDeviceUsage1
	
	VIdxObject.VertexTexture = RenderingDevice_Local.texture_create(TextureFormat, TextureView)
	VIdxObject.NormalTexture = RenderingDevice_Local.texture_create(TextureFormat, TextureView)
	
	if(UsePostProcessVertexBuffer):
		VIdxObject.VertexTexture_B = RenderingDevice_Local.texture_create(TextureFormat, TextureView)
	
	TextureFormat.height = IDX_SQRT_MAX_VERTS
	TextureFormat.width = IDX_SQRT_MAX_VERTS
	TextureFormat.format = RenderingDevice.DATA_FORMAT_R32_SFLOAT
	
	VIdxObject.IndexTexture = RenderingDevice_Local.texture_create(TextureFormat, TextureView)

	VIdxObject.rendering_server_vertex_texture = RenderingServer.texture_rd_create(VIdxObject.VertexTexture)
	VIdxObject.rendering_server_normal_texture = RenderingServer.texture_rd_create(VIdxObject.NormalTexture)
	VIdxObject.rendering_server_index_texture = RenderingServer.texture_rd_create(VIdxObject.IndexTexture)
	
	return VIdxObject
