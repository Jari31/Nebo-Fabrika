Separate biome blending from modification. i.e., in the first initialization pass of the environment, run the full biome blending algorithm. Then, simply just intersect it with the pre-existing grid.

Screw that. Moving to CPUs is faster. So instead of trying to do biome blending on the GPU through 8 separate passes, we do it directly on the CPU. Then the GPU checks 8 if() checks to dictate which biome functions it wants to use

Because that solves the extreme readback latency of GPU-to-CPU pipelines.

The CPU handles trilinear blending beforehand, alongside CSG trees. Then it compresses it down on a background thread onto disk for local cache.