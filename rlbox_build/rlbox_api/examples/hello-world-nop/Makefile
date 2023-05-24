.PHONY: hello clean

hello: main.cpp mylib.so
	$(CXX) -std=c++14 $(CFLAGS) -Wall main.cpp -Wl,--export-dynamic -ldl -o $@

mylib.so: mylib.c mylib.h
	$(CC) $(CFLAGS) -Wall -shared -fPIC $< -o $@

clean:
	-rm hello
	-rm -rf mylib.so
