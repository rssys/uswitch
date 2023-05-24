libfast_memcpy.a: fast_memcpy.o
	ar rcs libfast_memcpy.a fast_memcpy.o memmove-avx-unaligned-erms.o memset-avx2-unaligned-erms.o
fast_memcpy.o: fast_memcpy.c
	gcc fast_memcpy.c -nostdlib -c -O3
clean:
	rm -f fast_memcpy.o libfast_memcpy.a