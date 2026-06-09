// consult README for setup guide on compiling godot-cpp (scons doesn't work since it uses MSVC instead of Zig's own LLVM compiler). the cloned repo should cover the project structure
const std = @import("std");

var FileExtension: []const u8 = ".cpp";

fn _print_unsupported_build(Tag: anytype) void {
    std.debug.print("Unsupported build platform target: {}\n", .{Tag});
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

    p_Build.install_path = p_Build.path("../GDProject/bin/build/").getPath(p_Build);

    const base_module = p_Build.createModule(.{
        .root_source_file = p_Build.path("Zig/ZigRegistry.zig"),
        .target = Target,
        .optimize = optimization_level,
        .link_libc = true,
        .link_libcpp = true,
    });

    const godot_cpp_lib = switch (DebugBuild) {
        true => switch (os_tag) {
            .windows => p_Build.path("../godot-cpp/zig-out/bin/godot-cpp.windows.debug.x86_64.lib"),
            .linux => p_Build.path("../godot-cpp/zig-out/bin/libgodot-cpp.linux.debug.x86_64.a"),

            else => {
                _print_unsupported_build(os_tag);
                return;
            },
        },
        false => switch (os_tag) {
            .windows => p_Build.path("../godot-cpp/zig-out/bin/godot-cpp.windows.release.x86_64.lib"),
            .linux => p_Build.path("../godot-cpp/zig-out/bin/libgodot-cpp.linux.release.x86_64.a"),

            else => {
                _print_unsupported_build(os_tag);
                return;
            },
        },
    };

    base_module.addObjectFile(godot_cpp_lib);

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
    base_module.addCMacro("GDEXTENSION", "1");
    base_module.addCMacro("TYPED_METHOD_BIND", "1");

    if (!DebugBuild) {
        base_module.addCMacro("PRODUCTION_BUILD", "");
        base_module.addCMacro("NDEBUG", "1");
    } else {
        base_module.addCMacro("DEBUG_ENABLED", "1");
        base_module.addCMacro("HOT_RELOAD_ENABLED", "1");
    }

    switch (os_tag) {
        .windows => {
            flag_list.append(Allocator, "-DNOMINMAX") catch @panic("OOM");
        },
        .linux => {
            flag_list.append(Allocator, "-fPIC") catch @panic("OOM");

            if (DebugBuild) flag_list.append(Allocator, "-fno-gnu-unique") catch @panic("OOM");
        },
        else => {},
    }

    const cpp_optimization_level = if (DebugBuild) "-O2" else "-O3";
    flag_list.appendSlice(Allocator, &.{
        "-std=c++17",
        cpp_optimization_level,
        "-Wall",
        "-Werror",
        "-Wextra",
        "-Wno-gnu-anonymous-struct",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-DTYPED_METHOD_BIND",
        "-fno-exceptions",
    }) catch @panic("OOM");

    base_module.addCSourceFiles(.{
        .files = &.{
            file_path,
            registration_path,
        },
        .flags = flag_list.items,
    });

    base_module.addIncludePath(p_Build.path("../godot-cpp/include"));
    base_module.addIncludePath(p_Build.path("../godot-cpp/include"));
    base_module.addIncludePath(p_Build.path("../godot-cpp/gen/include"));
    base_module.addIncludePath(p_Build.path("../godot-cpp/gen/core/include"));
    base_module.addIncludePath(p_Build.path("../godot-cpp/gdextension"));
    base_module.addIncludePath(p_Build.path("../godot-cpp/gen/gdextension/include"));

    const library = p_Build.addLibrary(.{
        .name = Name,
        .linkage = .dynamic,
        .root_module = base_module,
    });

    const artifacts = p_Build.addInstallArtifact(
        library,
        .{ .dest_dir = .{ .override = .{ .custom = Name } } },
    );

    p_Build.getInstallStep().dependOn(&artifacts.step);
}

pub fn build(p_Build: *std.Build) void {
    const allocator = p_Build.allocator;

    const target = p_Build.standardTargetOptions(.{});
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
