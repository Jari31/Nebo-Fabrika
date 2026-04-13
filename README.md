# Nebo Fabrika
A community project led by Jari.  Developed on Godot 4.5+.

# Supported Platforms (as of now)
Windows x64.

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
- Execute Code $-$ Allows for the execution of quite a lot of languages (e.g., C++, JavaScript, Python etc.).
- Juggl $-$ Allows for the use of CSS for more formatted and cleaner documentation.
- Reminder $-$ Allows for check-marks next to text, which makes tracking to-dos easier.


# Files
The file structure can get a bit confusing, so here are the current important folders you can look at:
|Files|
|-----|
|[C++ Source](Undecided_Game_Proj/src/)|
|[GLSL/GDShader Files](Undecided_Game_Proj/GDProject/bin/Shaders/)|

# About Open Source
This project is *semi-open source*[^1]. As in, the music and 3D assets are not open, but the code (aside from some exceptions) is.

|Exception|Example|
|---------|-------|
| Security related code | Server-client checksums |
| Anti-cheat |

***Files with copyright will be specified or omitted from the public.***

# Q/A
### Why Taichi Lang?
Taichi makes unit tests for parallel operations a million times easier. Alongside, it allows for me to be able to prototype much faster than through C++, GLSL and Godot.
In essence, it is more of a testing and prototyping tool rather than being a production tool.
### AI use?
Yes and no. So far, AI has been used to help write boiler plate and translate code from one language to another. But mostly, LLMs and diffusion models have not been used (so far) for other purposes.
### Why are so many of the folders named weirdly and with placeholders?
Mostly because of technical debt. The end user will not see it, so there's no point in spending precious man-hours on fixing them.

# License
[^1]: The licensing of this project is quite tricky. Again, any code you can see within the public repo is open source. Or more accurately, under the MIT license.

Though, crucially, some code has been derived from other projects, papers and such. And so copyright in those instances goes to their respective copyright holders.

THIS DOES NOT APPLY FOR THE PRIVATE REPO. If you see this within the private repository, ignore it, as then standard copyright applies (in respect to Copyright (c) 2026 Jari under the standard license).

PUBLIC REPO LICENSE: [License](Undecided_Game_Proj/LICENSE)