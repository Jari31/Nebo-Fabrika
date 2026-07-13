add_requires("ispc", "slang", "zig", "taskflow")

local GlobalCache = path.join(os.projectdir(), "cache")
--set_config("packagedir", path.join(GlobalCache, "cache/Libraries"))

rule("ispc_rule")
    set_extensions(".ispc")
    on_buildcmd_file( function (target, batchcmds, sourcefile, opt)
        local cache_dir = path.join(GlobalCache, "ispc")
        if not os.exists(cache_dir) then os.mkdir(cache_dir) end

        local objfile = path.join(cache_dir, path.basename(sourcefile) .. ".obj")
        local header = path.join(cache_dir, path.basename(sourcefile) .. ".h")

        batchcmds:show_progress(opt.progress, "compiling.ispc %s", sourcefile)

        batchcmds:execv("ispc", {
            sourcefile,
            "-O3",
            "--target=sse4.2-i32x8,avx1-i32x8,avx2-i32x8,avx512skx-x8,neon-i32x8",
            "-h", header,
            "-o", objfile
            --"-v"
        })

        batchcmds:add_depfiles(sourcefile)

        target:add("objects", objfile)
    end)

target("test_ispc")
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
--[[
target("fetch_dependencies")
    set_kind("headeronly")
    add_packages("taskflow")

    on_build(function (target)
        import("core.project.project")

        for package_name, _ in pairs(target:pkgs()) do
            local package_instance = target:pkg(package_name)

            local include_directories = package_instance:get("sysincludedirs") or package_instance:get("includedirs")
            if include_directories then
                for _, include_directory in ipairs(include_directories) do
                    print("--> Copying " .. package_name .. " headers from " .. include_directory)
                    os.cp(include_directory .. "/*", "cache/Libraries/")
                end
            end
        end
    end)
]]--
