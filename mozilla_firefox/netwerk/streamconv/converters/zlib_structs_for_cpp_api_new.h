#define sandbox_fields_reflection_zlib_class_z_stream_s(f, g, ...) \
    f(z_const Bytef *,next_in, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(uInt           ,avail_in, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(uLong          ,total_in, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(Bytef *        ,next_out, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(uInt           ,avail_out, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(uLong          ,total_out, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(z_const char * ,msg, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(void *         ,state, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(alloc_func     ,zalloc, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(free_func      ,zfree, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(voidpf         ,opaque, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(int            ,data_type, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(uLong          ,adler, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(uLong          ,reserved, FIELD_NORMAL, ##__VA_ARGS__) \
    g()

#define sandbox_fields_reflection_zlib_class_gz_header(f, g, ...) \
    f(int,     text, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(uLong,   time, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(int,     xflags, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(int,     os, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(Bytef*,  extra, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(uInt,    extra_len, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(uInt,    extra_max, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(Bytef*,  name, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(uInt,    name_max, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(Bytef*,  comment, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(uInt,    comm_max, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(int,     hcrc, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(int,     done, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \


#define sandbox_fields_reflection_zlib_allClasses(f, ...) \
    f(z_stream_s, zlib, ##__VA_ARGS__) \
    f(gz_header, zlib, ##__VA_ARGS__)
