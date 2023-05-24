#include "zlib.h"
#include <stddef.h>
#include <stdio.h>

int main() {
	printf("%u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
		offsetof(z_stream, next_in),
		offsetof(z_stream, avail_in),
		offsetof(z_stream, total_in),
		offsetof(z_stream, next_out),
		offsetof(z_stream, avail_out),
		offsetof(z_stream, total_out),
		offsetof(z_stream, msg),
		offsetof(z_stream, state),
		offsetof(z_stream, zalloc),
		offsetof(z_stream, zfree),
		offsetof(z_stream, opaque),
		offsetof(z_stream, data_type),
		offsetof(z_stream, adler),
		offsetof(z_stream, reserved)
	);
}
