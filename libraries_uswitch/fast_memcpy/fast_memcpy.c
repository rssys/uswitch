#include <stddef.h>

long __x86_shared_non_temporal_threshold = 67108864l;

extern void *__mempcpy_avx_unaligned_erms(void *dest, const void *src, size_t len);
extern void *__memcpy_avx_unaligned_erms(void *dest, const void *src, size_t len);
extern void *__memmove_avx_unaligned_erms(void *dest, const void *src, size_t len);
extern void *__memset_avx2_unaligned_erms(void *dest, int ch, size_t count);

void *memcpy(void *dest, const void *src, size_t len) {
    return __memcpy_avx_unaligned_erms(dest, src, len);
}

void *mempcpy(void *dest, const void *src, size_t len) {
    return __mempcpy_avx_unaligned_erms(dest, src, len);
}

void *memmove(void *dest, const void *src, size_t len) {
    return __memmove_avx_unaligned_erms(dest, src, len);
}

void *memset(void *dest, int ch, size_t count) {
    return __memset_avx2_unaligned_erms(dest, ch, count);
}