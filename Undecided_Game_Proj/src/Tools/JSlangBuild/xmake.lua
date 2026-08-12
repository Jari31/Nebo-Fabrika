add_requires("toml++ master", "cli11 v2.7.2", "xxhash v0.8.3", "unordered_dense v4.9.0", "enkits v1.12", "dylib v3.0.1", "fmt 12.2.0", "whereami 2024.08.26")
add_requires("slang 2026.14.1", { configs = { binary = true } })
add_requires("zig v0.16", { verify = false })

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

package("slang") -- because building from source for C++ is a nightmare
    set_homepage("https://github.com/shader-slang/slang")
    set_description("Slang is a shading language and compiler framework.")

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
    if is_host("windows") then
        set_urls("https://github.com/Jake-Shadle/xwin/releases/download/$(version)/xwin-$(version)-$(arch)-pc-windows-msvc.tar.gz")
    else
        set_urls("https://github.com/Jake-Shadle/xwin/releases/download/$(version)/xwin-$(version)-$(arch)-unknown-$(host)-musl.tar.gz")
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
--https://ziglang.org/builds/zig-x86_64-windows-0.17.0-dev.1525+91c6d8a09.zip https://ziglang.org/download/0.16.0/zig-x86_64-windows-0.16.0.zip
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

rule("cache_dependencies")
    ---@diagnostic disable-next-line: undefined-global
    after_load( function (target)

        for package_name, package_instance in pairs(target:pkgs()) do
            if package_instance then
                -- Headers
                local include_directories = package_instance:get("sysincludedirs") or package_instance:get("includedirs")
                if include_directories then
                    for _, include_directory in ipairs(include_directories) do
                        local destination_directory = "cache/Libraries/include"
                        os.vcp(path.join(include_directory, "**"), destination_directory, { rootdir = include_directory })
                    end
                end

                -- Libs (.lib / .a)
                local lib_files = package_instance:get("libfiles")
                if lib_files then
                    for _, lib_file in ipairs(lib_files) do
                        local destination_file = path.join("cache/Libraries/lib", path.filename(lib_file))

                        if not os.isfile(destination_file) or os.mtime(lib_file) > os.mtime(destination_file) then
                            print("Caching binary: %s", path.filename(lib_file))
                            os.cp(lib_file, destination_file)
                        end
                    end
                end

                -- Dynamic Libs (.dll)
                local dll_files = package_instance:get("dllfiles")
                if dll_files then
                    local dll_directory = "cache/"

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


target("jslang")
    set_kind("binary")
    add_packages("@zig")
    add_packages("toml++")
    add_packages("cli11")
    add_packages("xxhash")
    add_packages("enkits")
    add_packages("unordered_dense")
    add_packages("dylib")
    add_packages("fmt")
    add_packages("whereami")
    add_packages("slang")

    add_rules("cache_dependencies")

    on_build( function (target)
        os.vrunv("zig", { "build" }, {
                    curdir = target:scriptdir(),
                    envs = target:pkgenvs()
                })
    end)
