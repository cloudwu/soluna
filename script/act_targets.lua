local lm = require "luamake"

lm:rule "act_lua" {
    args = { "$luamake", "lua", "$args" },
    description = "$args",
    pool = "console",
}

lm:build "pages" {
    rule = "act_lua",
    args = { "@act.lua", "pages" },
    inputs = { "act.lua" },
}

lm:build "nightly" {
    rule = "act_lua",
    args = { "@act.lua", "nightly", "host_os=" .. lm.os },
    inputs = { "act.lua" },
}

lm:phony "act" {
    inputs = "act.lua",
}
