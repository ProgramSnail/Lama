-- add_rules("mode.debug", "mode.release")
-- add_rules("c++.unity_build")

set_languages("c++20", "c11")

target("byterun")
    set_kind("binary")
    add_cxflags("-O3")
    add_includedirs("include", "../runtime")
    add_files("../runtime/**.c", "../runtime/**.S")
    add_files( "src/**.cpp", "src/**.c")
    set_warnings("allextra")
    set_rundir("$(projectdir)")
    add_defines("WITH_CHECK")

