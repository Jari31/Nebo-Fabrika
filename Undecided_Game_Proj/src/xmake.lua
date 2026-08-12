add_requires("ispc 1.31.0", { verify = false })
add_requires("zig v0.16", { verify = false })
add_requires("xwin 0.9.0", { verify = false })
add_requires("scons")
add_requires("enkits", "angelscript", "cpuinfo", "tracy")

set_runtimes("MD")

local target_platform = get_config("plat")
if not target_platform then
    target_platform = "linux"
end

local target_arch = "x86_64"
if is_arch("arm64") then
    target_arch = "aarch64"
elseif is_arch("wasm.*") then
    target_arch = "wasm"
end

local GlobalCache = path.join(os.projectdir() or "", "cache")
--set_config("packagedir", path.join(GlobalCache, "cache/Libraries"))
local BuildFolder = "../GDProject/bin/build/"

package("slang") -- because building from source for C++ is a nightmare
    set_homepage("https://github.com/shader-slang/slang")
    set_description("Slang is a shading language and compiler framework.")

    -- local target_platform = get_config("plat")

    -- local target_arch = "x86_64"
    -- if is_arch("arm64") then
    --     target_arch = "aarch64"
    -- elseif is_arch("wasm.*") then
    --     target_arch = "wasm"
    -- end
    -- for reference:
    -- https://github.com/shader-slang/slang/releases/download/v2026.14.1/slang-2026.14.1-windows-x86_64.tar.gz
    set_urls("https://github.com/shader-slang/slang/releases/download/v$(version)/slang-$(version)-" .. target_platform .. "-" .. target_arch .. ".tar.gz")

    on_install( function (package)
        if package:is_plat("windows") then
            os.cp("bin/slangc.exe", package:installdir("bin"))
            os.cp("bin/*.dll", package:installdir("bin"))
            os.cp("lib/*.lib", package:installdir("lib"))
        else
            os.cp("bin/slangc", package:installdir("bin"))
            os.cp("lib/*.so", package:installdir("lib"))
            os.cp("lib/*.dylib", package:installdir("lib"))
        end

        os.cp("include/*", package:installdir("include"))
    end)

    on_test( function (package)
        os.vrun("slangc -v")
    end)

package("xwin")
    set_kind("binary")

    -- for reference:
    -- https://github.com/Jake-Shadle/xwin/releases/download/0.9.0/xwin-0.9.0-x64-pc-windows-msvc.tar.gz
    -- https://github.com/Jake-Shadle/xwin/releases/download/0.9.0/xwin-0.9.0-x86_64-pc-windows-msvc.tar.gz

    if is_host("windows") then
        set_urls("https://github.com/Jake-Shadle/xwin/releases/download/$(version)/xwin-$(version)-" .. target_arch .. "-pc-windows-msvc.tar.gz")
    else
        set_urls("https://github.com/Jake-Shadle/xwin/releases/download/$(version)/xwin-$(version)-" .. target_arch .. "-unknown-$(host)-musl.tar.gz")
    end

    on_install( function (package)
        if package:is_plat("windows") then
            os.cp("xwin.exe", package:installdir("bin"))
        else
            os.cp("xwin", package:installdir("bin"))
        end
    end)

package("zig")
    set_kind("binary")
    set_homepage("https://ziglang.org/")
    set_description("Zig programming language compiler and toolchain")

    if is_host("windows") then
        set_urls("https://ziglang.org/download/$(version)/zig-$(arch)-windows-$(version).zip")
    else
        set_urls("https://ziglang.org/download/$(version)/zig-$(arch)-$(host)-$(version).tar.xz")
    end

    on_install( function (package)
        if package:is_plat("windows") then
            os.cp("zig.exe", package:installdir("bin"))
        else
            os.cp("zig", package:installdir("bin"))
        end
        os.cp("lib", package:installdir("bin"))
    end)

rule("ispc_rule")
    set_extensions(".ispc")
    on_buildcmd_file( function (target, batchcmds, sourcefile, opt)
        local cache_dir = path.join(GlobalCache, "ispc")
        if not os.exists(cache_dir) then os.mkdir(cache_dir) end

        local objfile_type = ".obj"
        if is_plat("windows") then
            objfile_type = ".o"
        end
        local objfile = path.join(cache_dir, path.basename(sourcefile) .. objfile_type)
        local header = path.join(cache_dir, path.basename(sourcefile) .. ".h")

        batchcmds:show_progress(opt.progress, "compiling.ispc %s", sourcefile)

        local ispc_targets = ""
        local arch = target:arch()

        if arch and (arch:find("arm") or arch:find("aarch64")) then
            ispc_targets = "neon-i32x4,neon-i32x8"
        else
            ispc_targets = "sse2-i32x4,sse4.2-i32x8,avx1-i32x8,avx2-i32x8,avx512skx-x16"
        end

        batchcmds:execv("ispc", {
            sourcefile,
            "-O3",
            "--target=" .. ispc_targets,
            "-h", header,
            "-o", objfile
            --"-v"
        })

        batchcmds:add_depfiles(sourcefile)

        target:add("objects", objfile)
    end)

target("ispc_build")
    set_kind("object")
    add_files("**.ispc")
    add_rules("ispc_rule")

    before_link( function (target)
        local cache_dir = path.join(GlobalCache, "ispc")
        for _, obj in ipairs(os.files(path.join(cache_dir, "*.obj"))) do
            target:add("objects", obj)
        end
    end)

rule("cache_dependencies")
    ---@diagnostic disable-next-line: undefined-global
    after_load( function (target)
        for package_name, package_instance in pairs(target:pkgs()) do
            if package_instance then
                -- Headers
                local include_directories = package_instance:get("sysincludedirs") or package_instance:get("includedirs")
                if include_directories then
                    for _, inc_dir in ipairs(include_directories) do
                        local destination_directory = "cache/Libraries/include"
                        os.vcp(path.join(inc_dir, "*"), destination_directory, { rootdir = inc_dir })
                    end
                end

                -- Libs (.lib / .a)
                local lib_files = package_instance:get("libfiles")
                if lib_files then
                    for _, lib_file in ipairs(lib_files) do
                        local dst_file = path.join("cache/Libraries/lib", path.filename(lib_file))

                        if not os.isfile(dst_file) or os.mtime(lib_file) > os.mtime(dst_file) then
                            print("Caching binary: %s", path.filename(lib_file))
                            os.cp(lib_file, dst_file)
                        end
                    end
                end

                -- Dynamic Libs (.dll)
                local dll_files = package_instance:get("dllfiles")
                if dll_files then
                    local dll_directory = "../GDProject/bin/build"

                    if dll_directory and #dll_directory > 0 then
                        for _, dll_file in ipairs(dll_files) do
                            local destination_file = path.join(dll_directory, path.filename(dll_file))

                            if not os.isfile(destination_file) or os.mtime(dll_file) > os.mtime(destination_file) then
                                print("Caching DLL: %s", path.filename(dll_file))
                                os.cp(dll_file, destination_file)
                            end
                        end
                    end
                end
            end
        end
    end)

target("build")
    set_kind("phony")
    add_packages("zig")
    add_packages("enkits")
    add_packages("angelscript")
    add_packages("cpuinfo")
    add_packages("tracy")
    add_packages("ispc")
    add_packages("scons")
    add_packages("xwin")

    add_rules("cache_dependencies")

    add_deps("ispc_build")

    -- on_build( function (target)
    --     os.vrunv("zig", { "build" }, {
    --                 curdir = target:scriptdir(),
    --                 envs = target:pkgenvs()
    --             })
    -- end)
