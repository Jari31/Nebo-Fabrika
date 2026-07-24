add_requires("ispc", "slangi", "zig", "enkits", "angelscript", "cpuinfo", "tracy")

set_runtimes("MD")

local GlobalCache = path.join(os.projectdir() or "", "cache")
--set_config("packagedir", path.join(GlobalCache, "cache/Libraries"))
local BuildFolder = "../GDProject/bin/build/"

package("slangi")
    set_homepage("https://shader-slang.com/")
    set_description("Making it easier to work with shaders")

    set_urls("https://github.com/shader-slang/slang.git")
    add_versions("v2026.12.0.1", "B77C44573277DD235399337DE38339EF5E41EAD0CB99882AC82DBB330F51A51E")

    on_install( function (package)
        local configs = { }
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        table.insert(configs, "-DSLANG_BUILD_EXAMPLES=OFF")
        table.insert(configs, "-DSLANG_ENABLE_TESTS=OFF")
        table.insert(configs, "-DSLANG_ENABLE_GFX=OFF")
        table.insert(configs, "-DSLANG_ENABLE_SLANG_RHI=OFF")

        import("package.tools.cmake").install(package, configs)
    end)

rule("ispc_rule")
    set_extensions(".ispc")
    on_buildcmd_file( function (target, batchcmds, sourcefile, opt)
        local cache_dir = path.join(GlobalCache, "ispc")
        if not os.exists(cache_dir) then os.mkdir(cache_dir) end

        local objfile = path.join(cache_dir, path.basename(sourcefile) .. ".obj")
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

target("ispc")
    set_kind("object")
    add_packages("ispc")
    add_files("**.ispc")
    add_rules("ispc_rule")

    before_link( function (target)
        local cache_dir = path.join(GlobalCache, "ispc")
        for _, obj in ipairs(os.files(path.join(cache_dir, "*.obj"))) do
            target:add("objects", obj)
        end
    end)

target("fetch_dependencies")
    set_kind("headeronly")
    add_packages("enkits")
    add_packages("angelscript")
    add_packages("cpuinfo")
    add_packages("tracy")
    add_packages("slangi")

    on_build( function (target)
        for package_name, _ in pairs(target:pkgs()) do
            local package_instance = target:pkg(package_name)
            if package_instance then
                local include_directories = package_instance:get("sysincludedirs") or package_instance:get("includedirs")
                if include_directories then
                    for _, include_directory in ipairs(include_directories) do
                        print("Copying " .. package_name .. " headers from " .. include_directory)
                        os.cp(include_directory .. "/*", "cache/Libraries/include/")
                    end
                end

                local lib_files = package_instance:get("libfiles")
                if lib_files then
                    for _, lib_file in ipairs(lib_files) do
                        print("Copying " .. package_name .. " binary: " .. lib_file)
                        os.cp(lib_file, "cache/Libraries/lib/")
                    end
                end
            end
        end
    end)


    after_build( function (targets)
        print("Copying over .dll files over to ", BuildFolder)

        os.cp(path.join(GlobalCache, "Libraries/lib/*.dll"), BuildFolder)
    end)
