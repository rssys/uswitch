This is a simple hello world example that uses the RLBox API to call functions
from a simple library. This example doesn't sandbox anything. It just loads the
library dynamically and calls a bunch of library functions.

- `mylib.{h,c}` is the simple library
- `main.cpp` is our main program

### Build  and run

```
make
./hello
```

Running the program should produce:

```
Hello world!
Hello world from mylib
Adding... 3+4 = 7
OK? = 1
> mylib: hi hi!
```
