const std = @import("std");
const builtin = @import("builtin");

const PROGRAM_NAME = "jslang";

inline fn GlobFilesWithExtension(Build: *std.Build, FolderPath: std.Build.LazyPath, FileExtension: []const u8, ListToAppendTo: *std.ArrayList(std.Build.LazyPath)) void {
    const io = Build.graph.io;
    var directory = Build.build_root.handle.openDir(io, FolderPath.src_path.sub_path, .{ .iterate = true }) catch @panic("OOM");
    defer directory.close(io);

    var directory_walker = directory.walk(Build.allocator) catch @panic("OOM");
    defer directory_walker.deinit();

    while (directory_walker.next(io) catch @panic("OOM")) |entry| {
        if (entry.kind == .file and std.mem.endsWith(u8, entry.path, FileExtension)) {
            const entry_path = Build.allocator.dupe(u8, entry.path) catch @panic("OOM");
            const entry_relative_path = FolderPath.path(Build, entry_path);
            ListToAppendTo.append(Build.allocator, entry_relative_path) catch @panic("OOM");
        }
    }
}

pub fn build(Build: *std.Build) void {
    const target = Build.standardTargetOptions(.{});
    const optimize = Build.standardOptimizeOption(.{});

    // const is_windows = target.result.os.tag == .windows;

    const executable_module = Build.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .link_libcpp = true,
    });

    executable_module.addIncludePath(Build.path("../../cache/")); // IMPORTANT: remove this if this tool is made public
    executable_module.addIncludePath(Build.path("cache/"));

    var library_file_paths = std.ArrayList(std.Build.LazyPath).empty;
    defer library_file_paths.deinit(Build.allocator);

    // GlobFilesWithExtension(Build, Build.path("../../cache/Libraries/lib/"), ".lib", &library_file_paths);

    GlobFilesWithExtension(Build, Build.path("cache/Libraries/lib/"), if (target.result.os.tag != .windows) ".a" else ".lib", &library_file_paths);

    executable_module.addCSourceFile(.{
        .file = Build.path("main.cpp"),
        .flags = &.{
            "-std=c++23",
            "-Wall",
            "-Wextra",
            // "-fms-runtime-lib=static",
        },
    });

    // if (!is_windows) {
    for (library_file_paths.items) |Path| {
        executable_module.addObjectFile(Path);
    }

    const executable = Build.addExecutable(.{
        .name = PROGRAM_NAME,
        .root_module = executable_module,
    });

    const install_step = Build.addInstallArtifact(executable, .{
        .dest_dir = .{ .override = .{ .custom = "../" } },
    });

    Build.getInstallStep().dependOn(&install_step.step);
    return;
    // }

    // const compilation_object = Build.addObject(.{
    //     .name = PROGRAM_NAME,
    //     .root_module = executable_module,
    // });

    // const linker_step = Build.addSystemCommand(&.{
    //     "xrepo",
    //     "env",
    // });

    // if (builtin.os.tag == .linux) {
    //     linker_step.addArgs(&.{ "-p", "msvc-wine" });
    // }

    // linker_step.addArgs(&.{
    //     "link.exe",
    //     "/NOLOGO",
    //     "/OUT:" ++ PROGRAM_NAME ++ ".exe",
    //     "kernel32.lib",
    //     "user32.lib",
    //     "msvcrt.lib",
    //     "vcruntime.lib",
    //     "ucrt.lib",
    // });

    // linker_step.addFileArg(compilation_object.getEmittedBin());

    // for (library_file_paths.items) |Path| {
    //     linker_step.addFileArg(Path);
    // }

    // Build.getInstallStep().dependOn(&linker_step.step);
}
