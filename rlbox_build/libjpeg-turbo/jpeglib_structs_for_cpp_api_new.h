//Auto generated with the modified clang - Pass the arg "-Xclang -fdump-struct-layouts-for-sbox" to clang while compiling

#define sandbox_fields_reflection_jpeglib_class_jpeg_error_mgr(f, g, ...) \
	f(void (*)(j_common_ptr), error_exit, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(j_common_ptr, int), emit_message, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(j_common_ptr), output_message, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(j_common_ptr, char *), format_message, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(j_common_ptr), reset_error_mgr, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, msg_code, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(char[80], msg_parm, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, trace_level, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(long, num_warnings, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(const char *const *, jpeg_message_table, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, last_jpeg_message, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(const char *const *, addon_message_table, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, first_addon_message, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, last_addon_message, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_jpeglib_class_decoder_error_mgr(f, g, ...) \
    f(struct jpeg_error_mgr, pub, FIELD_NORMAL, ##__VA_ARGS__) \
    g() \
    f(jmp_buf, setjmp_buffer, FIELD_NORMAL, ##__VA_ARGS__) \
    g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_common_struct(f, g, ...) \
	f(struct jpeg_error_mgr *, err, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_memory_mgr *, mem, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_progress_mgr *, progress, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void *, client_data, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, is_decompressor, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, global_state, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_memory_mgr(f, g, ...) \
	f(void *(*)(j_common_ptr, int, size_t), alloc_small, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void *(*)(j_common_ptr, int, size_t), alloc_large, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JSAMPARRAY (*)(j_common_ptr, int, JDIMENSION, JDIMENSION), alloc_sarray, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JBLOCKARRAY (*)(j_common_ptr, int, JDIMENSION, JDIMENSION), alloc_barray, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(jvirt_sarray_ptr (*)(j_common_ptr, int, boolean, JDIMENSION, JDIMENSION, JDIMENSION), request_virt_sarray, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(jvirt_barray_ptr (*)(j_common_ptr, int, boolean, JDIMENSION, JDIMENSION, JDIMENSION), request_virt_barray, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(j_common_ptr), realize_virt_arrays, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JSAMPARRAY (*)(j_common_ptr, jvirt_sarray_ptr, JDIMENSION, JDIMENSION, boolean), access_virt_sarray, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JBLOCKARRAY (*)(j_common_ptr, jvirt_barray_ptr, JDIMENSION, JDIMENSION, boolean), access_virt_barray, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(j_common_ptr, int), free_pool, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(j_common_ptr), self_destruct, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(long, max_memory_to_use, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(long, max_alloc_chunk, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_progress_mgr(f, g, ...) \
	f(void (*)(j_common_ptr), progress_monitor, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(long, pass_counter, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(long, pass_limit, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, completed_passes, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, total_passes, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_compress_struct(f, g, ...) \
	f(struct jpeg_error_mgr *, err, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_memory_mgr *, mem, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_progress_mgr *, progress, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void *, client_data, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, is_decompressor, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, global_state, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_destination_mgr *, dest, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, image_width, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, image_height, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, input_components, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(J_COLOR_SPACE, in_color_space, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(double, input_gamma, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, data_precision, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, num_components, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(J_COLOR_SPACE, jpeg_color_space, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(jpeg_component_info *, comp_info, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JQUANT_TBL *[4], quant_tbl_ptrs, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JHUFF_TBL *[4], dc_huff_tbl_ptrs, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JHUFF_TBL *[4], ac_huff_tbl_ptrs, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(UINT8 [16], arith_dc_L, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(UINT8 [16], arith_dc_U, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(UINT8 [16], arith_ac_K, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, num_scans, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(const jpeg_scan_info *, scan_info, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, raw_data_in, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, arith_code, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, optimize_coding, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, CCIR601_sampling, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, smoothing_factor, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(J_DCT_METHOD, dct_method, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, restart_interval, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, restart_in_rows, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, write_JFIF_header, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, JFIF_major_version, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, JFIF_minor_version, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, density_unit, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, X_density, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, Y_density, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, write_Adobe_marker, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, next_scanline, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, progressive_mode, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, max_h_samp_factor, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, max_v_samp_factor, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, total_iMCU_rows, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, comps_in_scan, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(jpeg_component_info *[4], cur_comp_info, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, MCUs_per_row, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, MCU_rows_in_scan, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, blocks_in_MCU, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int [10], MCU_membership, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, Ss, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, Se, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, Ah, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, Al, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_comp_master *, master, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_c_main_controller *, main, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_c_prep_controller *, prep, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_c_coef_controller *, coef, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_marker_writer *, marker, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_color_converter *, cconvert, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_downsampler *, downsample, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_forward_dct *, fdct, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_entropy_encoder *, entropy, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(jpeg_scan_info *, script_space, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, script_space_size, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_destination_mgr(f, g, ...) \
	f(JOCTET *, next_output_byte, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, free_in_buffer, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(j_compress_ptr), init_destination, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(boolean (*)(j_compress_ptr), empty_output_buffer, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(j_compress_ptr), term_destination, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_decompress_struct(f, g, ...) \
	f(struct jpeg_error_mgr *, err, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_memory_mgr *, mem, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_progress_mgr *, progress, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void *, client_data, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, is_decompressor, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, global_state, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_source_mgr *, src, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, image_width, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, image_height, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, num_components, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(J_COLOR_SPACE, jpeg_color_space, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(J_COLOR_SPACE, out_color_space, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, scale_num, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, scale_denom, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(double, output_gamma, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, buffered_image, FIELD_FREEZABLE, ##__VA_ARGS__) \
	g() \
	f(int, raw_data_out, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(J_DCT_METHOD, dct_method, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, do_fancy_upsampling, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, do_block_smoothing, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, quantize_colors, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(J_DITHER_MODE, dither_mode, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, two_pass_quantize, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, desired_number_of_colors, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, enable_1pass_quant, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, enable_external_quant, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, enable_2pass_quant, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, output_width, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, output_height, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, out_color_components, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, output_components, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, rec_outbuf_height, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, actual_number_of_colors, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JSAMPROW *, colormap, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, output_scanline, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, input_scan_number, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, input_iMCU_row, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, output_scan_number, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, output_iMCU_row, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int (*)[64], coef_bits, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JQUANT_TBL *[4], quant_tbl_ptrs, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JHUFF_TBL *[4], dc_huff_tbl_ptrs, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JHUFF_TBL *[4], ac_huff_tbl_ptrs, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, data_precision, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(jpeg_component_info *, comp_info, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, progressive_mode, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, arith_code, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(UINT8 [16], arith_dc_L, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(UINT8 [16], arith_dc_U, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(UINT8 [16], arith_ac_K, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, restart_interval, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, saw_JFIF_marker, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, JFIF_major_version, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, JFIF_minor_version, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, density_unit, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, X_density, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, Y_density, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, saw_Adobe_marker, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, Adobe_transform, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, CCIR601_sampling, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_marker_struct *, marker_list, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, max_h_samp_factor, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, max_v_samp_factor, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, min_DCT_scaled_size, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, total_iMCU_rows, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JSAMPLE *, sample_range_limit, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, comps_in_scan, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(jpeg_component_info *[4], cur_comp_info, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, MCUs_per_row, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, MCU_rows_in_scan, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, blocks_in_MCU, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int [10], MCU_membership, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, Ss, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, Se, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, Ah, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, Al, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, unread_marker, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_decomp_master *, master, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_d_main_controller *, main, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_d_coef_controller *, coef, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_d_post_controller *, post, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_input_controller *, inputctl, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_marker_reader *, marker, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_entropy_decoder *, entropy, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_inverse_dct *, idct, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_upsampler *, upsample, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_color_deconverter *, cconvert, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_color_quantizer *, cquantize, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_source_mgr(f, g, ...) \
	f(const JOCTET *, next_input_byte, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned long, bytes_in_buffer, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(j_decompress_ptr), init_source, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(boolean (*)(j_decompress_ptr), fill_input_buffer, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(j_decompress_ptr, long), skip_input_data, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(boolean (*)(j_decompress_ptr, int), resync_to_restart, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(j_decompress_ptr), term_source, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_marker_struct(f, g, ...) \
	f(struct jpeg_marker_struct *, next, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, marker, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, original_length, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, data_length, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JOCTET *, data, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_jpeglib_allClasses(f, ...) \
	f(jpeg_common_struct, jpeglib, ##__VA_ARGS__) \
	f(jpeg_compress_struct, jpeglib, ##__VA_ARGS__) \
	f(jpeg_decompress_struct, jpeglib, ##__VA_ARGS__) \
	f(jpeg_destination_mgr, jpeglib, ##__VA_ARGS__) \
	f(jpeg_error_mgr, jpeglib, ##__VA_ARGS__) \
	f(jpeg_marker_struct, jpeglib, ##__VA_ARGS__) \
	f(jpeg_memory_mgr, jpeglib, ##__VA_ARGS__) \
	f(jpeg_progress_mgr, jpeglib, ##__VA_ARGS__) \
	f(jpeg_source_mgr, jpeglib, ##__VA_ARGS__)
