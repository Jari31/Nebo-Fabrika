add_requires("ispc", "slang", "zig")

local GlobalCache = path.join(os.projectdir(), "cache")

rule("ispc_rule")
    set_extensions(".ispc")
    on_buildcmd_file( function (target, batchcmds, sourcefile, opt)
        local cache_dir = path.join(GlobalCache, "ispc")
        if not os.exists(cache_dir) then os.mkdir(cache_dir) end

        local objfile = path.join(cache_dir, path.basename(sourcefile) .. ".obj")
        local header = path.join(cache_dir, path.basename(sourcefile) .. ".h")

        batchcmds:execv("ispc", {
            sourcefile,
            "-O3",
            "--target=sse2-i32x4,sse4-i32x4,avx1-i32x8,avx2-i32x8,avx2-i32x16,avx512skx-x16,avx512icl-x16,avx512spr-x16,avx512gnr-x16,avx10.2dmr-x16,avx10.2nvl-x16,neon-i32x4,neon-i16x16,neon-i8x32,wasm32-i32x4",
            "-h", header,
            "-o", objfile,
            "-v"
        })

        batchcmds:add_depfiles(sourcefile)
        target:add("objects", objfile)
    end)

target("test_ispc")
    set_kind("static")
    add_packages("ispc")
    add_rules("ispc_rule")
    add_files("**.ispc")
    add_files("dummy.c")

    before_link( function (target)
        local cache_dir = path.join(GlobalCache, "ispc")
        for _, obj in ipairs(os.files(path.join(cache_dir, "*_*.obj"))) do
            target:add("objects", obj)
        end
    end)
