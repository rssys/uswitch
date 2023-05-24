//Auto generated with the modified clang - Pass the arg "-Xclang -fdump-struct-layouts-for-sbox" to clang while compiling

#define sandbox_fields_reflection_jpeglib_class_jpeg_error_mgr(f, g) \
	f(void (*)(j_common_ptr), error_exit) \
	g() \
	f(void (*)(j_common_ptr, int), emit_message) \
	g() \
	f(void (*)(j_common_ptr), output_message) \
	g() \
	f(void (*)(j_common_ptr, char *), format_message) \
	g() \
	f(void (*)(j_common_ptr), reset_error_mgr) \
	g() \
	f(int, msg_code) \
	g() \
	f(char[80], msg_parm) \
	g() \
	f(int, trace_level) \
	g() \
	f(long, num_warnings) \
	g() \
	f(const char *const *, jpeg_message_table) \
	g() \
	f(int, last_jpeg_message) \
	g() \
	f(const char *const *, addon_message_table) \
	g() \
	f(int, first_addon_message) \
	g() \
	f(int, last_addon_message) \
	g()

#define sandbox_fields_reflection_jpeglib_class_decoder_error_mgr(f, g) \
    f(struct jpeg_error_mgr, pub) \
    g() \
    f(jmp_buf, setjmp_buffer) \
    g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_common_struct(f, g) \
	f(struct jpeg_error_mgr *, err) \
	g() \
	f(struct jpeg_memory_mgr *, mem) \
	g() \
	f(struct jpeg_progress_mgr *, progress) \
	g() \
	f(void *, client_data) \
	g() \
	f(int, is_decompressor) \
	g() \
	f(int, global_state) \
	g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_memory_mgr(f, g) \
	f(void *(*)(j_common_ptr, int, size_t), alloc_small) \
	g() \
	f(void *(*)(j_common_ptr, int, size_t), alloc_large) \
	g() \
	f(JSAMPARRAY (*)(j_common_ptr, int, JDIMENSION, JDIMENSION), alloc_sarray) \
	g() \
	f(JBLOCKARRAY (*)(j_common_ptr, int, JDIMENSION, JDIMENSION), alloc_barray) \
	g() \
	f(jvirt_sarray_ptr (*)(j_common_ptr, int, boolean, JDIMENSION, JDIMENSION, JDIMENSION), request_virt_sarray) \
	g() \
	f(jvirt_barray_ptr (*)(j_common_ptr, int, boolean, JDIMENSION, JDIMENSION, JDIMENSION), request_virt_barray) \
	g() \
	f(void (*)(j_common_ptr), realize_virt_arrays) \
	g() \
	f(JSAMPARRAY (*)(j_common_ptr, jvirt_sarray_ptr, JDIMENSION, JDIMENSION, boolean), access_virt_sarray) \
	g() \
	f(JBLOCKARRAY (*)(j_common_ptr, jvirt_barray_ptr, JDIMENSION, JDIMENSION, boolean), access_virt_barray) \
	g() \
	f(void (*)(j_common_ptr, int), free_pool) \
	g() \
	f(void (*)(j_common_ptr), self_destruct) \
	g() \
	f(long, max_memory_to_use) \
	g() \
	f(long, max_alloc_chunk) \
	g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_progress_mgr(f, g) \
	f(void (*)(j_common_ptr), progress_monitor) \
	g() \
	f(long, pass_counter) \
	g() \
	f(long, pass_limit) \
	g() \
	f(int, completed_passes) \
	g() \
	f(int, total_passes) \
	g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_compress_struct(f, g) \
	f(struct jpeg_error_mgr *, err) \
	g() \
	f(struct jpeg_memory_mgr *, mem) \
	g() \
	f(struct jpeg_progress_mgr *, progress) \
	g() \
	f(void *, client_data) \
	g() \
	f(int, is_decompressor) \
	g() \
	f(int, global_state) \
	g() \
	f(struct jpeg_destination_mgr *, dest) \
	g() \
	f(unsigned int, image_width) \
	g() \
	f(unsigned int, image_height) \
	g() \
	f(int, input_components) \
	g() \
	f(J_COLOR_SPACE, in_color_space) \
	g() \
	f(double, input_gamma) \
	g() \
	f(int, data_precision) \
	g() \
	f(int, num_components) \
	g() \
	f(J_COLOR_SPACE, jpeg_color_space) \
	g() \
	f(jpeg_component_info *, comp_info) \
	g() \
	f(JQUANT_TBL *[4], quant_tbl_ptrs) \
	g() \
	f(JHUFF_TBL *[4], dc_huff_tbl_ptrs) \
	g() \
	f(JHUFF_TBL *[4], ac_huff_tbl_ptrs) \
	g() \
	f(UINT8 [16], arith_dc_L) \
	g() \
	f(UINT8 [16], arith_dc_U) \
	g() \
	f(UINT8 [16], arith_ac_K) \
	g() \
	f(int, num_scans) \
	g() \
	f(const jpeg_scan_info *, scan_info) \
	g() \
	f(int, raw_data_in) \
	g() \
	f(int, arith_code) \
	g() \
	f(int, optimize_coding) \
	g() \
	f(int, CCIR601_sampling) \
	g() \
	f(int, smoothing_factor) \
	g() \
	f(J_DCT_METHOD, dct_method) \
	g() \
	f(unsigned int, restart_interval) \
	g() \
	f(int, restart_in_rows) \
	g() \
	f(int, write_JFIF_header) \
	g() \
	f(unsigned char, JFIF_major_version) \
	g() \
	f(unsigned char, JFIF_minor_version) \
	g() \
	f(unsigned char, density_unit) \
	g() \
	f(unsigned short, X_density) \
	g() \
	f(unsigned short, Y_density) \
	g() \
	f(int, write_Adobe_marker) \
	g() \
	f(unsigned int, next_scanline) \
	g() \
	f(int, progressive_mode) \
	g() \
	f(int, max_h_samp_factor) \
	g() \
	f(int, max_v_samp_factor) \
	g() \
	f(unsigned int, total_iMCU_rows) \
	g() \
	f(int, comps_in_scan) \
	g() \
	f(jpeg_component_info *[4], cur_comp_info) \
	g() \
	f(unsigned int, MCUs_per_row) \
	g() \
	f(unsigned int, MCU_rows_in_scan) \
	g() \
	f(int, blocks_in_MCU) \
	g() \
	f(int [10], MCU_membership) \
	g() \
	f(int, Ss) \
	g() \
	f(int, Se) \
	g() \
	f(int, Ah) \
	g() \
	f(int, Al) \
	g() \
	f(struct jpeg_comp_master *, master) \
	g() \
	f(struct jpeg_c_main_controller *, main) \
	g() \
	f(struct jpeg_c_prep_controller *, prep) \
	g() \
	f(struct jpeg_c_coef_controller *, coef) \
	g() \
	f(struct jpeg_marker_writer *, marker) \
	g() \
	f(struct jpeg_color_converter *, cconvert) \
	g() \
	f(struct jpeg_downsampler *, downsample) \
	g() \
	f(struct jpeg_forward_dct *, fdct) \
	g() \
	f(struct jpeg_entropy_encoder *, entropy) \
	g() \
	f(jpeg_scan_info *, script_space) \
	g() \
	f(int, script_space_size) \
	g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_destination_mgr(f, g) \
	f(JOCTET *, next_output_byte) \
	g() \
	f(unsigned long, free_in_buffer) \
	g() \
	f(void (*)(j_compress_ptr), init_destination) \
	g() \
	f(boolean (*)(j_compress_ptr), empty_output_buffer) \
	g() \
	f(void (*)(j_compress_ptr), term_destination) \
	g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_decompress_struct(f, g) \
	f(struct jpeg_error_mgr *, err) \
	g() \
	f(struct jpeg_memory_mgr *, mem) \
	g() \
	f(struct jpeg_progress_mgr *, progress) \
	g() \
	f(void *, client_data) \
	g() \
	f(int, is_decompressor) \
	g() \
	f(int, global_state) \
	g() \
	f(struct jpeg_source_mgr *, src) \
	g() \
	f(unsigned int, image_width) \
	g() \
	f(unsigned int, image_height) \
	g() \
	f(int, num_components) \
	g() \
	f(J_COLOR_SPACE, jpeg_color_space) \
	g() \
	f(J_COLOR_SPACE, out_color_space) \
	g() \
	f(unsigned int, scale_num) \
	g() \
	f(unsigned int, scale_denom) \
	g() \
	f(double, output_gamma) \
	g() \
	f(int, buffered_image) \
	g() \
	f(int, raw_data_out) \
	g() \
	f(J_DCT_METHOD, dct_method) \
	g() \
	f(int, do_fancy_upsampling) \
	g() \
	f(int, do_block_smoothing) \
	g() \
	f(int, quantize_colors) \
	g() \
	f(J_DITHER_MODE, dither_mode) \
	g() \
	f(int, two_pass_quantize) \
	g() \
	f(int, desired_number_of_colors) \
	g() \
	f(int, enable_1pass_quant) \
	g() \
	f(int, enable_external_quant) \
	g() \
	f(int, enable_2pass_quant) \
	g() \
	f(unsigned int, output_width) \
	g() \
	f(unsigned int, output_height) \
	g() \
	f(int, out_color_components) \
	g() \
	f(int, output_components) \
	g() \
	f(int, rec_outbuf_height) \
	g() \
	f(int, actual_number_of_colors) \
	g() \
	f(JSAMPROW *, colormap) \
	g() \
	f(unsigned int, output_scanline) \
	g() \
	f(int, input_scan_number) \
	g() \
	f(unsigned int, input_iMCU_row) \
	g() \
	f(int, output_scan_number) \
	g() \
	f(unsigned int, output_iMCU_row) \
	g() \
	f(int (*)[64], coef_bits) \
	g() \
	f(JQUANT_TBL *[4], quant_tbl_ptrs) \
	g() \
	f(JHUFF_TBL *[4], dc_huff_tbl_ptrs) \
	g() \
	f(JHUFF_TBL *[4], ac_huff_tbl_ptrs) \
	g() \
	f(int, data_precision) \
	g() \
	f(jpeg_component_info *, comp_info) \
	g() \
	f(int, progressive_mode) \
	g() \
	f(int, arith_code) \
	g() \
	f(UINT8 [16], arith_dc_L) \
	g() \
	f(UINT8 [16], arith_dc_U) \
	g() \
	f(UINT8 [16], arith_ac_K) \
	g() \
	f(unsigned int, restart_interval) \
	g() \
	f(int, saw_JFIF_marker) \
	g() \
	f(unsigned char, JFIF_major_version) \
	g() \
	f(unsigned char, JFIF_minor_version) \
	g() \
	f(unsigned char, density_unit) \
	g() \
	f(unsigned short, X_density) \
	g() \
	f(unsigned short, Y_density) \
	g() \
	f(int, saw_Adobe_marker) \
	g() \
	f(unsigned char, Adobe_transform) \
	g() \
	f(int, CCIR601_sampling) \
	g() \
	f(struct jpeg_marker_struct *, marker_list) \
	g() \
	f(int, max_h_samp_factor) \
	g() \
	f(int, max_v_samp_factor) \
	g() \
	f(int, min_DCT_scaled_size) \
	g() \
	f(unsigned int, total_iMCU_rows) \
	g() \
	f(JSAMPLE *, sample_range_limit) \
	g() \
	f(int, comps_in_scan) \
	g() \
	f(jpeg_component_info *[4], cur_comp_info) \
	g() \
	f(unsigned int, MCUs_per_row) \
	g() \
	f(unsigned int, MCU_rows_in_scan) \
	g() \
	f(int, blocks_in_MCU) \
	g() \
	f(int [10], MCU_membership) \
	g() \
	f(int, Ss) \
	g() \
	f(int, Se) \
	g() \
	f(int, Ah) \
	g() \
	f(int, Al) \
	g() \
	f(int, unread_marker) \
	g() \
	f(struct jpeg_decomp_master *, master) \
	g() \
	f(struct jpeg_d_main_controller *, main) \
	g() \
	f(struct jpeg_d_coef_controller *, coef) \
	g() \
	f(struct jpeg_d_post_controller *, post) \
	g() \
	f(struct jpeg_input_controller *, inputctl) \
	g() \
	f(struct jpeg_marker_reader *, marker) \
	g() \
	f(struct jpeg_entropy_decoder *, entropy) \
	g() \
	f(struct jpeg_inverse_dct *, idct) \
	g() \
	f(struct jpeg_upsampler *, upsample) \
	g() \
	f(struct jpeg_color_deconverter *, cconvert) \
	g() \
	f(struct jpeg_color_quantizer *, cquantize) \
	g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_source_mgr(f, g) \
	f(const JOCTET *, next_input_byte) \
	g() \
	f(unsigned long, bytes_in_buffer) \
	g() \
	f(void (*)(j_decompress_ptr), init_source) \
	g() \
	f(boolean (*)(j_decompress_ptr), fill_input_buffer) \
	g() \
	f(void (*)(j_decompress_ptr, long), skip_input_data) \
	g() \
	f(boolean (*)(j_decompress_ptr, int), resync_to_restart) \
	g() \
	f(void (*)(j_decompress_ptr), term_source) \
	g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_marker_struct(f, g) \
	f(struct jpeg_marker_struct *, next) \
	g() \
	f(unsigned char, marker) \
	g() \
	f(unsigned int, original_length) \
	g() \
	f(unsigned int, data_length) \
	g() \
	f(JOCTET *, data) \
	g()

#define sandbox_fields_reflection_jpeglib_allClasses(f) \
	f(jpeg_common_struct, jpeglib) \
	f(jpeg_compress_struct, jpeglib) \
	f(jpeg_decompress_struct, jpeglib) \
	f(jpeg_destination_mgr, jpeglib) \
	f(jpeg_error_mgr, jpeglib) \
	f(jpeg_marker_struct, jpeglib) \
	f(jpeg_memory_mgr, jpeglib) \
	f(jpeg_progress_mgr, jpeglib) \
	f(jpeg_source_mgr, jpeglib) \
	f(decoder_error_mgr, jpeglib)
