set_project("CuteJson")
set_languages("c++2a")

add_rules("plugin.compile_commands.autoupdate")
set_toolchains("clang")

target("main", function()
    set_kind("binary")
    set_encodings("utf-8")
    add_files("./cutejson.cpp")
end)
