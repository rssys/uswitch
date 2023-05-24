# Sandboxing_NaCl
A modification of NaCl to support sandoxing of dynamic libraries from the main app

This is the one of the repos for the paper "Retrofitting Fine Grain Isolation in the Firefox Renderer" submitted to USENIX 2020 in which we introduce the RLBox sandboxing framework. Refer to the root repository git@github.com:shravanrn/LibrarySandboxing.git in general for using this repo. You can also use just this repo by rerferring the instructions below.

# First time setup instructions

The following commands should be run the first time only

First install the depot tools software as per http://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up or by running the following commands

```
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH="$(pwd)/depot_tools:$PATH"
```

Install gyp and some packages
```
sudo apt install python-setuptools
git clone https://chromium.googlesource.com/external/gyp.git
cd gyp
sudo apt install python-setuptools
sudo python setup.py install
```

# Build instructions

The following Makefile targets exist:
```
buildopt32 buildopt64 runopt32 runopt64 builddebug32 builddebug64 rundebug32 rundebug64 runperftest32 runperftest64 clean
```
For example, to build and test 64-bit normal build:
```
make runopt64
```

For verbose build add SCONS_FLAGS=--verbose e.g.
```
make SCONS_FLAGS=--verbose runopt64
```

Perf benchmark that measures just the switching cost (jump into the sandbox and jump out) is built and run separately with the targets
```
runperftest32 runperftest64
```

(Note - instructions for how to fetch a fresh copy of NaCl code are provided in MyBuildInstr.txt. This is not necessary unless you want to rebase this project on the latest NaCl)

# Usage
Example code, which also serves as a quick test is located in native_client/src/trusted/dyn_ldr/testing/dyn_ldr_test.c.
Here is an example of using libjpeg, in a sandboxed manner - https://github.com/shravanrn/libjpeg-turbo_nacltests
Here is an example of using libjpeg, inside firefox in a sandboxed manner - https://github.com/shravanrn/mozilla_firefox_nacl.git
Some related Code - A modification of NASM to assembly NACL complicant assembly - https://github.com/shravanrn/NASM_NaCl/blob/master/README
