set_config("vs_toolset", "14.43")
if os.subhost() == "windows" then
    add_requires("msvc 14.43.17+13")
    set_toolchains("@msvc")
elseif os.subhost() == "linux" then
    add_requires("msvc-wine")
    set_toolchains("@msvc-wine")
end
    -- set_toolchains("mingw@llvm-mingw")

-- add_requireconfs("*", {
--     build = true, -- Disable prebuilt binary downloads
--     configs = {
--         toolchains = "mingw@llvm-mingw"
--     }
-- })

-- add_requires("toml++ master", "cli11", "xxhash", "unordered_dense", "enkits") --, { configs = { toolchains = "mingw@llvm-mingw", cxflags = "-stdlib=libc++" } })
    -- ,{configs = {shared = false, vs_runtime = "MT"}, system = false})
add_requires("zig v0.16", { verify = false })

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
    before_build( function (target)
        for package_name, package_instance in pairs(target:pkgs()) do
            if package_instance then
                local include_dirs = package_instance:get("sysincludedirs") or package_instance:get("includedirs")
                if include_dirs then
                    for _, inc_dir in ipairs(include_dirs) do
                        local destination_directory = "cache/Libraries/include"
                        os.vcp(path.join(inc_dir, "*"), destination_directory, { rootdir = inc_dir })
                    end
                end

                local lib_files = package_instance:libraryfiles()--package_instance:get("libfiles")
                if lib_files then
                    for _, lib_file in ipairs(lib_files) do
                        local destination_file = path.join("cache/Libraries/lib", path.filename(lib_file))

                        if not os.isfile(destination_file) or os.mtime(lib_file) > os.mtime(destination_file) then
                            print("Caching binary: %s", path.filename(lib_file))
                            os.cp(lib_file, destination_file)
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

    add_rules("cache_dependencies")

    on_build( function (target)
        os.vrunv("zig", { "build" }, {
                    curdir = target:scriptdir(),
                    envs = target:pkgenvs()
                })
    end)
