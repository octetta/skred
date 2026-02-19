const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "skred",
        .target = target,
        .optimize = optimize,
    });

    // --- Versioning Logic ---
    const version_str = b.option([]const u8, "version", "Override version string") orelse version_blk: {
        const file_content = b.build_root.handle.readFileAlloc(b.allocator, "VERSION.txt", 128) catch |err| {
            std.debug.print("Error reading VERSION.txt: {s}\n", .{@errorName(err)});
            std.process.exit(1);
        };
        break :version_blk std.mem.trim(u8, file_content, " \n\r\t");
    };

    exe.root_module.addCMacro("SKRED_VERSION", b.fmt("\"{s}\"", .{version_str}));
    exe.root_module.addCMacro("_GNU_SOURCE", "1");

    const os_tag = target.result.os.tag;

    if (os_tag == .macos) {
        exe.root_module.addCMacro("_IS_OSX_", "1");
        exe.linkFramework("CoreAudio");
        exe.linkFramework("CoreFoundation");
        exe.stack_size = 0x800000;
    } else if (os_tag == .linux) {
        exe.linkSystemLibrary("asound");
    } else if (os_tag == .windows) {
        exe.root_module.addCMacro("_CRT_SECURE_NO_WARNINGS", "1");
        exe.root_module.addCMacro("WIN32_LEAN_AND_MEAN", "1");
        exe.linkSystemLibrary("ws2_32");
        exe.linkSystemLibrary("winmm");
    }

    var sources = std.ArrayList([]const u8).init(b.allocator);
    sources.appendSlice(&.{
        "src/skred.c", "src/miniwav.c", "src/amysamples.c",
        "src/retro/retro.c", "src/synth.c", "src/seq.c",
        "src/skode.c", "src/ands.c", "src/udp.c",
        "src/miniaudio.c", "src/skred-mem.c", "src/util.c",
        "src/skqueue.c",
        "src/synth-alloc.c",
    }) catch unreachable;

    if (os_tag != .windows) {
        sources.append("src/bestline.c") catch unreachable;
    } else {
        exe.root_module.addCMacro("NO_BESTLINE", "1");
    }

    exe.addCSourceFiles(.{
        .files = sources.items,
        .flags = &.{ "-Wall", "-Wno-multichar", "-fcommon", "-fno-sanitize=all", "-include", "src/portable_win.h" },
    });

    exe.addIncludePath(b.path("src"));
    exe.addIncludePath(b.path("src/retro"));
    exe.linkLibC();
    exe.linkSystemLibrary("m");

    if (os_tag != .windows) exe.linkSystemLibrary("pthread");
    if (os_tag == .linux) exe.linkSystemLibrary("rt");

    b.installArtifact(exe);

    // --- Bundle Step ---
    const bundle_step = b.step("bundle", "Create release folder");
    const install_bundle = b.addInstallArtifact(exe, .{
        .dest_dir = .{ .override = .{ .custom = "bundle" } },
    });

    const copy_readme = b.addInstallFile(b.path("README.md"), "bundle/README.md");
    const copy_license = b.addInstallFile(b.path("LICENSE.txt"), "bundle/LICENSE.txt");
    const copy_sk = b.addInstallDirectory(.{ .source_dir = b.path("sk"), .install_dir = .{ .custom = "bundle" }, .install_subdir = "sk" });
    const copy_wav = b.addInstallDirectory(.{ .source_dir = b.path("wav"), .install_dir = .{ .custom = "bundle" }, .install_subdir = "wav" });
    const copy_docs = b.addInstallDirectory(.{ .source_dir = b.path("docs"), .install_dir = .{ .custom = "bundle" }, .install_subdir = "docs" });

    bundle_step.dependOn(&install_bundle.step);
    bundle_step.dependOn(&copy_readme.step);
    bundle_step.dependOn(&copy_license.step);
    bundle_step.dependOn(&copy_sk.step);
    bundle_step.dependOn(&copy_wav.step);
    bundle_step.dependOn(&copy_docs.step);

    // --- Zip Step ---
    const zip_filename = b.fmt("skred-{s}-{s}.zip", .{ @tagName(os_tag), version_str });
    const zip_command = b.addSystemCommand(&.{ "zip", "-r", zip_filename, "bundle" });
    zip_command.setCwd(.{ .cwd_relative = b.getInstallPath(.{ .custom = "" }, "") });
    zip_command.step.dependOn(bundle_step);
    
    const zip_step = b.step("zip", "Create a final .zip archive");
    zip_step.dependOn(&zip_command.step);
}
