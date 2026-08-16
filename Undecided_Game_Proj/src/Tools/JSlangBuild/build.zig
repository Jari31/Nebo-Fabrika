const std = @import("std");
const builtin = @import("builtin");

const PROGRAM_NAME = "jslang";

/// IMPORTANT: *the external cache location for lib and win sdk files. change this to internal cache when made into a standalone repo
const EXTERNAL_CACHE_FOLDER_LOCATION = "../../cache/";
const CACHE_FOLDER_LOCATION = "cache";

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
    var target_query = std.Target.Query.parse(.{}) catch unreachable;
    const optimize = Build.standardOptimizeOption(.{});

    if ((target_query.os_tag == .windows or (target_query.os_tag == null and builtin.os.tag == .windows)) and target_query.abi == null) {
        target_query.abi = .msvc;
    }

    const target = Build.standardTargetOptions(.{ .default_target = target_query });

    const is_msvc_abi = target.result.os.tag == .windows;

    const executable_module = Build.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = !is_msvc_abi,
        .link_libcpp = !is_msvc_abi,
    });

    executable_module.addIncludePath(Build.path("../../cache/")); // IMPORTANT: remove this if this tool is made public
    executable_module.addIncludePath(Build.path("cache/"));

    var library_file_paths = std.ArrayList(std.Build.LazyPath).empty;
    defer library_file_paths.deinit(Build.allocator);

    // GlobFilesWithExtension(Build, Build.path("../../cache/Libraries/lib/"), ".lib", &library_file_paths);

    GlobFilesWithExtension(Build, Build.path("cache/Libraries/lib/"), if (target.result.os.tag != .windows) ".a" else ".lib", &library_file_paths);
    GlobFilesWithExtension(Build, Build.path("../../cache/Libraries/lib/"), if (target.result.os.tag != .windows) ".a" else ".lib", &library_file_paths);

    var compilation_flags = std.ArrayList([]const u8).empty;
    defer compilation_flags.deinit(Build.allocator);

    compilation_flags.appendSlice(Build.allocator, &.{
        "-std=c++23",
        "-Wall",
        "-Wextra",

        // #embed
        "-Wno-c23-extensions",
    }) catch {};

    if (is_msvc_abi) {
        compilation_flags.appendSlice(Build.allocator, &.{
            "-fms-runtime-lib=dll",
        }) catch @panic("OOM");
    }

    executable_module.addCSourceFile(.{
        .file = Build.path("main.cpp"),
        .flags = compilation_flags.items,
    });
    {
        const os_tag = target.result.os.tag;

        const library_directory_location = Build.pathJoin(&.{ CACHE_FOLDER_LOCATION, "Libraries/lib/" });
        const dynamic_library_extension = if (os_tag == .windows) ".dll" else if (os_tag == .macos) ".dylib" else ".so";

        executable_module.addCMacro(
            "PATH_TO_SLANG_COMPILER_DYNAMIC_LIBRARY",
            Build.fmt("\"{s}slang-compiler{s}\"", .{ library_directory_location, dynamic_library_extension }),
        );
        executable_module.addCMacro(
            "PATH_TO_SLANG_GLSL_MODULE_DYNAMIC_LIBRARY",
            Build.fmt("\"{s}slang-glsl-module{s}\"", .{ library_directory_location, dynamic_library_extension }),
        );
        executable_module.addCMacro(
            "PATH_TO_SLANG_GL_SLANG_DYNAMIC_LIBRARY",
            Build.fmt("\"{s}slang-glslang{s}\"", .{ library_directory_location, dynamic_library_extension }),
        );
        executable_module.addCMacro(
            "PATH_TO_SLANG_LLVM_DYNAMIC_LIBRARY",
            Build.fmt("\"{s}slang-llvm{s}\"", .{ library_directory_location, dynamic_library_extension }),
        );
        executable_module.addCMacro(
            "PATH_TO_SLANG_RUN_TIME_DYNAMIC_LIBRARY",
            Build.fmt("\"{s}slang-rt{s}\"", .{ library_directory_location, dynamic_library_extension }),
        );
    }

    const exclude_library_files_with_name = [_][]const u8{};

    for (library_file_paths.items) |Path| {
        var contains_black_listed_name = false;

        inline for (exclude_library_files_with_name) |Name| {
            if (std.mem.containsAtLeast(u8, Path.basename(Build, null), 1, Name)) {
                contains_black_listed_name = true;
            }
        }
        if (contains_black_listed_name) {
            // std.debug.print("Excluded {s}.\n", .{Path.basename(Build, null)});
            continue;
        }

        executable_module.addObjectFile(Path);
    }

    const executable = Build.addExecutable(.{
        .name = PROGRAM_NAME,
        .root_module = executable_module,
    });

    if (is_msvc_abi) {
        const msvc_header_includes = [_][]const u8{
            ".xwin/crt/include",
            ".xwin/sdk/include/ucrt",
            ".xwin/sdk/include/um",
            ".xwin/sdk/include/shared",
        };

        const target_cpu_arch: []const u8 = @tagName(target.result.cpu.arch);

        const msvc_library_includes = [_][]const u8{
            Build.fmt("{s}{s}", .{ ".xwin/crt/lib/", target_cpu_arch }),
            Build.fmt("{s}{s}", .{ ".xwin/sdk/lib/ucrt/", target_cpu_arch }),
            Build.fmt("{s}{s}", .{ ".xwin/sdk/lib/um/", target_cpu_arch }),
        };

        inline for (msvc_header_includes) |include_header| {
            executable.root_module.addSystemIncludePath(Build.path(Build.pathJoin(&.{ EXTERNAL_CACHE_FOLDER_LOCATION, include_header })));
        }

        inline for (msvc_library_includes) |include_library| {
            executable.root_module.addLibraryPath(Build.path(Build.pathJoin(&.{ EXTERNAL_CACHE_FOLDER_LOCATION, include_library })));
        }

        executable.root_module.linkSystemLibrary("msvcrt", .{});
        executable.root_module.linkSystemLibrary("vcruntime", .{});
        executable.root_module.linkSystemLibrary("ucrt", .{});
        executable.root_module.linkSystemLibrary("msvcprt", .{});
        executable.root_module.linkSystemLibrary("kernel32", .{});
        executable.root_module.linkSystemLibrary("user32", .{});
        executable.root_module.linkSystemLibrary("ntdll", .{});
        executable.root_module.linkSystemLibrary("dbghelp", .{});

        executable.linker_allow_undefined_version = true;
        executable.subsystem = .Console;
        executable.entry = .{ .symbol_name = "mainCRTStartup" };
    }

    const install_step = Build.addInstallArtifact(executable, .{
        .dest_dir = .{ .override = .{ .custom = "../" } },
    });

    Build.getInstallStep().dependOn(&install_step.step);
    return;
}
