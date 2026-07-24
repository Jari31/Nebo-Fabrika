# Nebo Fabrika

A game project led by Jari. Developed on Godot 4.5+.

# IF YOU ARE A DESIGNER/ARTIST

This is not for you (but still try to follow the compilation process to setup the project). Refer to the Google Doc for more info.

# Meta

### Supported platforms

Windows x64 and Linux x64.

### Expected language versions

| Language     | Version  |
| ------------ | -------- |
| C++          | 23       |
| Zig          | 0.16     |
| XMake        | 3.0+     |
| Shader Slang | 2026.10+ |
| GLSL         | 430      |
| ISPC         | 1.30+    |

> [!Note]
> GLSL is used in some legacy places, but heavily discouraged. Use Shader Slang instead.

# Compilation

This project is only suited for cross compilation across Windows and Linux.

## Prerequisites

**If you're on windows,** Scoop (any version that supports importing settings). And/or-else, XMake.

---

## Compiling on Windows

To begin, do:

```bash
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
Invoke-RestMethod -Uri https://get.scoop.sh | Invoke-Expression

# Great, now you have scoop. Now,
cd Undecided_Game_Proj

# After that:
scoop import scoopfile.json # Sit back and wait a minute, because it's going to be downloading the required binaries so you can get developing.

winget install Microsoft.VisualStudio.2022.BuildTools
```

Great! Now ignore the linux nerds below, and jump to [^using_xmake].

## Compiling on Linux

Since Linux does not have Scoop unlike Windows, you have to resort to XMake. To start, do:

```bash
curl -fsSL https://xmake.io/shget.text | bash

xrepo install doxygen graphviz ispc slang zig # slang might be outdated (v2025) for xrepo, so if that becomes an issue, either download it through the website, or install the Vulkan SDK

xrepo env
```

_*IF*_ this does not work for you, refer to: https://xmake.io/posts/quickstart-1-installation.html and install it in the other ways XMake provides.

## Actually compiling

([^using_xmake])

```bash
cd Undecided_Game_Proj/src

xmake # Just wait a second or two and let it cook

zig build
```

### Wait a second!

And we're done! Just kidding. There's some quirks with Zig compiling for godot-cpp because the godot-cpp build system uses MSVC on and for Windows. You CAN avoid that by rewriting parts of the SCons build scripts to fully use Zig, but at that point, setting up a docker environment is easier. **Take my advice:** If you're on Linux, just compile for Linux and be done with it, if you don't want to setup a docker environment. If you're on Windows, you can compile for both Linux and Windows.

Alright, so now copy the file from Undecided_Game_Proj/zig godot-cpp build scripts/SConstruct and replace the SConstruct inside Undecided_Game_Proj/godot-cpp/. Afterwards,

```bash
cd Undecided_Game_Proj/godot-cpp/

scons target=template_debug dev_build=yes use_static_cpp=no

scons target=template_release dev_build=no use_static_cpp=no
```

### Finally

```bash
cd Undecided_Game_Proj/src

zig build -DLibraryName='ALL' -DCompileFromDirectory='ALL'
```

It will automatically assume it's a debug build. Now just boot up Godot 4.7+, and go to Undecided_Game_Proj\GDProject. **Congrats, you're done!**

If you want more information about the build systems, never forget that you can do,

```bash
    xmake --help

    # and

    zig build --help # in the /src/ folder
```

# Q/A

## What IDE should I use?

Use Zed. It's faster than VS Code and doesn't eat 4 gigs of RAM to just sit there doing nothing.

But of course, keep VS Code in the back, as only VS Code has an extension for ISPC.

## Why Zig if you're already using XMake?

Because XMake isn't really that great for making complex build graphs. To put it simply, setting up a build system for godot-cpp is easier with Zig.

## What even is ISPC and Slang?

ISPC is just Intel's SPMD compiler. To put it simply, your CPU has threads, and those threads have vector units. Those vector units have a certain amount of lanes (imagine them as mini-threads). Those lanes can process data in parallel, much like threads themselves.

Or, even more simply: Your CPU has workers, and those workers have machines. Those machines can do a ton of work in one go. This is called Single Instruction, Multiple Data (SIMD)

ISPC lets you write linear looking code, and the compiler automatically writes jobs for the machine. So the code you write actually looks very simple, which makes maintaining and writing it a joy. Quite unlike manual intrinsics...

---

Slang is basically just if HLSL (Microsoft's shading language) wasn't painful to use. It's hard to explain what it does better than GLSL if you've never used GLSL or HLSL.

Much like ISPC, it compiles for parallel execution, but on the GPU (ISPC supports GPUs, but mainly for Intel GPUs).

## Why ISPC and Slang?

Because C++ intrinsics suck, as you have to essentially write the same piece of code for 8 different archs, or have a code generator look at your code and generate the vectorized loops. At which point, you're just recreating ISPC.

As for why Slang? GLSL is awful to write in. No proper LSP, no generics, no entrypoints... the list goes on.

## What's with the weird directory naming?

Well... Pick your poison at the start, and then you can't change it without spending 10 hours tracking down every reference. That's the short of it.

## Why is the build system so complex?

Because cross-compiling with godot-cpp is... Well, complex. For example, the main build.zig script inside /src/ is about 600 lines. Just to compile C++. Meanwhile, the one in src/Tools/JSlangBuild is about 45 lines, which compiles an entire program.

But hey, incremental builds within a second! (To improve in Zig 0.17, but that's a few more months ahead.)

# Why so many languages?

Because they do one thing, and one thing well. If, for example, we were to use C++ SIMD intrinsics, you'd have to hope and pray to the compiler, "Oh please, please don't alias this pointer! Please vectorize this loop!" Or, again, make a custom eDSL that ISPC already handles.

**ISPC gurantees it.** And it also yells at you if you're destroying performance.

---

Remember to have fun!
