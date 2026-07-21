extends EnvironmentGenerator

@export var GridSizeOfASingleAxis: int = 32;
var DensityBuffer: VoxelBuffer

func _ready() -> void:
	InitializeCPUGenerator(GridSizeOfASingleAxis)
	DensityBuffer = GetCPUVoxelBuffer(0)
	DensityBuffer.ExtractSlice(0, DensityBuffer.GetVoxelBufferSize())
	
	#PrintCPUBuffers()

func _input(Event: InputEvent) -> void:
	if(Event.is_action_pressed("F2")):
		CPU_ParallelSimplex3D(421, Vector3(0, 0, 0), GridSizeOfASingleAxis, DensityBuffer)
