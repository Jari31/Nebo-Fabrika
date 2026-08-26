# Nebo Fabrika

A game project led by Jari. Developed on Godot 4.7+. Experimental.

## A brief overview

Nebo Fabrika is a 3D, FPS project about space exploration and colonization. Space exploration how? By procedural generation, utilizing both the CPU and GPU.

Currently, the project is in its early infancy. Gameplay is not in effect, but subsystems such as procedural generation have had their foundations set in place. Such as SIMD and SIMT optimized Simplex3D and SIMT optimized Dual Contouring (draft version; to be ported to Slang and optimized further).

This project is also split licensed like projects like DOOM or Quake. In other words, the repo you currently see is open source, but the assets (e.g., 3D models, audio files, 2D assets, etc.) are omitted and are proprietary.

Regardless, this project is huge in scope, and the scope creep is a part of the plan. As it spins off more subprojects (like JSlang, the build utility) that serve as a learning experience, even if subpar compared to production tools. For the project would be a platformer game with 3 levels if releasing to Steam was the goal.

**For more details, consult me (Jari).**

# Meta

### Supported platforms

Windows and Linux (x64/x86_64, aarch64/ARM64).

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

## Compiling For Windows

To begin, do:

```bash
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
Invoke-RestMethod -Uri https://get.scoop.sh | Invoke-Expression

# Great, now you have scoop. Now,
cd Undecided_Game_Proj

# After that:
scoop import scoopfile.json # Sit back and wait a minute,
                            # because it's going to be downloading the
                            # required binaries so you can get developing.
```

Great! Now ignore the linux nerds below, and jump to [^using_xmake].

## Compiling on Linux

Since Linux does not have Scoop unlike Windows, you have to resort to XMake. To start, do:

```bash
curl -fsSL https://xmake.io/shget.text | bash
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

And we're done! Just kidding. There's some quirks with for Windows. Why? Because most libraries pre-build for MSVC (Microsoft's compiler toolchain), which means that you'd either have to build giant libraries like Slang from source (easily takes half an hour) to compile seamlessly for the GNU toolchain (which Zig embeds, meaning you get 1 click compilation), or just use the MSVC compiler. Now thankfully, you don't have to use the MSVC toolchain, since the Zig build script handles that with lld-link.

But for legal reasons, the Zig team, and we too, can not embed the required binaries and headers to compile for the MSVC ABI for C++. To fix that, **you** need to download the required binaries. The build script can not do it for you automatically, since you need to accept Microsoft's license.

Anyway, before we move onto the next step, copy the file from Undecided_Game_Proj/zig godot-cpp build scripts/SConstruct and replace the SConstruct inside Undecided_Game_Proj/godot-cpp/. You will need to use the MSVC compiler for this step if you're compiling for Windows (ironic, I know, but that's just how vendor lock in works). If you're on Linux, you can look at [MSVC WINE](https://github.com/mstorsjo/msvc-wine). MSVC WINE hasn't been tested against godot-cpp within this repo, but please do document it if you do figure out how to use MSVC WINE to compile for godot-cpp.

Either way, do:

```bash
cd Undecided_Game_Proj/godot-cpp/

scons target=template_debug dev_build=yes use_static_cpp=no

scons target=template_release dev_build=no use_static_cpp=no
```

### Finally

Open up an administrator shell:

```bash
cd Undecided_Game_Proj/src

xrepo env xwin --accept-license splat --output .xwin --disable-symlinks # this downloads the required binaries
                                                                        # and headers to compile for the MSVC ABI.
                                                                        # you can safely ignore this if you don't want to
                                                                        # compile for the MSVC ABI

xmake # or, if you'd prefer: zig build -DLibraryName='ALL' -DCompileFromDirectory='ALL'
```

It will automatically assume it's a debug build. Now just boot up Godot 4.7+, and go to Undecided_Game_Proj\GDProject. **Congrats, you're done!**

If you want more information about the build systems, never forget that you can do,

```bash
    xmake --help

    # and

    zig build --help # in the /src/ folder
```

# License

This project itself uses the Mozilla Public License (MPL 2.0). Check it [here](LICENSE.md).

This project uses libraries with their own licenses. For example:

| Library     | License                        |
| ----------- | ------------------------------ |
| Slang       | Apache 2.0 with LLVM exception |
| EnkiTS      | zlib                           |
| AngelScript | zlib                           |
| CPUInfo     | BSD 2-Clause                   |
| Tracy       | BSD 3-Clause                   |
| Godot       | MIT                            |

---

Submodules:

## JSlang

| Library         | License                        |
| --------------- | ------------------------------ |
| Slang           | Apache 2.0 with LLVM exception |
| EnkiTS          | zlib                           |
| CLI11           | BSD 3-Clause                   |
| xxHash          | BSD 2-Clause                   |
| unordered_dense | MIT                            |

---

Windows SDK and C++ runtime apply with their own licenses.

# Q/A

## What IDE should I use?

Use Zed. It's faster than VS Code and doesn't eat 4 gigs of RAM to just sit there doing nothing.

But of course, keep VS Code in the back, as only VS Code has an extension for ISPC.

## Why Zig if you're already using XMake?

Because XMake isn't really that great for making complex build graphs. To put it simply, setting up a build system for godot-cpp is easier with Zig.

Zig itself is a drop-in C/C++ compiler and linker. You don't need to download the whole nine yards to compile for Linux from Windows. Even if godot-cpp forces it in some places.

## What even is ISPC and Slang?

ISPC is just Intel's SPMD compiler. To put it simply, your CPU has threads, and those threads have vector units. Those vector units have a certain amount of lanes (imagine them as mini-threads). Those lanes can process data in parallel, much like threads themselves.

Or, even more simply: Your CPU has workers, and those workers have machines. Those machines can do a ton of work in one go. This is called Single Instruction, Multiple Data (SIMD). SPMD is Single Program, Multiple Data.

ISPC lets you write linear looking code, and the compiler automatically writes jobs for the machine. So the code you write actually looks very simple, which makes maintaining and writing it a joy. Quite unlike manual intrinsics...

---

Slang is basically just if HLSL (Microsoft's shading language) wasn't painful to use. It's hard to explain what it does better than GLSL if you've never used GLSL or HLSL.

Much like ISPC, it compiles for parallel execution (Single Instruction, Multiple Threads (SIMT), which is SPMD wearing a different trench coat), but on the GPU (ISPC supports GPUs, but mainly for Intel GPUs).

## Why ISPC and Slang?

Because C++ intrinsics suck, as you have to essentially write the same piece of code for 8 different archs, or have a code generator look at your code and generate the vectorized loops. At which point, you're just recreating ISPC.

As for why Slang? GLSL is awful to write in. No proper LSP, no generics, no entrypoints... the list goes on.

## What's with the weird directory naming?

Well... Pick your poison at the start, and then you can't change it without spending 10 hours tracking down every reference. That's the short of it.

## Why is the build system so complex?

Because cross-compiling with godot-cpp is... Well, complex. For example, the main build.zig script inside /src/ is about 600 lines (although, admittedly, it's a bit over-engineered in some aspects). Just to compile C++. Meanwhile, the one in src/Tools/JSlangBuild is about 45 lines, which compiles an entire program.

But hey, incremental builds within a second! (To improve in Zig 0.17, but that's a few more months ahead.)

## Why so many languages?

Because they do one thing, and one thing well. If, for example, we were to use C++ SIMD intrinsics, you'd have to hope and pray to the compiler, "Oh please, please don't alias this pointer! Please vectorize this loop! Please don't have me write for NEON again! I don't want to read assembly, please tell me when you aren't actually vectorizing my code!" Or, again, make a custom eDSL that ISPC already handles.

**ISPC guarantees it.** And it also yells at you if you're destroying performance.

## Can I use AI?

Depends.

### Vibe coding?

Absolutely not.

### Generating boilerplate?

Sure, as long as you understand most of it.

### Using it to google?

Fine, but make sure to test and understand it fully before committing.

### Using it as a teacher?

A bit iffy for production, but as long as you are writing every single line of it, and you can explain every single line of it.

The rule of thumb is that **you** need to _own every single line of code._ You must know the _why_ and _how._

---

Remember to have fun!
