typedef struct {} jmp_buf;

static inline int setjmp(jmp_buf env) {
    return 0;
}

static inline void longjmp(jmp_buf env, int val) {}