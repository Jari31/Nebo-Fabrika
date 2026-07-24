add_requires("toml++", "cli11")

-- do xmake -P . if trying to run as a submodule (i.e., another XMake file exists as a parent higher up in the scope)
target("fetch_dependencies")
    set_kind("headeronly")
    add_packages("toml++")
    add_packages("cli11")

    on_build( function (target) -- copied over from the main build file to avoid the chicken and egg problem of who gets what module in what repo
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
