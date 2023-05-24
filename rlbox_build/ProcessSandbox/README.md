# ProcessSandbox

Sandboxing dynamic libraries via separate processes

## Building

Adjust the variables at the top of the Makefile (especially the ones related to
`libjpeg-turbo_nacltests`) to fit your system. Then, use:

* `make all` to build everything
* `make all32` or `make all64` to build all 32-bit or all 64-bit components only
* `make run32` or `make run64` to run a test script (which sandboxes a dummy
  library consisting of very simple functions) in 32-bit or 64-bit
* `make debug32` or `make debug64` to run under gdb with debug build

Switching between debug and non-debug running may require a `make clean` in between.

## Adopting for a new library

The only thing that should be required is creating a new version of the file
`library_helpers.h` / `jpeglib_helpers.h` / `pnglib_helpers.h` / `libtheora_helpers.h` / `libvpx_helpers.h` / `libvorbis_helpers.h`
(those are the files for our dummy library, libjpeg, libpng, libtheora, libvpx,
and libvorbis respectively). This entails little more than just inputting the
information about all the library's methods and their signatures in the
appropriate format.
