# Nebo Fabrika
A community project led by Jari.  Developed on Godot 4.5+.

# Supported Platforms (as of now)
Windows x64 and Linux x64. 

# Documentation
The obsidian .mds within the [Obsidian folder](Undecided_Game_Proj/Obsidian%20Files) explain every algorithm currently implemented and show diagrams to visualize the flow. Not every algorithm is explained thoroughly, but we (me - Jari, currently) are working on it.

### Color-coding for nodes within Obsidian:
|Legends|Def|
|-------|---|
|Color: *Red* | Unfinished and WIP code |
|Color: *Green* | Finished code |
|Color: *Yellow* | Bugged code |
|Color: *Blue*|Pure documentation notes|

### Recommended Obsidian Extensions
*Though you do not need them to view the documentation, you will benefit massively by having them.*
- DataviewJS $-$ Allows for WebGL and WebGPU use within Obsidian itself. 
- Execute Code $-$ Allows for the execution of quite a lot of languages (e.g., C++, JavaScript, Python etc.)
- Juggl $-$ Allows for the use of CSS for more formatted and cleaner documentation.
- Reminder $-$ Allows for check-marks next to text, which makes tracking to-dos easier.


# Files
The file structure can get a bit confusing, so here are the current important folders you can look at:
|Files|
|-----|
|[C++ Source](Undecided_Game_Proj/src/)|
|[GLSL/GDShader Files](Undecided_Game_Proj/GDProject/bin/Shaders/)|

# Variable Naming Conventions
*outdated; will be updated soon.

The naming convention has been quite mixed overtime. Though project mostly uses snake-case and Pascal-case. Either in mixture, or exclusively. For now, snake-case is mostly for smaller functions. Main functions must use Pascal-case. You might see camel-case here and there, but they are a relic of when the project started out. 

Verbose naming is a must.

### A few legends:
UB $-$ Universal Base. Classes (or functions) that act as basses for other objects to inherit.
# Compilation
This project is only suited for cross compilation across Windows and Linux. Currently, [this script](Undecided_Game_Proj/PythonScripts/GDExtensionBuild//GDExtensionBuild_CPP.py) allows for cross compilation with Ubuntu. Outside of that, this project supports Zig as a build system.

First, you need to compile [godot-cpp](Undecided_Game_Proj/godot-cpp/) to link C++ against it. Open the folder, paste in the [build.zig file](Undecided_Game_Proj/zig%20godot-cpp%20build%20scripts/), open the folder in your terminal this time, then run:
```bash
zig build
```

Now sit back for half an hour and let it cook.

### Scons build system with python
You will need to recompile godot-cpp with SCons instead of Zig. I do not recommend this path.

To setup Ubuntu, you must install WSL on your Windows machine. Afterwards, install Ubuntu, and then run: 
```bash
sudo apt update && sudo apt install -y build-essential scons git ccache
```

Then you can proceed by using the Python script.

For building specific modules, you can setup your build function call as:
```bash
scons --target=PCG_Environment --targetFolder='Procedural Environment Generator' --productionBuild=0
```

### Zig build system for C++
Or, for a maintained build script for the C++ backend, download Zig. Afterwards, navigate to [src](Undecided_Game_Proj/src/) and use:
```bash 
zig build
``` 

# About Open Source
This project is *semi-open source*[^1]. As in, the music and 3D assets are not open, but the code (aside from some exceptions) is.

|Exception|Example|
|---------|-------|
| Security related code | Server-client checksums |
| Anti-cheat |

***Files with copyright will be specified or omitted from the public.***

# Q/A
### Why Taichi Lang?
Taichi makes unit tests for parallel operations a million times easier. Alongside, it allows for prototyping much faster than through C++, GLSL and Godot.
In essence, it is more of a testing and prototyping tool rather than being a production tool.
### AI use?
Yes and no. So far, AI has been used to help write boiler plate and translate code from one language to another. But mostly, LLMs and diffusion models have not been used (so far) for other purposes.
### Why are so many of the folders named weirdly and with placeholders?
Mostly because of technical debt. The end user will not see it, so there's no point in spending precious man-hours on fixing them.
