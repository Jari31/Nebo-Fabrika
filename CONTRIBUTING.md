# Naming Conventions

The naming convention is a mix of PascalCase, snake_case and SCREAMING_SNAKE_CASE. Verbosity and long-term usability and stability is valued above all else. Abbreviation is discouraged in most cases.

### PascalCase

PascalCase is dedicated for _*heap-allocated variables, external functions, parameters, non-compile-time constants, and types*_. **snake_case takes over for heap-allocated variables if they are within a function.**

### snake_case

snake_case is, as mentioned before, dedicated for internal variables and functions. Internal as in outside invokers aren't meant to access it; internal plumbing, in a sense.

### SCREAMING_SNAKE_CASE

SCREAMING_SNAKE_CASE is meant for compile-time constants.

## A full example

```cpp
inline void ParallelNoise3D(ParallelNoise3D_Options &Options, auto NoiseInvokerLambda)
{
    const uint8_t     SUB_CHUNK_SIZE_X = Options.SupportedSIMDLanes;
    constexpr uint8_t SUB_CHUNK_SIZE_Y =
        NoiseAlgorithms::SIMD_Helpers::ISPC_MAXIMUM_SUPPORTED_LANES;
    constexpr uint8_t SUB_CHUNK_SIZE_Z =
        NoiseAlgorithms::SIMD_Helpers::ISPC_MAXIMUM_SUPPORTED_LANES;

    auto GridSizeOfASingleAxis = Options.GridSizeOfASingleAxis;

    const uint32_t NumberOfChunksX = GridSizeOfASingleAxis / SUB_CHUNK_SIZE_X;
    const uint32_t NumberOfChunksY = GridSizeOfASingleAxis / SUB_CHUNK_SIZE_Y;
    const uint32_t NumberOfChunksZ = GridSizeOfASingleAxis / SUB_CHUNK_SIZE_Z;

    const uint32_t TotalChunks    = NumberOfChunksX * NumberOfChunksY * NumberOfChunksZ;
    const uint32_t ChunksPerSlice = NumberOfChunksX * NumberOfChunksY; // X major

    enki::TaskSet task(
        TotalChunks,
        [=](enki::TaskSetPartition Range, uint32_t ThreadNumber) -> void
        {
            for (uint32_t i = Range.start; i < Range.end; i++)
            {
                // find the current thread's XYZ coordinates
                uint32_t i_x = i % NumberOfChunksX;
                uint32_t i_y = (i / NumberOfChunksX) % NumberOfChunksY;
                uint32_t i_z = i / ChunksPerSlice;

                ParallelNoise3D_Context Context;

                Context.StartFromIndex = {
                    i_x * SUB_CHUNK_SIZE_X,
                    i_y * SUB_CHUNK_SIZE_Y,
                    i_z * SUB_CHUNK_SIZE_Z,
                };
                Context.WorkUntilIndex = {
                    Context.StartFromIndex[0] + SUB_CHUNK_SIZE_X,
                    Context.StartFromIndex[1] + SUB_CHUNK_SIZE_Y,
                    Context.StartFromIndex[2] + SUB_CHUNK_SIZE_Z,
                };

                NoiseInvokerLambda(Context);
            }
        });

    Options.TaskScheduler.AddTaskSetToPipe(&task);
    Options.TaskScheduler.WaitforTask(&task);
}
```

### Exceptions

As you might've noticed, the example provided had some quirks.

```cpp
    const uint8_t     SUB_CHUNK_SIZE_X = Options.SupportedSIMDLanes;
    constexpr uint8_t SUB_CHUNK_SIZE_Y =
        NoiseAlgorithms::SIMD_Helpers::ISPC_MAXIMUM_SUPPORTED_LANES;
    constexpr uint8_t SUB_CHUNK_SIZE_Z =
        NoiseAlgorithms::SIMD_Helpers::ISPC_MAXIMUM_SUPPORTED_LANES;
```

That is intentional. If it aids in readability and usability, you can put aside the rules for conformity. Use the rules as a guide. Though, remember not to overdo it.

#### Abrv

```cpp
    for (uint32_t i = Range.start; i < Range.end; i++)
            {
                // find the current thread's XYZ coordinates
                uint32_t i_x = i % NumberOfChunksX;
                uint32_t i_y = (i / NumberOfChunksX) % NumberOfChunksY;
                uint32_t i_z = i / ChunksPerSlice;
```

Abbreviation like this is acceptable _only if the thing you're abbreviating is well known and aids in readability._ **Do not optimize for how many keystrokes it takes to write code.** Writing code is only a fraction of the battle; people, including you, will have to read it in the future. And you are not a programmer from the 1980s; you have an IDE with autocomplete. Use it. As for what is readability? Ask a fellow dev when in doubt.

# Comments

Write code that comments itself, though, not dogmatically. Comment things that are not obvious, like a physics solver, or whilst talking to another API. Otherwise, keep comments minimal, and keep code noise at minima.

# What else?

_**Use ClangD and Cppcheck before committing. Be respectful to people.**_
