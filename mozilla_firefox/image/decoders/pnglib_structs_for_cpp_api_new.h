//Auto generated with the modified clang - Pass the arg "-Xclang -fdump-struct-layouts-for-sbox" to clang while compiling

#define sandbox_fields_reflection_pnglib_class_z_stream_s(f, g, ...) \
	f(const Bytef *, next_in, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, avail_in, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, total_in, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(Bytef *, next_out, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, avail_out, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, total_out, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(const char *, msg, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct internal_state *, state, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(voidpf (*)(voidpf, uInt, uInt), zalloc, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(voidpf, voidpf), zfree, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void *, opaque, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, data_type, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, adler, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, reserved, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_color_16_struct(f, g, ...) \
	f(unsigned char, index, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, red, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, green, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, blue, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, gray, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_color_8_struct(f, g, ...) \
	f(unsigned char, red, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, green, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, blue, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, gray, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, alpha, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_unknown_chunk_t(f, g, ...) \
	f(png_byte [5], name, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_byte *, data, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, size, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, location, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_xy(f, g, ...) \
	f(int, redx, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, redy, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, greenx, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, greeny, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, bluex, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, bluey, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, whitex, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, whitey, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_XYZ(f, g, ...) \
	f(int, red_X, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, red_Y, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, red_Z, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, green_X, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, green_Y, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, green_Z, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, blue_X, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, blue_Y, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, blue_Z, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_colorspace(f, g, ...) \
	f(int, gamma, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct png_xy, end_points_xy, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct png_XYZ, end_points_XYZ, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, rendering_intent, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, flags, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_struct_def(f, g, ...) \
	f(struct __jmp_buf_tag [1], jmp_buf_local, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(struct __jmp_buf_tag *, int), longjmp_fn, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(jmp_buf *, jmp_buf_ptr, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, jmp_buf_size, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(png_structp, png_const_charp), error_fn, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(png_structp, png_const_charp), warning_fn, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void *, error_ptr, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(png_structp, png_bytep, png_size_t), write_data_fn, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(png_structp, png_bytep, png_size_t), read_data_fn, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void *, io_ptr, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, mode, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, flags, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, transformations, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, zowner, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct z_stream_s, zstream, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct png_compression_buffer *, zbuffer_list, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, zbuffer_size, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, zlib_level, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, zlib_method, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, zlib_window_bits, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, zlib_mem_level, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, zlib_strategy, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, zlib_set_level, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, zlib_set_method, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, zlib_set_window_bits, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, zlib_set_mem_level, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, zlib_set_strategy, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, width, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, height, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, num_rows, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, usr_width, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, rowbytes, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, iwidth, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, row_number, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, chunk_name, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_byte *, prev_row, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_byte *, row_buf, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, info_rowbytes, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, idat_size, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, crc, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_color *, palette, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, num_palette, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, num_trans, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, compression, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, filter, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, interlaced, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, pass, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, do_filter, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, color_type, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, bit_depth, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, usr_bit_depth, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, pixel_depth, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, channels, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, usr_channels, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, sig_bytes, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, maximum_pixel_depth, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, transformed_pixel_depth, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, zstream_start, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(png_structp), output_flush_fn, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, flush_dist, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, flush_rows, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, gamma_shift, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, screen_gamma, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_byte *, gamma_table, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_uint_16 **, gamma_16_table, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct png_color_8_struct, sig_bit, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_byte *, trans_alpha, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct png_color_16_struct, trans_color, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(png_structp, png_uint_32, int), read_row_fn, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(png_structp, png_uint_32, int), write_row_fn, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(png_structp, png_infop), info_fn, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(png_structp, png_bytep, png_uint_32, int), row_fn, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(png_structp, png_infop), end_fn, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_byte *, save_buffer_ptr, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_byte *, save_buffer, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_byte *, current_buffer_ptr, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_byte *, current_buffer, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, push_length, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, skip_length, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, save_buffer_size, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, save_buffer_max, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, buffer_size, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, current_buffer_size, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, process_mode, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, cur_palette, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, options, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, free_me, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, apng_flags, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, next_seq_num, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, first_frame_width, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, first_frame_height, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, num_frames_read, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(png_structp, png_uint_32), frame_info_fn, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(png_structp, png_uint_32), frame_end_fn, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, num_frames_to_write, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, num_frames_written, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_byte *, big_row_buf, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, compression_type, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, user_width_max, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, user_height_max, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, user_chunk_cache_max, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, user_chunk_malloc_max, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, old_big_row_buf_size, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_byte *, read_buffer, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, read_buffer_size, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_byte *, big_prev_row, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*[4])(png_row_infop, png_bytep, png_const_bytep), read_filter, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct png_colorspace, colorspace, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, num_exif, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_row_info_struct(f, g, ...) \
	f(unsigned int, width, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, rowbytes, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, color_type, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, bit_depth, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, channels, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, pixel_depth, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_compression_buffer(f, g, ...) \
	f(struct png_compression_buffer *, next, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_byte [1], output, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_color_struct(f, g, ...) \
	f(unsigned char, red, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, green, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, blue, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_time_struct(f, g, ...) \
	f(unsigned short, year, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, month, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, day, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, hour, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, minute, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, second, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_info_def(f, g, ...) \
	f(unsigned int, width, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, height, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, valid, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, rowbytes, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_color *, palette, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, num_palette, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, num_trans, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, bit_depth, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, color_type, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, compression_type, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, filter_type, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, interlace_type, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, channels, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, pixel_depth, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, spare_byte, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_byte [8], signature, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct png_colorspace, colorspace, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(char *, iccp_name, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_byte *, iccp_profile, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, iccp_proflen, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_byte *, trans_alpha, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct png_color_16_struct, trans_color, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, free_me, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, num_frames, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, num_plays, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, next_frame_width, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, next_frame_height, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, next_frame_x_offset, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, next_frame_y_offset, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, next_frame_delay_num, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, next_frame_delay_den, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, next_frame_dispose_op, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, next_frame_blend_op, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_text_struct(f, g, ...) \
	f(int, compression, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(char *, key, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(char *, text, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, text_length, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, itxt_length, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(char *, lang, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(char *, lang_key, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_sPLT_struct(f, g, ...) \
	f(char *, name, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, depth, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_sPLT_entry *, entries, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, nentries, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_sPLT_entry_struct(f, g, ...) \
	f(unsigned short, red, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, green, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, blue, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, alpha, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, frequency, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_control(f, g, ...) \
	f(png_struct *, png_ptr, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_info *, info_ptr, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void *, error_buf, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(const png_byte *, memory, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, size, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, for_write, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, owned_file, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_qcms_CIE_xyY(f, g, ...) \
	f(double, x, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(double, y, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(double, Y, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_qcms_CIE_xyYTRIPLE(f, g, ...) \
	f(qcms_CIE_xyY, red, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(qcms_CIE_xyY, green, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(qcms_CIE_xyY, blue, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_get_iCCP_params(f, g, ...) \
	f(png_uint_32, profileLen, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_bytep, profileData, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_charp, profileName, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, compression, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_get_cHRM_And_gAMA_params(f, g, ...) \
	f(qcms_CIE_xyYTRIPLE, primaries, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(qcms_CIE_xyY, whitePoint, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(double, gammaOfFile, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_get_IHDR_params(f, g, ...) \
	f(png_uint_32, width, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_uint_32, height, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, bit_depth, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, color_type, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, interlace_type, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, compression_type, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, filter_type, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_class_png_get_tRNS_params(f, g, ...) \
	f(png_bytep, trans, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(png_color_16p, trans_values, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, num_trans, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_pnglib_allClasses(f, ...) \
	f(z_stream_s, pnglib, ##__VA_ARGS__) \
	f(png_color_16_struct, pnglib, ##__VA_ARGS__) \
	f(png_color_8_struct, pnglib, ##__VA_ARGS__) \
	f(png_unknown_chunk_t, pnglib, ##__VA_ARGS__) \
	f(png_xy, pnglib, ##__VA_ARGS__) \
	f(png_XYZ, pnglib, ##__VA_ARGS__) \
	f(png_colorspace, pnglib, ##__VA_ARGS__) \
	f(png_row_info_struct, pnglib, ##__VA_ARGS__) \
	f(png_compression_buffer, pnglib, ##__VA_ARGS__) \
	f(png_color_struct, pnglib, ##__VA_ARGS__) \
	f(png_time_struct, pnglib, ##__VA_ARGS__) \
	f(png_struct_def, pnglib, ##__VA_ARGS__) \
	f(png_info_def, pnglib, ##__VA_ARGS__) \
	f(png_text_struct, pnglib, ##__VA_ARGS__) \
	f(png_sPLT_struct, pnglib, ##__VA_ARGS__) \
	f(png_sPLT_entry_struct, pnglib, ##__VA_ARGS__) \
	f(png_control, pnglib, ##__VA_ARGS__) \
	f(qcms_CIE_xyY, pnglib, ##__VA_ARGS__) \
	f(qcms_CIE_xyYTRIPLE, pnglib, ##__VA_ARGS__) \
	f(png_get_iCCP_params, pnglib, ##__VA_ARGS__) \
	f(png_get_cHRM_And_gAMA_params, pnglib, ##__VA_ARGS__) \
	f(png_get_IHDR_params, pnglib, ##__VA_ARGS__) \
	f(png_get_tRNS_params, pnglib, ##__VA_ARGS__)
