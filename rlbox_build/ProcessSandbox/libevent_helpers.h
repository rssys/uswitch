#pragma once

#include <event2/event.h>
#include <event2/http.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>
#include "generic_helpers.h"

#define LIB EVENT
namespace LIB {

// Callback signatures
typedef void (*t_evhttp_gencb) (struct evhttp_request *, void *);

// Must define callback types by these aliases
typedef t_evhttp_gencb CB_TYPE_0;

// unused, but we require these typedef'd to some function-pointer type (for now)
typedef void (*CB_UNUSED)();
typedef CB_UNUSED CB_TYPE_1;
typedef CB_UNUSED CB_TYPE_2;
typedef CB_UNUSED CB_TYPE_3;
typedef CB_UNUSED CB_TYPE_4;
typedef CB_UNUSED CB_TYPE_5;
typedef CB_UNUSED CB_TYPE_6;
typedef CB_UNUSED CB_TYPE_7;

};  // namespace LIB

// All types used by library functions must be a single preprocessor token, so that
//   they play nice with macros
// This means that we can't have two-word types like "struct x", nor types that contain *
typedef evhttp_uri* EVHTTP_URI_STAR;
typedef const evhttp_uri* CONST_EVHTTP_URI_STAR;
typedef evhttp_request* EVHTTP_REQUEST_STAR;
typedef const evhttp_request* CONST_EVHTTP_REQUEST_STAR;
typedef evkeyvalq* EVKEYVALQ_STAR;
typedef evbuffer* EVBUFFER_STAR;
typedef event_config* EVENT_CONFIG_STAR;
typedef const event_config* CONST_EVENT_CONFIG_STAR;
typedef event_base* EVENT_BASE_STAR;
typedef evhttp* EVHTTP_STAR;
typedef evhttp_bound_socket* EVHTTP_BOUND_SOCKET_STAR;
typedef char* CHARSTAR;
typedef const char* CONSTCHARSTAR;
typedef void* VOIDSTAR;
typedef const void* CONSTVOIDSTAR;
typedef void (*EVHTTP_GENCB) (struct evhttp_request *, void *);

// Like FOR_EACH_0ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_0ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_0ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_0ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(evbuffer_new, EVBUFFER_STAR) \
  macro(event_config_new, EVENT_CONFIG_STAR) \

// Like FOR_EACH_1ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_1ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(evhttp_uri_free, void, EVHTTP_URI_STAR) \
  macro(evbuffer_free, void, EVBUFFER_STAR) \
  macro(event_enable_debug_logging, void, uint32_t) \
  macro(event_config_free, void, EVENT_CONFIG_STAR) \

// Like FOR_EACH_1ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_1ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(evhttp_uri_parse, EVHTTP_URI_STAR, CONSTCHARSTAR) \
  macro(evhttp_uri_get_path, CONSTCHARSTAR, CONST_EVHTTP_URI_STAR) \
  macro(evhttp_request_get_uri, CONSTCHARSTAR, CONST_EVHTTP_REQUEST_STAR) \
  macro(evhttp_request_get_output_headers, EVKEYVALQ_STAR, EVHTTP_REQUEST_STAR) \
  macro(event_base_new_with_config, EVENT_BASE_STAR, CONST_EVENT_CONFIG_STAR) \
  macro(event_base_dispatch, int, EVENT_BASE_STAR) \
  macro(evhttp_new, EVHTTP_STAR, EVENT_BASE_STAR) \

// Like FOR_EACH_2ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_2ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(evhttp_set_max_body_size, void, EVHTTP_STAR, ssize_t) \

// Like FOR_EACH_2ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_2ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_3ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_3ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(evhttp_send_error, void, EVHTTP_REQUEST_STAR, int, CONSTCHARSTAR) \
  macro(evhttp_set_gencb, void, EVHTTP_STAR, EVHTTP_GENCB, VOIDSTAR) \

// Like FOR_EACH_3ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_3ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(evhttp_add_header, int, EVKEYVALQ_STAR, CONSTCHARSTAR, CONSTCHARSTAR) \
  macro(evhttp_bind_socket_with_handle, EVHTTP_BOUND_SOCKET_STAR, EVHTTP_STAR, CONSTCHARSTAR, uint16_t) \

// Like FOR_EACH_4ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_4ARG_VOID_LIBRARY_FUNCTION(macro) \
  macro(evhttp_send_reply, void, EVHTTP_REQUEST_STAR, int, CONSTCHARSTAR, EVBUFFER_STAR) \

// Like FOR_EACH_4ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_4ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_5ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_5ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_5ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_5ARG_NONVOID_LIBRARY_FUNCTION(macro) \
  macro(evbuffer_add_reference, int, EVBUFFER_STAR, CONSTVOIDSTAR, size_t, evbuffer_ref_cleanup_cb, VOIDSTAR) \

// Like FOR_EACH_6ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_6ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_6ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_6ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_7ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_7ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_7ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_7ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_8ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_8ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_8ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_8ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_9ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_9ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_9ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_9ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_10ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_10ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_10ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_10ARG_NONVOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_11ARG_LIBRARY_FUNCTION, but only void functions
#define FOR_EACH_11ARG_VOID_LIBRARY_FUNCTION(macro) \

// Like FOR_EACH_11ARG_LIBRARY_FUNCTION, but only nonvoid functions
#define FOR_EACH_11ARG_NONVOID_LIBRARY_FUNCTION(macro) \

