// This script assumes that the project structure is as the same one in the repo
// Why Zig and not SCons or CMake? Zig is faster. Both at caching and compilation.
// And also because SCons needs WSL + a second compiler + VM for MSVC bindings
// Zig compiles Zig, C and C++, for basically all platforms (WASI is important for NF's modding API)

const std = @import("std");

var FileExtension: []const u8 = ".cpp"; // mostly redundant because changing this WILL break everything
var BuildFolderPath = "../GDProject/bin/build/";

fn _print_unsupported_build(Tag: anytype) void {
    std.debug.print("Unsupported build platform target: {}\n", .{Tag});
}

fn _add_macros_and_includes(Module: *std.Build.Module, p_Build: *std.Build) void {
    Module.addIncludePath(p_Build.path("../godot-cpp/include"));
    Module.addIncludePath(p_Build.path("../godot-cpp/gen/include"));
    Module.addIncludePath(p_Build.path("../godot-cpp/gen/core/include"));
    Module.addIncludePath(p_Build.path("../godot-cpp/gdextension"));
    Module.addIncludePath(p_Build.path("../godot-cpp/gen/gdextension/include"));

    Module.addCMacro("TYPED_METHOD_BIND", "1");
}

fn _read_meta_file_and_compile(
    p_Build: *std.Build,
    Allocator: std.mem.Allocator,
    DebugBuild: bool,
    Target: std.Build.ResolvedTarget,
    Optimize: std.builtin.OptimizeMode,
) void {
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
    std.debug.print("Loaded meta file onto memory. Proceeding to read and compile...\n", .{});
    while (lines.next()) |line| {
        const clean_line = std.mem.trim(u8, line, "\r");

        if (clean_line.len == 0) continue;

        std.debug.print("Found line: {s}\n", .{clean_line});

        var parts = std.mem.splitSequence(u8, clean_line, ":");

        if (parts.next()) |key| {
            if (parts.next()) |value| {
                const raw_folder = std.mem.trim(u8, key, " \t");
                const folder = std.fmt.allocPrint(Allocator, "{s}/", .{raw_folder}) catch |err|
                    // moves it to the heap since otherwise it gets corrupted into:
                    // error: unable to update file from '.zig-cache\o\004797be9ffb1d8f419dced72c944d2c\libShaderComp.so' to 'F:\Openworld_Game\Undecided_Game_Proj\GDProject\bin\build\¬¬¬¬¬¬¬¬¬¬\libShaderComp.so': BadPathName
                    {
                        std.debug.print("Failed to format 'folder' for compilation whilst trying to read meta file with error: {}", .{err});
                        return;
                    };

                const raw_file = std.mem.trim(u8, value, " \t");
                const file = Allocator.dupe(u8, raw_file) catch |err| {
                    std.debug.print("Failed to allocate space on memory for 'file' with error: {}", .{err});
                    return;
                };

                std.debug.print("Found folder: {s}\n Found file: {s}\n", .{ folder, file });

                _compile(file, p_Build, folder, Target, Optimize, Allocator, DebugBuild);
            }
        }
    }
}

fn _compile(
    Name: []const u8,
    p_Build: *std.Build,
    CompileFromDirectory: []const u8,
    Target: std.Build.ResolvedTarget,
    Optimize: std.builtin.OptimizeMode,
    Allocator: std.mem.Allocator,
    DebugBuild: bool,
) void {
    const os_tag = Target.result.os.tag;

    var optimization_level = Optimize;

    if (Optimize == std.builtin.OptimizeMode.Debug and !DebugBuild)
        optimization_level = std.builtin.OptimizeMode.ReleaseFast;

    p_Build.install_path = p_Build.path(BuildFolderPath).getPath(p_Build);

    const is_windows = if (Target.result.os.tag == .windows) true else false;

    const base_module = p_Build.createModule(.{
        .root_source_file = p_Build.path("Zig/ZigRegistry.zig"),
        .target = Target,
        .optimize = optimization_level,
        .link_libc = true,
        .link_libcpp = !is_windows,
    });

    const godot_cpp_lib = switch (DebugBuild) { // Can compile to Android, and such, too, but that's out of this project's scope currently
        true => switch (os_tag) {
            .windows => p_Build.path("../godot-cpp/bin/libgodot-cpp.windows.template_debug.x86_64.lib"),
            .linux => p_Build.path("../godot-cpp/bin/libgodot-cpp.linux.template_debug.x86_64.a"),
            .macos => p_Build.path("../godot-cpp/bin/libgodot-cpp.macos.template_debug.x86_64.a"),

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

    const file_path = std.fmt.allocPrint(
        Allocator,
        "{s}{s}{s}",
        .{ CompileFromDirectory, Name, FileExtension },
    ) catch return;
    std.debug.print("File path: {s}\n", .{file_path});
    const registration_path = std.fmt.allocPrint(
        Allocator,
        "{s}Register_{s}{s}",
        .{ CompileFromDirectory, Name, FileExtension },
    ) catch return;

    var flag_list = std.ArrayList([]const u8).empty;
    defer flag_list.deinit(Allocator);

    //transliterated from godot-cpp py scripts
    //base_module.addCMacro("GDEXTENSION", "1");

    if (!DebugBuild) {
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
                    //"-fms-runtime-lib=dll",
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
            //flag_list.append(Allocator, "-fPIC") catch @panic("OOM");
        },
        else => {},
    }

    const cpp_optimization_level = if (DebugBuild) "-O2" else "-O3";
    flag_list.appendSlice(Allocator, &.{
        "-std=c++17",
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
    }) catch @panic("OOM");

    if (os_tag != .windows) {
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
            .name = Name,
            .linkage = .dynamic,
            .root_module = base_module,
        });

        //if (os_tag == .linux) {
        //    library.root_module.addRPath(.{ .cwd_relative = "." });
        //}

        const artifacts = p_Build.addInstallArtifact(
            library,
            .{ .dest_dir = .{ .override = .{ .custom = Name } } },
        );

        p_Build.getInstallStep().dependOn(&artifacts.step);
    } else {
        // If you don't use MSVC's linker.exe, it segfault Godot because Godot expects MSVC ABIs and not LLVM
        // You could avoid this and use the steps above if you compiled Godot with MinGW instead of MSVC (MSVC genuinely is so ass)
        // I recommend doing that if you are on Linux and don't want to setup a VM or a WINE environment
        // Also, you need to run this in a 'x64 Native Tools Command Prompt for VS 2022'
        // Mostly because Zig currently doesn't have an LTS WindowsSDK API and I didn't want any external dependencies (Aside from MSVC,-
        //  -but again, you can just compile Godot with MinGW or Zig)
        const files_to_compile = &[_][]const u8{
            file_path,
            registration_path,
        };

        const folder_path = std.fmt.allocPrint(
            Allocator,
            "{s}{s}",
            .{ BuildFolderPath, Name },
        ) catch @panic("OOM");

        const io = p_Build.graph.io;

        std.Io.Dir.createDirPath(std.Io.Dir.cwd(), io, folder_path) catch |err| {
            std.debug.print("Failed to create directory {s}: {}\n", .{ folder_path, err });
            @panic("Directory creation failed");
        };

        std.debug.print("Creating a subfolder at: {s}", .{folder_path});

        // const output_comp_object = compilation_object.getEmittedBin();

        const output_location = std.fmt.allocPrint(
            Allocator,
            "/OUT:{s}{s}/{s}.dll",
            .{ BuildFolderPath, Name, Name },
        ) catch @panic("OOM");
        std.debug.print("Linker outputting to: {s}", .{output_location});

        const linker_step = p_Build.addSystemCommand(&.{"link.exe"});

        linker_step.addArgs(&.{
            "/DLL",
            output_location,
            "/NOLOGO",
            "/NODEFAULTLIB:libcmt.lib",
        });

        inline for (files_to_compile, 0..) |source_file, i| {
            const step_name = std.fmt.allocPrint(Allocator, "{s}_{d}", .{ Name, i }) catch @panic("OOM");
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

        linker_step.addArgs(&.{
            "kernel32.lib",
            "user32.lib",
            "msvcrt.lib",
            "vcruntime.lib",
            "ucrt.lib",
        });

        // if (p_Build.graph.environ_map.get("LIB")) |lib_env| {
        //     var it = std.mem.splitScalar(u8, lib_env, ';');
        //     while (it.next()) |path| {
        //         if (path.len == 0) continue;
        //
        //         const lib_path_flag = std.fmt.allocPrint(Allocator, "/LIBPATH:{s}", .{path}) catch @panic("OOM");
        //         linker_step.addArg(lib_path_flag);
        //     }
        // } else {
        //     std.debug.print(
        //         "\n\x1b[31m[BUILD ERROR] MSVC Linker paths not found.\x1b[0m" ++
        //             "\\Please run 'zig build' from inside the 'x64 Native Tools Command Prompt for VS 2022'" ++
        //             "\\so that the required Windows SDK paths can be passed to link.exe." ++ "\n\n",
        //         .{},
        //     );
        // }

        p_Build.getInstallStep().dependOn(&linker_step.step);
    }
}

pub fn build(p_Build: *std.Build) void {
    const allocator = p_Build.allocator;

    const _target = p_Build.standardTargetOptions(.{});
    var target = _target;

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

    if (_target.result.os.tag == .windows and !windows_use_another_abi) {
        std.debug.print("Target platform is windows, switching ABI to .msvc...\n", .{});
        target = p_Build.resolveTargetQuery(.{
            .cpu_arch = .x86_64,
            .os_tag = .windows,
            .abi = .msvc,
        });
    }

    const compile_from_directory = std.fmt.allocPrint(allocator, "{s}/", .{input_compile_directory}) catch
        {
            return;
        };

    //const directory_base = "../GDProject/bin/build/";

    //const compile_to_directory: []const u8 = std.fmt.allocPrint(
    //    allocator,
    //    "{s}{s}",
    //    .{ directory_base, name },
    //) catch return;

    if (!std.mem.eql(u8, "ALL", compile_from_directory) and !std.mem.eql(u8, "ALL", name) and compile_from_directory.len > 0 and name.len > 0) {
        _compile(
            name,
            p_Build,
            compile_from_directory,
            target,
            optimize,
            allocator,
            debug_build,
        );
    } else if (std.mem.eql(u8, "ALL", compile_from_directory) or std.mem.eql(u8, "ALL", name)) {
        _read_meta_file_and_compile(p_Build, allocator, debug_build, target, optimize);
    }
}
