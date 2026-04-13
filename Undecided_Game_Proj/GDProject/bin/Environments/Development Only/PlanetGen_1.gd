extends PCG_Environment

func _ready():
	#initCompute(123412, 42000, true, true, true, "", )
	SetSettings(true, true)
	var ShaderCompObject = ShaderCompiler.new()	
	var DoCompilation = false
	
	var CompileDense = false
	var CompileSparse = false
	var CompilePlanet = false
	
	var ShaderComp_DEBUG = true
	var Workgroups = Vector3i(8, 8, 8)
	var RenderingDevice_Local = GetRID()
	
	var PathToComputeShader = "res://bin/Shaders/Compute Shaders/Libs/Dual Contouring/DualContouring.glsl"
	var CompileTo = "res://bin/Shaders/Compute Shaders/Compiled/dual_contouring.spv"
	
	var DualContouring_Compiled = ShaderCompObject.LoadOrCompileShader(PathToComputeShader, CompileTo, CompileDense,
									RenderingDevice_Local, WorldManager.ShaderStages.COMPUTE_SHADER, 
									Workgroups, ShaderComp_DEBUG)

	PathToComputeShader = "res://bin/Shaders/Compute Shaders/Libs/SVO/SparseVoxelOctreeOptimized.glsl"
	CompileTo = "res://bin/Shaders/Compute Shaders/Compiled/svo.spv"
	
	var SVO_Compiled = ShaderCompObject.LoadOrCompileShader(PathToComputeShader, CompileTo, DoCompilation,
									RenderingDevice_Local, WorldManager.ShaderStages.COMPUTE_SHADER, 
									Workgroups, ShaderComp_DEBUG)
	
	PathToComputeShader = "res://bin/Shaders/Compute Shaders/Libs/MathLibs/SVOHistogram.glsl"
	CompileTo = "res://bin/Shaders/Compute Shaders/Compiled/svo_histogram.spv"
	
	var Histogram_Compiled = ShaderCompObject.LoadOrCompileShader(PathToComputeShader, CompileTo, DoCompilation,
									RenderingDevice_Local, WorldManager.ShaderStages.COMPUTE_SHADER, 
									Workgroups, ShaderComp_DEBUG)
	
	PathToComputeShader = "res://bin/Shaders/Compute Shaders/Libs/MathLibs/SVOPrefixSum.glsl"
	CompileTo = "res://bin/Shaders/Compute Shaders/Compiled/svo_prefixsum.spv"
	
	var PrefixSum_Compiled = ShaderCompObject.LoadOrCompileShader(PathToComputeShader, CompileTo, DoCompilation,
									RenderingDevice_Local, WorldManager.ShaderStages.COMPUTE_SHADER, 
									Workgroups, ShaderComp_DEBUG)
	
	PathToComputeShader = "res://bin/Shaders/Compute Shaders/Environment Generation GLSL/test_planet.glsl"
	CompileTo = "res://bin/Shaders/Compute Shaders/Compiled/test_planet.spv"
	
	var TestPlanet_Compiled = ShaderCompObject.LoadOrCompileShader(PathToComputeShader, CompileTo, CompilePlanet,
									RenderingDevice_Local, WorldManager.ShaderStages.COMPUTE_SHADER, 
									Workgroups, ShaderComp_DEBUG)
									
	PathToComputeShader = "res://bin/Shaders/Compute Shaders/Libs/Dual Contouring/DualContouring_SVO.glsl"
	CompileTo = "res://bin/Shaders/Compute Shaders/Compiled/dual_contouring_sparse.spv"

	var DualContouringSparse_Compiled = ShaderCompObject.LoadOrCompileShader(PathToComputeShader, CompileTo, CompileSparse,
										RenderingDevice_Local, WorldManager.ShaderStages.COMPUTE_SHADER, 
										Workgroups, ShaderComp_DEBUG)
										
	var CurrentEntityLocation = PackedInt32Array()
	var CurrentPlanetLocation = PackedInt32Array()
	var CHUNK_SIZE = PackedInt32Array()
	var VOXELS_PER_CHUNK = PackedInt32Array()
	var MCLUT_FileName = PackedStringArray()
	MCLUT_FileName.append("")
	for i in range(3):
		CurrentEntityLocation.append(0)
		CurrentPlanetLocation.append(0)
		CHUNK_SIZE.append(4)
		VOXELS_PER_CHUNK.append(64)
	
	SetCompiledShaders(TestPlanet_Compiled, SVO_Compiled, DualContouring_Compiled, DualContouringSparse_Compiled, PrefixSum_Compiled, Histogram_Compiled)
	initCompute(13231, 40000, 
				true, true, true, 
				MCLUT_FileName, CHUNK_SIZE, VOXELS_PER_CHUNK, 
				CurrentEntityLocation, CurrentPlanetLocation, 1)
	passParams_to_PCG(false, true, 0, CurrentEntityLocation, CurrentPlanetLocation, 1, VOXELS_PER_CHUNK, CHUNK_SIZE)
