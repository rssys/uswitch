{
  "targets": [
    {
      "target_name": "mylib_node",
      "sources": [ "mylib_node.cc" ],
      "cflags_cc": [ "-std=c++14", "-Wl,--export-dynamic", "-ldl" ],
      "include_dirs": ["."]
    }
  ]
}
