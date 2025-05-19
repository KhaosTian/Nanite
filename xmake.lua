set_project("Nanity")
set_version("0.1.0")

set_arch("x64")
set_plat("windows")
set_languages("c++20")
set_toolchains("msvc")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})
add_rules("plugin.vsxmake.autoupdate")

add_requires("spdlog", "glm", "meshoptimizer 0.22")

if is_mode("debug") then 
    add_defines("_DEBUG")
    set_runtimes("MDd")
elseif is_mode("release") then 
    add_defines("_NDEBUG")
    set_runtimes("MD")
end

-- Core Nanity library
target("NanityCore")
    set_kind("static")
    
    add_packages("spdlog", "glm", "meshoptimizer")
    
    add_includedirs("source")
    add_includedirs("external/metis/include")
    add_linkdirs("external/metis/lib")
    
    add_files("source/**.cpp|unity_plugin.cpp")
    add_headerfiles("source/**.h")
    
    add_links("metis")
target_end()

-- Unity plugin DLL
target("NanityPlugin")
    set_kind("shared")
    add_defines("UNITY_PLUGIN")
    
    add_deps("NanityCore")
    add_packages("spdlog", "glm", "meshoptimizer")
    
    add_includedirs("source")
    
    add_files("source/unity_plugin.cpp")
target_end()

