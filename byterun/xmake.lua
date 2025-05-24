-- add_rules("mode.debug", "mode.release")
-- add_rules("c++.unity_build")

set_languages("c++23", "c23")

target("byterun")
    set_kind("binary")
    add_includedirs("include")
    add_files("../runtime/**.c", "../runtime/**.S")
    add_files("src/**.cpp", "src/**.c")
    remove_files("src/compiler.cpp")
    set_warnings("allextra")
    set_rundir("$(projectdir)")
    add_defines("WITH_CHECK")

