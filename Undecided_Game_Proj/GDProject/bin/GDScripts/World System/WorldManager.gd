extends Node

enum ShaderStages {COMPUTE_SHADER, VERTEX_SHADER, FRAGMENT_SHADER}

var TRUTH_GRID_SIZE = 64 * 4
var CHUNK_SIZE = 4
var BaseTerrainThreadCoefficient: float = 25.2

var VisualThreadLoDCounts: Array = [4, 32, 64]
var VisualThreadLoDVertexCount: Array = [60000, 30000, 10000]

var _mutex: Mutex = Mutex.new()

class TextureArrayRIDObject:
	## Normal, height
	var NormalHTextureArray_RID: RID
	## Albedo, alpha/transparency
	var AlbedoATextureArray_RID: RID
	## Roughness, emission, ambient occlusion, metallic
	var REAMTextureArray_RID: RID

var TextureArrayPool: Dictionary

func TrySetTexturePoolIndex(TextureArrayRIDs: TextureArrayRIDObject, 
							Alias: int) -> bool:
	if(_mutex.try_lock()):
		TextureArrayPool[Alias] = TextureArrayRIDs
		_mutex.unlock()
		return true
		
	return false

func TryGetTexturePoolIndex(Alias: int) -> TextureArrayRIDObject:
	if(_mutex.try_lock()):
		var Obj = TextureArrayPool[Alias]
		_mutex.unlock()
		return Obj
	return null
