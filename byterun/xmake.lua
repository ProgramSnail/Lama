add_rules("mode.debug", "mode.release")

set_languages("c23")

target("byterun")
    set_kind("binary")
    add_includedirs("include")
    add_headerfiles("include/*.h")
    add_files("src/*.c")
