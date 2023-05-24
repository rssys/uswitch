.PHONY: hello clean

hello: mylib.so
	npm install

mylib.so: mylib.c mylib.h
	$(CC) $(CFLAGS) -Wall -shared -fPIC $< -o $@

clean:
	-rm -rf build
	-rm -rf mylib.so
