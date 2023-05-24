This is a fork of discount. We build a shared library so we can sandbox it in
`mod_markdown`. As an example of how to use this library in a sandbox way see
[rlboxMain.cpp](rlboxMain.cpp).

## Building the shared library

1. Link [RLBox](https://github.com/shravanrn/rlbox_api) header files:

```
ln -s $(RLBOX_DIR)/rlbox.h
ln -s $(RLBOX_DIR)/RLBox_DynLib.h
```

2. Configure and build

```
./configure.sh
make
```

This will produce `libmarkdown.so` (and `rlboxMain` if you want a toy example). We will use `libmarkdown.so` in our `mod_markdown`.


# OLD README
DISCOUNT is a implementation of John Gruber & Aaron Swartz's
 Markdown markup language.   It implements, as far as I can tell,
all of the language as described in
 <http://daringfireball.net/projects/markdown/syntax>
and passes the Markdown test suite at
<http://daringfireball.net/projects/downloads/MarkdownTest_1.0.zip>

DISCOUNT is free software written by David Parsons
<orc@pell.portland.or.us>; it is released under a BSD-style license
that allows you to do as you wish with it as long as you don't
attempt to claim it as your own work.

Most of the programs included in the DISCOUNT distribution have
manual pages describing how they work.

The file INSTALL describes how to build and install discount
