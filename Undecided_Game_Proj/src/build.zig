const std = @import("std");

var FileExtension: []const u8 = ".cpp";

fn _print_unsupported_build(Tag: anytype) void {
    std.debug.print("Unsupported build platform target: {}\n", .{Tag});
}

fn _compile(
    Name: []const u8,
    p_Build: *std.Build,
    CompileFromDirectory: []const u8,
    CompileToDirectory: []const u8,
    Target: std.Build.ResolvedTarget,
    Optimize: std.builtin.OptimizeMode,
    Allocator: std.mem.Allocator,
    DebugBuild: bool,
) void {
    const os_tag = Target.result.os.tag;

    const base_module = p_Build.createModule(.{
        .root_source_file = p_Build.path("Zig/ZigRegistry.zig"),
        .target = Target,
        .optimize = Optimize,
        .link_libc = true,
        .link_libcpp = true,
    });

    const godot_cpp_lib = switch (DebugBuild) {
        true => switch (os_tag) {
            .windows => p_Build.path("../godot-cpp/bin/libgodot-cpp.windows.template_debug.x86_64.lib"),
            .linux => p_Build.path("../godot-cpp/bin/libgodot-cpp.linux.template_debug.x86_64.lib"),

            else => {
                _print_unsupported_build(os_tag);
                return;
            },
        },
        false => switch (os_tag) {
            .windows => p_Build.path("../godot-cpp/bin/libgodot-cpp.windows.template_release.x86_64.lib"),
            .linux => p_Build.path("../godot-cpp/bin/libgodot-cpp.linux.template_release.x86_64.lib"),

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
    const registration_path = std.fmt.allocPrint(
        Allocator,
        "{s}Register_{s}{s}",
        .{ CompileFromDirectory, Name, FileExtension },
    ) catch return;

    //library.
    base_module.addCSourceFiles(.{
        .files = &.{
            file_path,
            registration_path,
        },
        .flags = &.{
            "-std=c++17",
            "-O3",
        },
    });

    base_module.addIncludePath(p_Build.path("../godot-cpp/include"));
    base_module.addIncludePath(p_Build.path("../godot-cpp/gen/include"));
    base_module.addIncludePath(p_Build.path("../godot-cpp/gdextension"));

    const library = p_Build.addLibrary(.{
        .name = Name,
        .linkage = .dynamic,
        .root_module = base_module,
    });

    const artifacts = p_Build.addInstallArtifact(
        library,
        .{ .dest_dir = .{ .override = .{ .custom = CompileToDirectory } } },
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
            "            Leave it empty if you want to compile everything.\n",
    ) orelse "";

    const input_compile_directory = p_Build.option(
        []const u8,
        "CompileFromDirectory",
        "Which directory to compile from.\n" ++
            "            (e.g., -DCompileFromDirectory='Procedural Environment Generator'.)\n" ++
            "            Or leave it empty to compile everything\n",
    ) orelse "";

    const debug_build = p_Build.option(
        bool,
        "DebugBuild",
        "True or false to dictate whether this build should be treated as debug or not. Default is true.",
    ) orelse false;

    const compile_from_directory = std.fmt.allocPrint(allocator, "{s}\\", .{input_compile_directory}) catch
        {
            return;
        };

    const directory_base = "../GDProject/bin/build/";

    const compile_to_directory: []const u8 = std.mem.concat(allocator, u8, &.{ directory_base, name }) catch |err|
        {
            std.debug.print("String concat failed with error: {}\n", .{err});

            return;
        };

    _compile(name, p_Build, compile_from_directory, compile_to_directory, target, optimize, allocator, debug_build);
}
