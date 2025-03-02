-- add_rules("mode.debug", "mode.release")
-- add_rules("c++.unity_build")

set_languages("c++20", "c20")

target("byterun")
    set_kind("binary")
    add_includedirs("include")
    add_files("../runtime/runtime.a")
    add_files("src/**.cpp", "src/**.c")
    remove_files("src/compiler.cpp")
    set_warnings("allextra")
    set_rundir("$(projectdir)")

