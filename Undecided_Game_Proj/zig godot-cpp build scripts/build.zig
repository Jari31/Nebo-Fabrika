// AI generated boilerplate. I would rewrite the SConstruct in Zig myself, but I don't have the time currently
// Though, it does contain some modification. Because AI is very confidently wrong most of the time.

const std = @import("std");

const Platform = struct {
    name: []const u8,
    os: std.Target.Os.Tag,
    abi: std.Target.Abi,
    ext: []const u8,
};

const TargetPlatforms = [_]Platform{
    .{ .name = "windows", .os = .windows, .abi = .msvc, .ext = "lib" },
    .{ .name = "linux", .os = .linux, .abi = .gnu, .ext = "a" },
    .{ .name = "macos", .os = .macos, .abi = .none, .ext = "a" },
};

const OptProfiles = [_]struct {
    name: []const u8,
    mode: std.builtin.OptimizeMode,
}{
    .{ .name = "debug", .mode = .Debug },
    .{ .name = "release", .mode = .ReleaseFast },
};

pub fn build(b: *std.Build) void {
    const io = b.graph.io;

    inline for (TargetPlatforms) |plat| {
        inline for (OptProfiles) |opt| {
            const target = b.resolveTargetQuery(.{
                .cpu_arch = .x86_64,
                .os_tag = plat.os,
                .abi = plat.abi,
            });

            const DebugBuild: bool = if (opt.mode == .Debug) true else false;

            const optimization_level = if (DebugBuild) "-O2" else "-O3";
            const godot_mod = b.createModule(.{
                .target = target,
                .optimize = opt.mode,
                .link_libc = true,
                .link_libcpp = true,
            });

            var flag_list = std.ArrayList([]const u8).empty;
            defer flag_list.deinit(b.allocator);

            //transliterated from godot-cpp py scripts
            godot_mod.addCMacro("GDEXTENSION", "1");
            godot_mod.addCMacro("THREADS_ENABLED", "1");
            godot_mod.addCMacro("TYPED_METHOD_BIND", "1");

            if (DebugBuild) {
                godot_mod.addCMacro("DEBUG_ENABLED", "1");
                godot_mod.addCMacro("HOT_RELOAD_ENABLED", "1");
            } else {
                godot_mod.addCMacro("NDEBUG", "1");
            }

            const is_os_target_android = (plat.os == .linux and plat.abi == .android);
            if (is_os_target_android) {
                godot_mod.addCMacro("ANDROID_ENABLED", "1");
                godot_mod.addCMacro("UNIX_ENABLED", "1");
                flag_list.append(b.allocator, "-fPIC") catch @panic("OOM");

                //if (Target.result.cpu.arch == .arm) {
                //    flag_list.append(Allocator, "-mfpu=neon") catch @panic("OOM");
                //} else if (Target.result.cpu.arch == .x86) {
                //    flag_list.append(Allocator, "-mstackrealign") catch @panic("OOM");
                //}
            } else {
                switch (plat.os) {
                    .windows => {
                        godot_mod.addCMacro("WINDOWS_ENABLED", "1");
                        godot_mod.addCMacro("NOMINMAX", "1");
                        godot_mod.addCMacro("V_ALIGNED", "1");
                        if (plat.abi == .msvc) {
                            godot_mod.addCMacro("_HAS_EXCEPTIONS", "0");
                            flag_list.append(b.allocator, "-fms-runtime-lib=static") catch @panic("OOM");
                        }
                    },
                    .linux => {
                        godot_mod.addCMacro("LINUX_ENABLED", "1");
                        godot_mod.addCMacro("UNIX_ENABLED", "1");
                        flag_list.append(b.allocator, "-fPIC") catch @panic("OOM");
                        //if (DebugBuild) {
                        // prevents the OS from pinning old hot-reload files in memory
                        //flag_list.append(b.allocator, "-fno-gnu-unique") catch @panic("OOM");
                        //}
                    },
                    .ios => {
                        godot_mod.addCMacro("IOS_ENABLED", "1");
                        godot_mod.addCMacro("UNIX_ENABLED", "1");
                        flag_list.append(b.allocator, "-miphoneos-version-min=12.0") catch @panic("OOM");
                        flag_list.append(b.allocator, "-stdlib=libc++") catch @panic("OOM");
                    },
                    .emscripten => {
                        godot_mod.addCMacro("WEB_ENABLED", "1");
                        godot_mod.addCMacro("UNIX_ENABLED", "1");
                        flag_list.appendSlice(b.allocator, &.{
                            "-sSIDE_MODULE=1",
                            "-sWASM_BIGINT",
                            "-sSUPPORT_LONGJMP=wasm",
                            "-sUSE_PTHREADS=1",
                        }) catch @panic("OOM");
                    },
                    else => {},
                }
            }

            flag_list.appendSlice(b.allocator, &.{
                "-std=c++17",
                "-fno-exceptions",
            }) catch @panic("OOM");

            flag_list.append(b.allocator, optimization_level) catch @panic("OOM");
            if (DebugBuild) flag_list.append(b.allocator, "-g") catch @panic("OOM");

            const cpp_flags = flag_list.items;

            godot_mod.addIncludePath(b.path("include"));
            godot_mod.addIncludePath(b.path("gen/include"));
            godot_mod.addIncludePath(b.path("gen/core/include"));
            godot_mod.addIncludePath(b.path("gen/gdextension/include"));
            godot_mod.addIncludePath(b.path("gdextension"));

            // Capture by value, then create a local 'var' to allow state mutations
            if (b.build_root.handle.openDir(io, "src", .{ .iterate = true })) |*src_dir| {
                defer src_dir.close(io);

                if (src_dir.walk(b.allocator)) |captured_walker| {
                    var src_walker = captured_walker;
                    defer src_walker.deinit();

                    while (src_walker.next(io) catch null) |entry| {
                        if (entry.kind == .file and std.mem.endsWith(u8, entry.path, ".cpp")) {
                            const full_path = b.fmt("src/{s}", .{entry.path});
                            godot_mod.addCSourceFile(.{
                                .file = b.path(full_path),
                                .flags = cpp_flags,
                            });
                        }
                    }
                } else |_| {}
            } else |err| {
                std.debug.print("Warning: Could not open 'src' directory for {s}-{s}: {}\n", .{ plat.name, opt.name, err });
            }

            if (b.build_root.handle.openDir(io, "gen/src", .{ .iterate = true })) |*gen_dir| {
                defer gen_dir.close(io);

                if (gen_dir.walk(b.allocator)) |captured_gen_walker| {
                    var gen_walker = captured_gen_walker;
                    defer gen_walker.deinit();

                    while (gen_walker.next(io) catch null) |entry| {
                        if (entry.kind == .file and std.mem.endsWith(u8, entry.path, ".cpp")) {
                            const full_path = b.fmt("gen/src/{s}", .{entry.path});
                            godot_mod.addCSourceFile(.{
                                .file = b.path(full_path),
                                .flags = cpp_flags,
                            });
                        }
                    }
                } else |_| {}
            } else |err| {
                std.debug.print("Warning: Could not open 'gen/src' directory for {s}-{s}: {}\n", .{ plat.name, opt.name, err });
            }

            const lib_name = b.fmt("godot-cpp.{s}.{s}.x86_64", .{ plat.name, opt.name });

            const godot_lib = b.addLibrary(.{
                .name = lib_name,
                .linkage = .static,
                .root_module = godot_mod,
            });

            if (plat.os == .linux) godot_lib.root_module.addRPath(.{ .cwd_relative = "." });

            const install_step = b.addInstallArtifact(godot_lib, .{
                .dest_dir = .{ .override = .{ .custom = "bin" } },
            });

            b.getInstallStep().dependOn(&install_step.step);
        }
    }
}
