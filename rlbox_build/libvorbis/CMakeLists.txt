cmake_minimum_required(VERSION 3.10)

project(libvorbis
        VERSION 1
        DESCRIPTION "libvorbis included in Firefox 57")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(vorbis STATIC
                    "lib/vorbis_analysis.c"
                    "lib/vorbis_bitrate.c"
                    "lib/vorbis_block.c"
                    "lib/vorbis_envelope.c"
                    "lib/vorbis_floor0.c"
                    "lib/vorbis_lookup.c"
                    "lib/vorbis_lpc.c"
                    "lib/vorbis_lsp.c"
                    "lib/vorbis_mapping0.c"
                    "lib/vorbis_mdct.c"
                    "lib/vorbis_psy.c"
                    "lib/vorbis_registry.c"
                    "lib/vorbis_sharedbook.c"
                    "lib/vorbis_smallft.c"
                    "lib/vorbis_synthesis.c"
                    "lib/vorbisenc.c"

                    "lib/vorbis_codebook.c"
                    "lib/vorbis_floor1.c"
                    "lib/vorbis_info.c"
                    "lib/vorbis_res0.c"
                    "lib/vorbis_window.c"

                    "lib/ogg/ogg_alloc.c"
                    "lib/ogg/ogg_bitwise.c"
                    "lib/ogg/ogg_framing.c"
                )

target_include_directories(vorbis PUBLIC "lib")
target_include_directories(vorbis PUBLIC "include")
target_compile_definitions(vorbis PRIVATE VORBIS_DISABLE_ENCODE=True)