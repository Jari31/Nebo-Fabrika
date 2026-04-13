from tkinter.constants import CENTER

import taichi as ti
import taichi.math as tm
import numpy as np
from taichi.math import vec3, ivec3, floor, clamp, dot, mix, length
import mcubes as mc
from skimage import measure
import time

#*some of the code has been converted into python runnable scripts by AI to save time (original implementations are in GLSL)

ti.init(arch=ti.gpu, kernel_profiler=True)


@ti.func
def hash_func(in_vector: ivec3) -> ti.f32:
    """Hash function converting ivec3 to normalized float [0, 1)"""
    # Note: renamed from 'hash' to avoid conflict with Python builtin
    mult_sum = (ti.u32(in_vector.x) * 374761393) ^ \
               (ti.u32(in_vector.y) * 1664525) ^ \
               (ti.u32(in_vector.z) * 1103515245)

    # Final mixing stages
    mult_sum = mult_sum + (mult_sum << 3)
    mult_sum = mult_sum ^ (mult_sum >> 11)
    mult_sum = mult_sum + (mult_sum << 15)

    # Convert uint to normalized float: 1/2^32 ≈ 2.3283064365386963e-10
    return ti.f32(mult_sum) * 2.3283064365386963e-10

@ti.func
def gradient_hash(in_vector: ivec3, seed: ti.u32) -> vec3:
    """Generate gradient vector using hash with seed offset"""
    i_seed = ti.i32(seed)

    rand_x = hash_func(in_vector)
    rand_y = hash_func(in_vector + ivec3(4, 7, 13) * i_seed)
    rand_z = hash_func(in_vector + ivec3(19, 23, 29) * i_seed)

    # Map [0,1) to [-1, 1]
    return vec3(rand_x, rand_y, rand_z) * 2.0 - 1.0

@ti.func
def point_process(in_point: vec3, gradient_hash: vec3) -> ti.f32:
    """Apply radial falloff and dot product for single simplex point"""
    radius_falloff = 0.5

    t0_sq = dot(in_point, in_point)
    t0 = radius_falloff - t0_sq

    result = 0.0
    if t0 > 0.0:
        t0_pow3 = t0 * t0 * t0
        result = t0_pow3 * dot(gradient_hash, in_point)

    return result
@ti.func
def iqint1(x: ti.int32):
    x = ((x >> 16) ^ x) * 0x45d9f3b
    x = ((x >> 16) ^ x) * 0x45d9f3b
    x = (x >> 16) ^ x
    return x

@ti.func
def cast_hash(f: ti.float32) -> ti.float32:
    intVal = ti.bit_cast(f, ti.i32)

    intVal ^= intVal >> 16
    intVal *= 0x45d9f3b
    intVal ^= intVal >> 16

    return (ti.cast(intVal, ti.f32) * (1.0 / 4294967296.0)) * 2.0 - 1

@ti.func
def cast_gradient_hash(inVector: ivec3, SEED: ti.int32) -> vec3:
    iSEED = SEED

    rand_X = cast_hash(ti.cast(ti.int32(inVector.x) ^ (SEED * 0x27D4EB2D), ti.f32))
    rand_Y = cast_hash(ti.cast(ti.int32(inVector.y) ^ (SEED * 0x375E546B), ti.f32))
    rand_Z = cast_hash(ti.cast(ti.int32(inVector.z) ^ (SEED * 0x4385DF64), ti.f32))

    outVector = vec3(rand_X, rand_Y, rand_Z) - floor(vec3(rand_X, rand_Y, rand_Z))
    #print(outVector)
    return outVector

def CPU_iqint1(x):
    x = ((x >> 16) ^ x) * 0x45d9f3b
    x = ((x >> 16) ^ x) * 0x45d9f3b
    x = (x >> 16) ^ x
    return x

def CPU_iqint_gradient_hash(inVector, SEED) -> vec3:
    iSEED = SEED

    rand_X = inVector.x
    rand_Y = inVector.y + (8 * iSEED)
    rand_Z = inVector.z + (16 * iSEED)

    outVector = vec3(rand_X, rand_Y, rand_Z) * 2.0 - 1.0

    return outVector

@ti.func
def simplex3d(in_vector: vec3, seed: ti.int32) -> ti.f32:

    skew_factor = 0.333333333  # 1/3
    unskewing_factor = 0.166666666  # 1/6

    simplex_sum = in_vector.x + in_vector.y + in_vector.z
    skew_offset = simplex_sum * skew_factor

    skewed_vec = in_vector + skew_offset
    floored_vec = ivec3(floor(skewed_vec))

    floored_vec_sum = ti.f32(floored_vec.x + floored_vec.y + floored_vec.z)
    unskew_offset = floored_vec_sum * unskewing_factor

    unskewed_floored_vec = vec3(floored_vec) - unskew_offset
    point_zero = in_vector - unskewed_floored_vec

    i1_offset = ivec3(0, 0, 0)
    i2_offset = ivec3(0, 0, 0)

    if point_zero.x >= point_zero.y:
        if point_zero.y >= point_zero.z:  # X >= Y >= Z
            i1_offset = ivec3(1, 0, 0)
            i2_offset = ivec3(1, 1, 0)
        elif point_zero.x >= point_zero.z:  # X >= Z > Y
            i1_offset = ivec3(1, 0, 0)
            i2_offset = ivec3(1, 0, 1)
        else:  # Z > X >= Y
            i1_offset = ivec3(0, 0, 1)
            i2_offset = ivec3(1, 0, 1)
    else:
        if point_zero.y < point_zero.z:  # Z > Y > X
            i1_offset = ivec3(0, 0, 1)
            i2_offset = ivec3(0, 1, 1)
        elif point_zero.x >= point_zero.z:  # Y >= X >= Z
            i1_offset = ivec3(0, 1, 0)
            i2_offset = ivec3(1, 1, 0)
        else:  # Y >= Z > X
            i1_offset = ivec3(0, 1, 0)
            i2_offset = ivec3(0, 1, 1)

    i3_offset = ivec3(1, 1, 1)

    i0_cell = floored_vec
    i1_cell = floored_vec + i1_offset
    i2_cell = floored_vec + i2_offset
    i3_cell = floored_vec + i3_offset

    offset = ti.f32(i1_offset.x + i1_offset.y + i1_offset.z)

    i1_sum = offset
    point_one = point_zero - (vec3(i1_offset) - i1_sum * unskewing_factor)

    i2_sum = offset
    point_two = point_zero - (vec3(i2_offset) - i2_sum * unskewing_factor)

    i3_sum = offset
    point_three = point_zero - (vec3(i3_offset) - i3_sum * unskewing_factor)

    gradient_vec  = gradient_hash(i0_cell, seed)
    gradient_vec1 = gradient_hash(i1_cell, seed)
    gradient_vec2 = gradient_hash(i2_cell, seed)
    gradient_vec3 = gradient_hash(i3_cell, seed)

    n = point_process(point_zero, gradient_vec)
    n += point_process(point_one, gradient_vec1)
    n += point_process(point_two, gradient_vec2)
    n += point_process(point_three, gradient_vec3)

    return clamp(n * 40, -1.0, 1.0)

VSize = 128
DensityBuffer = ti.Vector.field(4, dtype=ti.f32, shape=(VSize ** 3))

AccumulativeResult = ti.field(dtype=ti.f32, shape=(1))

@ti.func
def expand_bits(x: ti.u32) -> ti.u32:
    x = (x * ti.u32(0x00010001)) & ti.u32(0xFF0000FF)
    x = (x * ti.u32(0x00000101)) & ti.u32(0x0F00F00F)
    x = (x * ti.u32(0x00000011)) & ti.u32(0xC30C30C3)
    x = (x * ti.u32(0x00000005)) & ti.u32(0x49249249)
    return x

@ti.func
def de_interleave_bits(x: int) -> int:
    x &= ti.u32(0x49249249)
    x = (x | (x >> 2)) & ti.u32(0xC30C30C3)
    x = (x | (x >> 4)) & ti.u32(0x0F00F00F)
    x = (x | (x >> 8)) & ti.u32(0xFF0000FF)
    x = (x | (x >> 16)) & ti.u32(0x000003FF)

    return x

@ti.kernel
def loop_over_voxel_grid(frequency: ti.f32, frequency2: ti.f32, Amp: ti.f32, Amp2: ti.f32, SEED: ti.int32):
    radius = 6371
    center = vec3(500, -radius + 30, 0) #vec3(VSize / 2, VSize / 2, VSize / 2)
    MIndex = VSize ** 3

    for x, y, z in ti.ndrange(VSize, VSize, VSize):
        i = x + (y * VSize) + (z * VSize ** 2)
        #i = ti.uint32((expand_bits(x)) | (expand_bits(y) << 1) | (expand_bits(z) << 2))
        distance = length(center + vec3(x, y, z))
        pos = vec3(x, y, z) + cast_hash(ivec3(x, y + x, z + y))
        if (i < MIndex):
            noise_val = simplex3d(pos * vec3(frequency), SEED) #* Amp
            #TORF = ti.uint1(distance <= radius)
            #if TORF:
            Density = 0.2 - noise_val
            #DensityBuffer[i].w = ti.max((tm.length(vec3(x, y, z) - center) - radius),  -noise_val)

            Density += DensityBuffer[i].w + simplex3d(pos * frequency * 2, SEED) * (Amp * 0.5)
            Density -= DensityBuffer[i].w + simplex3d(pos * frequency * 3, SEED) * (Amp * 0.25)

            #DensityBuffer[i].w = ti.max(DensityBuffer[i].w, (tm.length(vec3(x, y, z) - center - vec3(0, 20, 0))) - radius)
            #DensityBuffer[i].w = 1
            DensityBuffer[i].w = Density



    '''
    # print(simplex3d(ti.Vector([14, 4, 5]), 2044))
    VSIZE_TOTAL = VSize ** 3 # voxel in 1 axis * 3
    for i in range(VSIZE_TOTAL):
        DensityBuffer[i].x = ti.float32(i) #+ 0.7) #* frequency
        DensityBuffer[i].y = ti.float32(i)# + 0.6) #* frequency
        DensityBuffer[i].z = ti.float32(i)# + 0.8) #* frequency
        #DensityBuffer[i].w = simplex3d(DensityBuffer[i].xyz, 3290)

        #DensityBuffer[i].x = ti.float32(i + 0.7) * frequency2
        #DensityBuffer[i].y = ti.float32(i + 0.6) * frequency2
        #DensityBuffer[i].z = ti.float32(i + 0.8) * frequency2
        #DensityBuffer[i].w += simplex3d(DensityBuffer[i].xyz, 3290)

        DensityBuffer[i].w = (tm.length(vec3(DensityBuffer[i].x, DensityBuffer[i].y, DensityBuffer[i].z)) - 4)
    '''


        # print(DensityBuffer[i].w)

def CPU_expand_bits(x) :
    x = (x * 0x00010001) & 0xFF0000FF
    x = (x * 0x00000101) & 0x0F00F00F
    x = (x * 0x00000011) & 0xC30C30C3
    x = (x * 0x00000005) & 0x49249249
    return x

def CPU_get_morton_code(x, y, z):
    return expand_bits(x) | expand_bits(y << 1) | expand_bits(z << 2)

# if __name__ == "__main__":
loop_over_voxel_grid(0.01, 0.1, 1, 0.6, -500)
ti.profiler.print_kernel_profiler_info()

np_buffer = DensityBuffer.to_numpy()
reshaped_buffer = np_buffer.reshape((VSize, VSize, VSize, 4))
density = reshaped_buffer[:, :, :, 3]

#print(AccumulativeResult[0])
"""
vertices, triangles = mc.marching_cubes(density, 0.5)

# print(vertices, "+", triangles)

NumVertices = vertices.shape[0]
NumTriangles = triangles.shape[0]
#print(triangles.shape)

VertexBuffer = ti.Vector.field(3, dtype=ti.f32, shape=NumVertices)
IndiceBuffer = ti.field(dtype=ti.i32, shape=NumTriangles * 3)

VertexBuffer.from_numpy(vertices.astype(np.float32))
IndiceBuffer.from_numpy(triangles.astype(np.int32).reshape(-1))
"""

verts, faces, normals, values = measure.marching_cubes(density, level=0)

NumVertices = verts.shape[0]
NumTriangles = faces.shape[0]

VertexBuffer = ti.Vector.field(3, dtype=ti.f32, shape=NumVertices)
NormalBuffer = ti.Vector.field(3, dtype=ti.f32, shape=NumVertices)
IndiceBuffer = ti.field(dtype=ti.i32, shape=NumTriangles * 3)

VertexBuffer.from_numpy(verts.astype(np.float32))
NormalBuffer.from_numpy(normals.astype(np.float32))
IndiceBuffer.from_numpy(faces.astype(np.int32).reshape(-1))

Window = ti.ui.Window("Marching Cubes", (1280, 720), vsync=False)
Canvas = Window.get_canvas()
Camera = ti.ui.Camera()

radius = 5.0
speed = 1.0

while Window.running:
    Camera.track_user_inputs(Window, movement_speed=1, hold_key=ti.ui.RMB)\

    t = time.time() * speed

    lx = radius * ti.math.cos(t)
    lz = radius * ti.math.sin(t)
    ly = 10.0

    Scene = Window.get_scene()
    Scene.set_camera(Camera)
    Scene.ambient_light((0.2, 0.2, 0.2))

    Scene.point_light(pos=(lx, ly, lz), color=(1.0, 1.0, 1.0))
    Scene.point_light(pos=(40, 20, 40), color=(1.0, 1.0, 1.0))

    Scene.mesh(VertexBuffer, indices=IndiceBuffer, normals=NormalBuffer, color=(0.2, 0.8, 0.2))

    Canvas.scene(Scene)
    Window.show()
