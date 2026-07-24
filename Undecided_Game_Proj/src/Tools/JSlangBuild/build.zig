const std = @import("std");

pub fn build(Build: *std.Build) void {
    const target = Build.standardTargetOptions(.{});
    const optimize = Build.standardOptimizeOption(.{});

    const executable_module = Build.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .link_libcpp = true,
    });

    executable_module.addIncludePath(Build.path("../../cache/")); // IMPORTANT: remove this if this tool is made public
    executable_module.addIncludePath(Build.path("cache/"));

    executable_module.addCSourceFile(.{ .file = Build.path("main.cpp"), .flags = &.{
        "-std=c++23",
        "-Wall",
        "-Wextra",
        "-fno-exceptions",
    } });

    const executable = Build.addExecutable(.{
        .name = "jslangbuild",
        .root_module = executable_module,
    });

    const install_step = Build.addInstallArtifact(executable, .{
        .dest_dir = .{ .override = .{ .custom = "../" } },
    });

    Build.getInstallStep().dependOn(&install_step.step);

    const run_command = Build.addRunArtifact(executable);
    run_command.step.dependOn(&install_step.step);

    if (Build.args) |Arguments| {
        run_command.addArgs(Arguments);
    }

    const run_step = Build.step("run", "Run the application.");
    run_step.dependOn(&run_command.step);
}
