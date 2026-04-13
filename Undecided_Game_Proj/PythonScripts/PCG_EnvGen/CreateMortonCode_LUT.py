import taichi as ti
import numpy as np

ARCH = ti.gpu

ti.init(ARCH, kernel_profiler=True)

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

InterleaveLUT = ti.field(ti.u32, shape=1024)
DeInterleaveLUT = ti.field(ti.u32, shape=1024)

@ti.kernel #
def main():
    for i in range(1024):
        mcode = expand_bits(i)
        de_mcode = de_interleave_bits(i)

        InterleaveLUT[i] = mcode
        DeInterleaveLUT[i] = de_mcode

def save_to_binary(field, filename):
    data = field.to_numpy().astype(np.uint32)
    with open(filename, 'wb') as file:
        file.write(data.tobytes())

if __name__ == "__main__":
    main()

    save_to_binary(InterleaveLUT, f"interLUT.bin")
    save_to_binary(DeInterleaveLUT, f"deinterLUT.bin")
    ti.profiler.print_kernel_profiler_info()