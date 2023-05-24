cmake_minimum_required(VERSION 3.10)

project(libtheora
        VERSION 1
        DESCRIPTION "libtheora included in Firefox 57")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(theora STATIC
                "lib/apiwrapper.c"
                "lib/bitpack.c"
                "lib/decapiwrapper.c"
                "lib/decinfo.c"
                "lib/decode.c"
                "lib/dequant.c"
                "lib/fragment.c"
                "lib/huffdec.c"
                "lib/idct.c"
                "lib/info.c"
                "lib/internal.c"
                "lib/quant.c"
                "lib/state.c"
                "lib/x86/mmxfrag.c"
                "lib/x86/mmxidct.c"
                "lib/x86/mmxstate.c"
                "lib/x86/sse2idct.c"
                "lib/x86/x86cpu.c"
                "lib/x86/x86state.c"
                "lib/ogg/ogg_alloc.c"
                "lib/ogg/ogg_bitwise.c"
                "lib/ogg/ogg_framing.c"
                )

target_include_directories(theora PUBLIC "lib")
target_include_directories(theora PUBLIC "include")
target_compile_definitions(theora PRIVATE THEORA_DISABLE_ENCODE=True)