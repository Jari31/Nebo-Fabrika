// This script assumes that the project structure is as the same one in the repo
//
// Mac is broken.
// It also has a different GPU arch, which will definitely brick some of the compute shaders.
//
// (*there is a Mac branch, but I have not tested it much due to me lacking a Mac)
//
// Standard command to build everything: zig build -DLibraryName="ALL" -DCompileFromDirectory='ALL' -Dtarget=x86_64-linux
// Or, for Windows, inside of 'x64 Native Tools Command Prompt for VS 2022': zig build -DLibraryName="ALL" -DCompileFromDirectory='ALL' -Dtarget=x86_64-windows

const std = @import("std");
const compile_commands = @import("compile_commands");
const builtin = @import("builtin");

var FileExtension: []const u8 = ".cpp"; // mostly redundant because changing this WILL break everything
const RegistrationFilePrefix: []const u8 = "Register_";
var BuildFolderPath = "../GDProject/bin/build/";
var SkipDebugFlag: bool = false;

const IncludeFiles = struct {
    ObjectFiles_ISPC: std.ArrayList([]const u8) = .empty,
    StaticFiles_LibraryCache: std.ArrayList([]const u8) = .empty,
    // where there might be more ObjectFiles_C
    const GlobFileOptions = struct {
        DirectoryPath: []const u8 = "cache/ispc",
        FileWithExtension: []const u8 = ".obj",
        ExcludeFileWith: std.ArrayList([]const u8) = .empty,
    };

    pub fn GlobFilesInDirectory(
        Build: *std.Build,
        ListToAppendTo: *std.ArrayList([]const u8),
        Options: GlobFileOptions,
    ) void {
        const io = Build.graph.io;

        var directory = Build.build_root.handle.openDir(io, Options.DirectoryPath, .{ .iterate = true }) catch @panic("OOM");
        defer directory.close(io);

        var directory_walker = directory.walk(Build.allocator) catch @panic("OOM");
        defer directory_walker.deinit();

        while (directory_walker.next(io) catch null) |entry| {
            if (entry.kind == .file and std.mem.endsWith(
                u8,
                entry.path,
                Options.FileWithExtension,
            )) {
                if (Options.ExcludeFileWith.items.len > 0) {
                    var contains_blacklisted_character = false;
                    for (Options.ExcludeFileWith.items) |ExcludedCharacter| {
                        if (std.mem.containsAtLeast(u8, entry.path, 1, ExcludedCharacter)) {
                            std.debug.print("\x1b[33mFile ({s}) contains blacklisted identifier \x1b[1;4;33m'{s}'\x1b[0;33m. Ignoring...\x1b[0m\n", .{ entry.basename, ExcludedCharacter });
                            contains_blacklisted_character = true;
                            break;
                        }
                    }

                    if (contains_blacklisted_character) continue;
                }
                const found_path = if (std.mem.endsWith( // ugly way to do it. but it works, so eh
                    u8,
                    Options.DirectoryPath,
                    "/",
                )) Build.fmt("{s}{s}", .{
                    Options.DirectoryPath,
                    entry.path,
                }) else Build.fmt("{s}/{s}", .{
                    Options.DirectoryPath,
                    entry.path,
                });
                std.debug.print("Found {s} file, appending full path as: {s}\n", .{ Options.FileWithExtension, found_path });
                ListToAppendTo.append(Build.allocator, found_path) catch @panic(Build.fmt(
                    "Failed to appent to list whilst walking the cache directory for {s} in '{s}'.\n",
                    .{ FileExtension, Options.DirectoryPath },
                ));
            }
        }
    }

    pub fn DeInitializeObjects(Self: *IncludeFiles, Allocator: std.mem.Allocator) void {
        Self.ObjectFiles_ISPC.deinit(Allocator);
    }
};

const CompilationOptions = struct {
    Name: []const u8 = "PCG_Environment",
    CompileFromDirectory: []const u8 = "Procedural Environment Generator",
    DebugBuild: bool = true,

    FileIncludes: ?IncludeFiles = null,

    pub fn DeInitializeMembers(Self: *CompilationOptions, Allocator: std.mem.Allocator) void {
        if (Self.FileIncludes) |*FileInclude| {
            FileInclude.DeInitializeObjects(Allocator);
        }
    }
};

// idk why this function is here but whatever ig
fn _print_unsupported_build(Tag: anytype) void {
    std.debug.print("Unsupported build platform target: {}\n", .{Tag});
}

fn _add_macros_and_includes(Module: *std.Build.Module, Build: *std.Build) void {
    Module.addIncludePath(Build.path("../godot-cpp/include"));
    Module.addIncludePath(Build.path("../godot-cpp/gen/include"));
    Module.addIncludePath(Build.path("../godot-cpp/gen/core/include"));
    Module.addIncludePath(Build.path("../godot-cpp/gdextension"));
    Module.addIncludePath(Build.path("../godot-cpp/gen/gdextension/include"));
    Module.addIncludePath(Build.path("cache/"));
    Module.addIncludePath(Build.path("InternalLibraries/"));

    Module.addCMacro("TYPED_METHOD_BIND", "1"); // technically pointless because the flag-list already contains this as a flag, but eh
}

fn _read_meta_file_and_compile(
    p_Build: *std.Build,
    Allocator: std.mem.Allocator,
    Target: std.Build.ResolvedTarget,
    Optimize: std.builtin.OptimizeMode,
    Options: *CompilationOptions,
) void {
    // Follows this format:
    // # This is a comment.
    // # List your file paths as path/to/something:file_something
    // # Also: ../external lib/something:entry_point

    // Procedural Environment Generator:PCG_Environment
    // Shader compiler:ShaderComp
    // ThreadPhysics/ECS Pool:ECS_Particles

    const io = p_Build.graph.io;

    const max_file_size = 100 * 1024 * 1024;
    const read_buffer = p_Build.build_root.handle.readFileAlloc(
        io,
        "meta",
        Allocator,
        std.Io.Limit.limited(max_file_size),
    ) catch |err|
        {
            std.debug.print("Failed to read file into the read buffer with error: {}", .{err});
            return;
        };
    defer Allocator.free(read_buffer);

    var lines = std.mem.splitSequence(u8, read_buffer, "\n");
    std.debug.print("\x1b[32mLoaded meta file onto memory. Proceeding to read and compile...\x1b[0m\n", .{});
    while (lines.next()) |line| {
        var clean_line = std.mem.trim(u8, line, "\r");

        if (clean_line.len == 0) continue;

        { // comments
            const stripped_line = std.mem.trim(u8, clean_line, " \t\r\n");

            const Lambda_CheckLine = struct {
                pub inline fn CheckWhether_Character_Exists(
                    CleanLine: *[]const u8,
                    Line: []const u8,
                    StrippedLine: []const u8,
                    Character: []const u8,
                ) void {
                    if (std.mem.startsWith(u8, StrippedLine, Character)) {
                        CleanLine.* = "";
                    } else if (std.mem.indexOfScalar(u8, StrippedLine, Character[0])) |index| CleanLine.* = Line[0..index];
                }
            };

            Lambda_CheckLine.CheckWhether_Character_Exists(&clean_line, line, stripped_line, "#");
            Lambda_CheckLine.CheckWhether_Character_Exists(&clean_line, line, stripped_line, "<");

            if (clean_line.len <= 0) continue;
        }

        std.debug.print("Found line: {s}\n", .{clean_line});

        var parts = std.mem.splitSequence(u8, clean_line, ":");

        if (parts.next()) |key| {
            if (parts.next()) |value| {
                const raw_folder = std.mem.trim(u8, key, " \t"); // needs to strip again because of "something : something" type syntax
                const folder = std.fmt.allocPrint(Allocator, "{s}/", .{raw_folder}) catch |err|
                    // moves it to the heap since otherwise it gets corrupted into:
                    // error: unable to update file from '.zig-cache\o\004797be9ffb1d8f419dced72c944d2c\libShaderComp.so' to 'F:\Openworld_Game\Undecided_Game_Proj\GDProject\bin\build\¬¬¬¬¬¬¬¬¬¬\libShaderComp.so': BadPathName
                    // something to do with the build graph
                    {
                        std.debug.print("Failed to format 'folder' for compilation whilst trying to read meta file with error: {}", .{err});
                        return;
                    };

                const raw_file = std.mem.trim(u8, value, " \t");
                const file = Allocator.dupe(u8, raw_file) catch |err| {
                    std.debug.print("Failed to allocate space on memory for 'file' with error: {}", .{err});
                    return;
                };

                std.debug.print("Found folder: {s}\nFound file: {s}\nChecking whether they exist.\n", .{ folder, file });

                const Lambda_CheckFileSystem = struct {
                    /// Expects . (dot) syntax for extension
                    pub fn FileWith_Extension_Exists(
                        Build: *std.Build,
                        IO: std.Io,
                        Folder: []const u8,
                        File: []const u8,
                        Prefix: []const u8,
                        Extension: []const u8,
                    ) bool {
                        const _file = Build.fmt("{s}{s}{s}", .{ Prefix, File, Extension });
                        const file_path = Build.fmt("{s}{s}", .{ Folder, _file });

                        std.debug.print("\x1b[33mChecking file: {s}\x1b[0m\n", .{file_path});

                        Build.build_root.handle.access(IO, file_path, .{}) catch |err|
                            switch (err) {
                                error.FileNotFound => {
                                    std.debug.print("\x1b[31mFailed to find {s}\x1b[0m\n", .{_file});
                                    return false;
                                },
                                else => {
                                    std.debug.print("\x1b[31mFailed to find file with error: {}\x1b[0m\nExiting to avoid further errors.\n", .{err});
                                    return false;
                                },
                            };
                        std.debug.print("\x1b[32mFile exists.\x1b[0m Continuing...\n", .{});
                        return true;
                    }
                };

                var file_exists = true;

                const CheckFilePrefixesAndExtensions = [_]struct { Prefix: []const u8, Extension: []const u8 }{
                    .{ .Prefix = "", .Extension = ".cpp" },
                    //.{ .Prefix = "", .Extension = ".h" },
                    //.{ .Prefix = "", .Extension = ".hpp" },
                    .{ .Prefix = RegistrationFilePrefix, .Extension = ".cpp" },
                    .{ .Prefix = RegistrationFilePrefix, .Extension = ".h" },
                };

                for (CheckFilePrefixesAndExtensions) |Pair| {
                    file_exists = Lambda_CheckFileSystem.FileWith_Extension_Exists(p_Build, io, folder, file, Pair.Prefix, Pair.Extension);

                    if (!file_exists) break;
                }

                if (!file_exists) {
                    std.debug.print("\x1b[31mRequired files for the compilation of \x1b[4;31m{s}\x1b[0m\x1b[31m not found.\x1b[0m Attempting to continue...\n", .{file});
                    continue;
                }

                Options.Name = file;
                Options.CompileFromDirectory = folder;

                _compile(p_Build, Target, Optimize, Allocator, Options);
            }
        }
    }
}

fn _compile(
    p_Build: *std.Build,
    Target: std.Build.ResolvedTarget,
    Optimize: std.builtin.OptimizeMode,
    Allocator: std.mem.Allocator,
    Options: *CompilationOptions,
) void {
    const os_tag = Target.result.os.tag;

    const optimization_level = if (!Options.DebugBuild and !SkipDebugFlag) std.builtin.OptimizeMode.ReleaseFast else Optimize; // default for Optimize is Debug

    p_Build.install_path = p_Build.path(BuildFolderPath).getPath(p_Build);

    const is_msvc_abi = if (Target.result.abi == .msvc) true else false;

    const base_module = p_Build.createModule(.{
        .root_source_file = p_Build.path("Zig/ZigRegistry.zig"),
        .target = Target,
        .optimize = optimization_level,
        .link_libc = true,
        .link_libcpp = !is_msvc_abi,
    });

    const godot_cpp_lib = switch (Options.DebugBuild) { // Can compile to Android, and such, too, but that's out of this project's scope currently
        true => switch (os_tag) {
            .windows => p_Build.path("../godot-cpp/bin/libgodot-cpp.windows.template_debug.dev.x86_64.lib"),
            .linux => p_Build.path("../godot-cpp/bin/libgodot-cpp.linux.template_debug.dev.x86_64.a"),
            .macos => p_Build.path("../godot-cpp/bin/libgodot-cpp.macos.template_debug.dev.x86_64.a"),

            else => {
                _print_unsupported_build(os_tag);
                return;
            },
        },
        false => switch (os_tag) {
            .windows => p_Build.path("../godot-cpp/bin/libgodot-cpp.windows.template_release.x86_64.lib"),
            .linux => p_Build.path("../godot-cpp/bin/libgodot-cpp.linux.template_release.x86_64.a"),
            .macos => p_Build.path("../godot-cpp/bin/libgodot-cpp.macos.template_release.x86_64.a"),

            else => {
                _print_unsupported_build(os_tag);
                return;
            },
        },
    };

    const file_path = p_Build.fmt(
        "{s}{s}{s}",
        .{ Options.CompileFromDirectory, Options.Name, FileExtension },
    );
    std.debug.print("File path: {s}\n", .{file_path});
    const registration_path = p_Build.fmt(
        "{s}{s}{s}{s}",
        .{ Options.CompileFromDirectory, RegistrationFilePrefix, Options.Name, FileExtension },
    );

    var flag_list = std.ArrayList([]const u8).empty;
    defer flag_list.deinit(Allocator);

    //transliterated from godot-cpp py scripts
    //base_module.addCMacro("GDEXTENSION", "1");

    if (!Options.DebugBuild) {
        base_module.addCMacro("PRODUCTION_BUILD", "");
        //base_module.addCMacro("NDEBUG", "1");
    } //else {
    //     base_module.addCMacro("DEBUG_ENABLED", "1");
    //     base_module.addCMacro("HOT_RELOAD_ENABLED", "1");
    // }

    // if you need platform specific flags. I found most of these to be unneeded, but this can be a good LuT if you're just looking
    switch (os_tag) {
        .windows => {
            //base_module.addCMacro("WINDOWS_ENABLED", "1");
            //base_module.addCMacro("NOMINMAX", "1");

            if (Target.result.abi == .msvc) {
                //base_module.addCMacro("_HAS_EXCEPTIONS", "0");
                flag_list.appendSlice(Allocator, &.{
                    "-fms-runtime-lib=dll",
                    //    "-fno-rtti",
                }) catch @panic("OOM");

                //base_module.linkSystemLibrary("msvcrt", .{});
                //base_module.linkSystemLibrary("vcruntime", .{});

                //flag_list.append(Allocator, "-fms-runtime-lib=static") catch @panic("OOM");

                //flag_list.appendSlice(Allocator, &.{
                //    "-fms-runtime-lib=static",
                //    "-Xclang",
                //    "--dependent-lib=libcmt",
                //}) catch @panic("OOM");
            }
        },
        .linux => {
            //std.debug.print("Appending Linux flags...\n", .{});
            flag_list.appendSlice(Allocator, &.{
                "-fPIC", // Linux needs this for .so, as otherwise the memory gets corrupted, fast
                "-Wl,-s",

                // gets rid of dead code
                "-Wl,--gc-sections",
                "-ffunction-sections",
                "-fdata-sections",
            }) catch @panic("OOM");
        },

        .macos => {
            flag_list.appendSlice(Allocator, &.{
                // without these flags, MacOS has temper tantrums over what it expects long to mean vs what godot templates makes it out to be
                // i.e., long is 64bit on MacOS; long is 32bit on Linux and Windows
                "-D__STDC_INT64__",
                "-mlong-double-64",
                //"-fno-emulated-tls",
                "-D_GODOT_CPP_AVOID_THREAD_LOCAL",
                "-pthread",
            }) catch @panic("OOM");
        },
        else => {},
    }

    if (optimization_level == .Debug) {
        flag_list.appendSlice(Allocator, &.{
            "-g", "-fno-omit-frame-pointer", "-DTRACY_ENABLE",
        }) catch @panic("OOM");
    } else {
        flag_list.appendSlice(Allocator, &.{"-s"}) catch @panic("OOM");
    }

    const cpp_optimization_level = if (Options.DebugBuild) "-O2" else "-O3";
    flag_list.appendSlice(Allocator, &.{
        "-std=c++23",
        cpp_optimization_level,
        "-Wall",
        //"-Werror",
        "-Wextra",
        "-Wno-gnu-anonymous-struct",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-DTYPED_METHOD_BIND",

        // these three drop binary sizes from 128mbs to 270kbs
        "-fno-sanitize=undefined",
        "-fno-sanitize-trap=all",
        "-fno-exceptions",
        "-fvisibility=hidden", // godot-cpp uses this, and it drops binary sizes by 2 kbs
        "-fno-asynchronous-unwind-tables", // godot-cpp doesn't use this, but it does drop binary sizes by 3kbs (average from multiple files)

        // #embed
        "-Wno-c23-extensions",
    }) catch @panic("OOM");

    const Lambda_IncludeFileHelper = struct {
        /// ListOfPathsToObjectFiles is the field of _Options
        pub inline fn AddIncludedFiles(
            Build: *std.Build,
            Function_AddObjectFile: anytype,
            Self: anytype,
            ListOfPathsToObjectFiles: anytype,
        ) void {
            const FunctionType = @typeInfo(@TypeOf(Function_AddObjectFile)).@"fn";
            //const FirstParameterType = FunctionType.params[0].type.?;
            if (ListOfPathsToObjectFiles.items.len == 0) {
                std.debug.print("\x1b[33mList of paths passed to includer, but is empty.\n\x1b[0m", .{});
                return;
            }

            for (ListOfPathsToObjectFiles.items) |ObjectFilePath| {
                switch (FunctionType.params.len) {
                    2 => {
                        Function_AddObjectFile(Self, Build.path(ObjectFilePath));
                    },
                    else => {
                        @panic("Unkown function signature. Exiting to avoid corruption.");
                    },
                }
            }
        }
    };

    if (!is_msvc_abi) {
        _add_macros_and_includes(base_module, p_Build);

        base_module.addCSourceFiles(.{
            .files = &.{
                file_path,
                registration_path,
            },
            .flags = flag_list.items,
        });

        base_module.addObjectFile(godot_cpp_lib);

        const library = p_Build.addLibrary(.{
            .name = Options.Name,
            .linkage = .dynamic,
            .root_module = base_module,
        });

        // these two drop binary sizes from 10mbs to 1.25mbs
        if (optimization_level != .Debug) {
            library.root_module.strip = true;
            library.link_gc_sections = true;
        }

        Lambda_IncludeFileHelper.AddIncludedFiles(p_Build, std.Build.Module.addObjectFile, library.root_module, &Options.FileIncludes.?.ObjectFiles_ISPC);
        Lambda_IncludeFileHelper.AddIncludedFiles(p_Build, std.Build.Module.addObjectFile, library.root_module, &Options.FileIncludes.?.StaticFiles_LibraryCache);

        const artifacts = p_Build.addInstallArtifact(
            library,
            .{ .dest_dir = .{ .override = .{ .custom = Options.Name } } },
        );

        p_Build.getInstallStep().dependOn(&artifacts.step);
    } else {
        // If you don't use MSVC's linker.exe, it segfaults Godot because Godot expects MSVC ABIs and not LLVM
        // You could avoid this and use the steps above if you compiled Godot with MinGW instead of MSVC (MSVC genuinely is so ass)
        // I recommend doing that if you are on Linux and don't want to setup a VM or a WINE environment
        // Also, you need to run this in a 'x64 Native Tools Command Prompt for VS 2022'
        // Mostly because Zig currently doesn't have an LTS WindowsSDK API and I didn't want any external dependencies (Aside from MSVC,-
        //  -but again, you can just compile Godot with MinGW or Zig)
        // If you're using Linux Godot, you can ignore this step.
        const files_to_compile = &[_][]const u8{
            file_path,
            registration_path,
        };

        const folder_path = std.fmt.allocPrint(
            Allocator,
            "{s}{s}",
            .{ BuildFolderPath, Options.Name },
        ) catch @panic("OOM");

        const io = p_Build.graph.io;

        std.Io.Dir.createDirPath(std.Io.Dir.cwd(), io, folder_path) catch |err| {
            std.debug.print("Failed to create directory {s}: {}\n", .{ folder_path, err });
            @panic("Directory creation failed");
        };

        std.debug.print("Creating a subfolder at: {s}\n", .{folder_path});

        const output_location = std.fmt.allocPrint(
            Allocator,
            "/OUT:{s}{s}/{s}.dll",
            .{ BuildFolderPath, Options.Name, Options.Name },
        ) catch @panic("OOM");
        std.debug.print("Linker outputting to: {s}", .{output_location});

        const linker_step = p_Build.addSystemCommand(&.{"link.exe"});

        linker_step.addArgs(&.{ "/DLL", output_location, "/NOLOGO" });

        if (optimization_level == .Debug) { // Microslo- I mean linker.exe can't see that a null character isn't valid and ignore it like every other linker, causing it to crash if you try to do ternanry operations like that
            linker_step.addArgs(&.{"/DEBUG"});
        }

        inline for (files_to_compile, 0..) |source_file, i| {
            const step_name = std.fmt.allocPrint(Allocator, "{s}_{d}", .{ Options.Name, i }) catch @panic("OOM");
            std.debug.print("Linking for step: {s}\n", .{step_name});

            const file_module = p_Build.createModule(.{
                .root_source_file = p_Build.path("Zig/ZigRegistry.zig"),
                .target = Target,
                .optimize = optimization_level,
                .link_libc = true,
                .link_libcpp = false, // is_windows is always true here
            });

            _add_macros_and_includes(file_module, p_Build);

            const compilation_object = p_Build.addObject(.{
                .name = step_name,
                .root_module = file_module,
            });

            compilation_object.root_module.addCSourceFile(.{
                .file = p_Build.path(source_file),
                .flags = flag_list.items,
            });

            linker_step.addFileArg(compilation_object.getEmittedBin());
        }

        linker_step.addFileArg(godot_cpp_lib);

        Lambda_IncludeFileHelper.AddIncludedFiles(p_Build, std.Build.Step.Run.addFileArg, linker_step, &Options.FileIncludes.?.ObjectFiles_ISPC);
        Lambda_IncludeFileHelper.AddIncludedFiles(p_Build, std.Build.Step.Run.addFileArg, linker_step, &Options.FileIncludes.?.StaticFiles_LibraryCache);

        linker_step.addArgs(&.{
            "kernel32.lib",
            "user32.lib",
            "msvcrt.lib",
            "vcruntime.lib",
            "ucrt.lib",
        });

        p_Build.getInstallStep().dependOn(&linker_step.step);
    }
}

pub fn build(p_Build: *std.Build) void {
    const allocator = p_Build.allocator;

    const _target = p_Build.standardTargetOptions(.{});
    var target = _target;

    var targets = std.ArrayList(*std.Build.Step.Compile).empty; // track all the compile commands
    defer targets.deinit(allocator);

    const optimize = p_Build.standardOptimizeOption(.{});

    const name: []const u8 = p_Build.option(
        []const u8,
        "LibraryName",
        "Name of the output GDExtension library.\n" ++
            "            (e.g., --DLibraryName='PCG_Environment'.)\n" ++
            "            Do 'ALL' if you want to compile everything.\n",
    ) orelse "";

    const input_compile_directory = p_Build.option(
        []const u8,
        "CompileFromDirectory",
        "Which directory to compile from.\n" ++
            "            (e.g., -DCompileFromDirectory='Procedural Environment Generator'.)\n" ++
            "            Do 'ALL' to compile everything\n",
    ) orelse "";

    const debug_build = p_Build.option(
        bool,
        "DebugBuild",
        "True or false to dictate whether this build should be treated as debug or not. Default is true.",
    ) orelse true;

    const windows_use_another_abi = p_Build.option(
        bool,
        "WindowsUseAnotherABI",
        "Whether to use another ABI for Windows than MSVC. e.g., GNU for Linux (-Dtarget=x86_64-windows-gnu). Default is false.",
    ) orelse false;

    const location_of_object_files_ispc = p_Build.option(
        []const u8,
        "LocationOfObjectFiles_ISPC",
        "Location of where the ISPC compiler emits its .obj files to. Default is 'cache/ispc.'",
    ) orelse "cache/ispc";

    switch (_target.result.os.tag) {
        .windows => {
            if (builtin.os.tag == .windows and !windows_use_another_abi) {
                std.debug.print("\x1b[33mHost operating-system is windows. Switching ABI to .msvc...\x1b[0m\n", .{});
                target = p_Build.resolveTargetQuery(.{
                    .cpu_arch = .x86_64,
                    .os_tag = .windows,
                    .abi = .msvc,
                });
            }
        },
        .macos => {
            target.result.os.version_range = .{ .semver = .{
                .min = .{ .major = 13, .minor = 0, .patch = 0 },
                .max = .{ .major = 13, .minor = 0, .patch = 0 },
            } };
        },
        else => {},
    }

    SkipDebugFlag = p_Build.option(
        bool,
        "SkipDebugFlag",
        "Whether to use a custom defined OptimizeMode flag or not. Does not affect C++ building (Godot binaries use -O2 for debug, -O3 for release). Default is false.",
    ) orelse false;

    const compile_from_directory = std.fmt.allocPrint(allocator, "{s}/", .{input_compile_directory}) catch
        {
            return;
        };
    const documentation_step = p_Build.step("docs", "Generate C++ (and other languages') code documentation maps using Doxygen.");

    const doxygen_run_command = p_Build.addSystemCommand(&.{ "doxygen", "Doxyfile" });
    const doxyfile_path = p_Build.path("Doxyfile");
    doxygen_run_command.addFileInput(doxyfile_path);

    documentation_step.dependOn(&doxygen_run_command.step);

    const compile_everything = std.mem.eql(u8, "ALL", compile_from_directory) or std.mem.eql(u8, "ALL", name);
    const compile_specific_files = name.len > 0 and compile_from_directory.len > 0;

    if (compile_everything or compile_specific_files) {
        var include_files = IncludeFiles{};

        var black_listed_characters = std.ArrayList([]const u8).empty;

        if (target.result.cpu.arch.isArm()) {
            black_listed_characters.appendSlice(allocator, &.{ "avx", "sse" }) catch @panic("OOM"); // try doesn't work on build scripts, so a man gotta do what a man gotta do
        } else if (target.result.cpu.arch.isX86()) {
            black_listed_characters.appendSlice(allocator, &.{"neon"}) catch @panic("OOM");
        }
        IncludeFiles.GlobFilesInDirectory(p_Build, &include_files.ObjectFiles_ISPC, .{
            .ExcludeFileWith = black_listed_characters,
            .DirectoryPath = location_of_object_files_ispc,
        });

        const dependencies_folder = "cache/Libraries/lib/";

        IncludeFiles.GlobFilesInDirectory(p_Build, &include_files.StaticFiles_LibraryCache, .{
            .DirectoryPath = dependencies_folder,
            .FileWithExtension = ".lib",
        });

        var compilation_options = CompilationOptions{
            .Name = name,
            .CompileFromDirectory = compile_from_directory,
            .DebugBuild = debug_build,
            .FileIncludes = include_files,
        };
        defer compilation_options.DeInitializeMembers(allocator); // destroys object_files too

        if (!compile_everything and compile_specific_files) {
            _compile(
                p_Build,
                target,
                optimize,
                allocator,
                &compilation_options,
            );
        } else if (compile_everything) {
            _read_meta_file_and_compile(
                p_Build,
                allocator,
                target,
                optimize,
                &compilation_options,
            );
        }
    } else {
        std.debug.print("\x1b[31mYou didn't tell the compiler to actually do anything! \x1b[30mYa sure you're doin good over there? Try:\x1b[0m \nzig build -DLibraryName='ALL' -DCompileFromDirectory='ALL'\n", .{});
    }
}
