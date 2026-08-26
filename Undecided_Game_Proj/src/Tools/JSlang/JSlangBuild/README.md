# JSlang

## What even is it?

A build tool for Shader Slang.

## Why?

To avoid having to build complex build tools inside of CMake, XMake, or wherever else.

## Features

JSlang implements a couple of nice-to-haves, such as caching IRs of modules and time-stamp caching entry point files, and embedding a specific version of Slang into itself to provide deterministic builds.

Aside from that, you know the drill: multithreaded file globbing and compilation.

Slang reflection is a W.I.P., so please do contribute any fixes and changes you find in your own use cases.

It also uses a declarative TOML file to make it easy to maintain:

```toml
# Save as jslang.toml
[Build]
SearchFolders = ["../../InternalLibraries", "../../PCG"]
OutputFolder = "build"
DoFileContentIntegrityChecks = false
MacroDefines = ["SOMETHING", "0"] # To apply to all targets

[[Build.Target]] # You can have multiple of these
# Output binary/text format:
    # extension_to_target_map = {
    #     {".glsl", SLANG_GLSL},
    #     {".hlsl", SLANG_HLSL},
    #     {".spv", SLANG_SPIRV},
    #     {".spvasm", SLANG_SPIRV_ASM},
    #     {".dxbc", SLANG_DXBC},
    #     {".dxbcasm", SLANG_DXBC_ASM},
    #     {".dxil", SLANG_DXIL},
    #     {".dxilasm", SLANG_DXIL_ASM},
    #     {".c", SLANG_C_SOURCE},
    #     {".cpp", SLANG_CPP_SOURCE},
    #     {".exe", SLANG_HOST_EXECUTABLE},
    #     {".cu", SLANG_CUDA_SOURCE},
    #     {".ptx", SLANG_PTX},
    #     {".metal", SLANG_METAL},
    #     {".metallib", SLANG_METAL_LIB},
    #     {".metallibasm", SLANG_METAL_LIB_ASM},
    #     {".wgsl", SLANG_WGSL},
    #     {".slangvm", SLANG_HOST_VM},
    #     {".h", SLANG_CPP_HEADER},
    #     {".cuh", SLANG_CUDA_HEADER},
    #     {".ll", SLANG_SHADER_LLVM_IR},
    #     {".dll", SLANG_SHADER_SHARED_LIBRARY},
    #     {".o", SLANG_OBJECT_CODE}};
Format = ".spv"

Profile = "sm_6_6"

# Optimization level: 0 (None), 1 (Default), 2 (High), 3 (Maximal)
OptimizationLevel = 3

# Compiler behavior flags
GenerateWholeProgram = false
GenerateSPIRVDirectly = true

# Memory alignment defaults: "column_major" or "row_major"
MatrixLayout = "column_major"

# Preprocessor definitions to pass to all shaders (Key-Value pair)
MacroDefines = ["DEBUG", "0"]
```

# Compiling it

On Windows, you'll need the MSVC toolchain. Universally, you'll need XMake.

Once you're setup, just run:

```bash
xmake
```

And you're done. To check if it compiled correctly (-v for verbose, -c for clean rebuild; you can do jslang build --help for more info):

```bash
jslang build -v -c
[INFO]   : Failed to find temporary directory. Creating a new one...
[INFO]   : Initializing TaskScheduler...
[INFO]   : Building with 16 thread(s).
[SUCCESS]: TaskScheduler initialized.
[INFO]   : Searching for Slang shaders...
[INFO]   : Searching for shaders in directory: ../../InternalLibraries
[INFO]   : Searching for shaders in directory: ../../PCG
[INFO]   : Found shader file: ../../InternalLibraries\MathLibraries\Noise\HashingFunctions.slang
[INFO]   : Found shader file: ../../InternalLibraries\MathLibraries\Noise\Simplex3D.slang
[INFO]   : Found shader file: ../../PCG\EnvironmentGenerator\Shaders\TestShader.slang
[SUCCESS]: Successfully found shaders (3 of them) and loaded their file paths onto memory. Proceeding with next step...
[INFO]   : Attempting to load companion dynamic libraries...
[WARN]   : Companion dynamic library file not found: slang-compiler.dll. Emitting from memory...
[WARN]   : Companion dynamic library file not found: slang-glsl.dll. Emitting from memory...
[WARN]   : Companion dynamic library file not found: slang-glslang.dll. Emitting from memory...
[WARN]   : Companion dynamic library file not found: slang-llvm.dll. Emitting from memory...
[WARN]   : Companion dynamic library file not found: slang-rt.dll. Emitting from memory...
[SUCCESS]: Loaded companion dynamic libraries.
[INFO]   : Attempting to load the Slang compiler...
[SUCCESS]: Successfully loaded Slang compiler onto memory. Proceeding to compile Slang shaders...
[INFO]   : Compiling for module: ../../PCG\EnvironmentGenerator\Shaders\TestShader.slang
[INFO]   : Compiling for 3 dependencies for module ../../PCG\EnvironmentGenerator\Shaders\TestShader.slang.
[INFO]   : Compiling dependency: F:\Openworld_Game\Undecided_Game_Proj\src\InternalLibraries\MathLibraries\Noise\HashingFunctions.slang
[SUCCESS]: Successfully compiled dependency module into an IR format.
[INFO]   : Compiling dependency: F:\Openworld_Game\Undecided_Game_Proj\src\InternalLibraries\MathLibraries\Noise\Simplex3D.slang
[SUCCESS]: Successfully compiled dependency module into an IR format.
[INFO]   : Compiling for 1 entry point(s) in TestShader.
[SUCCESS]: Shader compiled as build\TestShader_main.spv.
[INFO]   : Saving IR file HashingFunctions.slang...
[SUCCESS]: Successfully saved IR file.
[INFO]   : Saving IR file Simplex3D.slang...
[SUCCESS]: Successfully saved IR file.
[INFO]   : Build process took 467ms.
```

#### If you want a sample project to test this on; you can clone https://github.com/Jari31/Nebo-Fabrika/.

# Contributing

If you're looking to contribute, you can refer to the main project's [CONTRIBUTING.md file](https://github.com/Jari31/Nebo-Fabrika/blob/dev/CONTRIBUTING.md).

# Dependencies

| Name            | License          |
| --------------- | ---------------- |
| Slang           | Apache 2.0 / MIT |
| toml++          | MIT              |
| CLI11           | BSD-3-Clause     |
| xxHash          | BSD-2-Clause     |
| unordered_dense | MIT              |
| enkiTS          | zlib             |
| dylib           | MIT              |
| whereami        | WTFPL / MIT      |
| Zig             | MIT              |

# Q/A

## How do I change Slang versions?

It's fairly simple. Just go to xmake.lua, find:

```lua
add_requires("slang 2026.14.1", { configs = { binary = true } })
```

And then edit it as you'd like. For using custom .dlls, support is planned, but for now, that's what you can resort to.
