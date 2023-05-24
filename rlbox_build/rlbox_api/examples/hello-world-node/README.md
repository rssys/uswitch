This is a simple hello world example that uses the RLBox API to call functions
from a simple library from Node.JS. This example doesn't actually sandbox
anything. It just loads the library dynamically and calls a bunch of library
functions. This example extends the [hello-world-nop](../hello-world-nop) example.

- `mylib.{h,c}` is the simple library
- `hello.js` is our main program

### Install node.js

First, install [NVM](https://github.com/nvm-sh/nvm). Then install Node.JS 12:

```
nvm install 12
nvm alias default 12
```

### Build and run this example

```
make hello
node hello.js
```

Running the program should produce:

```
Hello world from mylib
Adding... 3+4 = 7
> mylib: hi hi!
```
