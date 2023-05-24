// Macros used by other *_helpers.h files, as well as ProcessSandbox
// These macros are non-library-specific

// Expects a macro that itself expects:
//   - function name
//   - return type
//   - additional arguments: type of each parameter
#define FOR_EACH_LIBRARY_FUNCTION(macro) \
  macro(invokeDlSym, void*, const char*) \
  FOR_EACH_0ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_1ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_2ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_3ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_4ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_5ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_6ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_7ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_8ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_9ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_10ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_11ARG_LIBRARY_FUNCTION(macro) \

// Expects a macro that itself expects:
//   - function name
//   - return type
//   - argument type
#define FOR_EACH_0ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_0ARG_VOID_LIBRARY_FUNCTION(macro) \
  FOR_EACH_0ARG_NONVOID_LIBRARY_FUNCTION(macro)

// Expects a macro that itself expects:
//   - function name
//   - return type
//   - argument type
#define FOR_EACH_1ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_1ARG_VOID_LIBRARY_FUNCTION(macro) \
  FOR_EACH_1ARG_NONVOID_LIBRARY_FUNCTION(macro)

// Expects a macro that itself expects:
//   - function name
//   - return type
//   - argument 1 type
//   - argument 2 type
#define FOR_EACH_2ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_2ARG_VOID_LIBRARY_FUNCTION(macro) \
  FOR_EACH_2ARG_NONVOID_LIBRARY_FUNCTION(macro)

// Expects a macro that itself expects:
//   - function name
//   - return type
//   - argument 1 type
//   - argument 2 type
//   - argument 3 type
#define FOR_EACH_3ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_3ARG_VOID_LIBRARY_FUNCTION(macro) \
  FOR_EACH_3ARG_NONVOID_LIBRARY_FUNCTION(macro)

// Expects a macro that itself expects:
//   - function name
//   - return type
//   - argument 1 type
//   - argument 2 type
//   - argument 3 type
//   - argument 4 type
#define FOR_EACH_4ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_4ARG_VOID_LIBRARY_FUNCTION(macro) \
  FOR_EACH_4ARG_NONVOID_LIBRARY_FUNCTION(macro)

// Expects a macro that itself expects:
//   - function name
//   - return type
//   - argument 1 type
//   - argument 2 type
//   - argument 3 type
//   - argument 4 type
//   - argument 5 type
#define FOR_EACH_5ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_5ARG_VOID_LIBRARY_FUNCTION(macro) \
  FOR_EACH_5ARG_NONVOID_LIBRARY_FUNCTION(macro)

// Expects a macro that itself expects:
//   - function name
//   - return type
//   - argument 1 type
//   - argument 2 type
//   - argument 3 type
//   - argument 4 type
//   - argument 5 type
//   - argument 6 type
#define FOR_EACH_6ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_6ARG_VOID_LIBRARY_FUNCTION(macro) \
  FOR_EACH_6ARG_NONVOID_LIBRARY_FUNCTION(macro)

// Expects a macro that itself expects:
//   - function name
//   - return type
//   - argument 1 type
//   - argument 2 type
//   - argument 3 type
//   - argument 4 type
//   - argument 5 type
//   - argument 6 type
//   - argument 7 type
#define FOR_EACH_7ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_7ARG_VOID_LIBRARY_FUNCTION(macro) \
  FOR_EACH_7ARG_NONVOID_LIBRARY_FUNCTION(macro)

// Expects a macro that itself expects:
//   - function name
//   - return type
//   - argument 1 type
//   - argument 2 type
//   - argument 3 type
//   - argument 4 type
//   - argument 5 type
//   - argument 6 type
//   - argument 7 type
//   - argument 8 type
#define FOR_EACH_8ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_8ARG_VOID_LIBRARY_FUNCTION(macro) \
  FOR_EACH_8ARG_NONVOID_LIBRARY_FUNCTION(macro)

// Expects a macro that itself expects:
//   - function name
//   - return type
//   - argument 1 type
//   - argument 2 type
//   - argument 3 type
//   - argument 4 type
//   - argument 5 type
//   - argument 6 type
//   - argument 7 type
//   - argument 8 type
//   - argument 9 type
#define FOR_EACH_9ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_9ARG_VOID_LIBRARY_FUNCTION(macro) \
  FOR_EACH_9ARG_NONVOID_LIBRARY_FUNCTION(macro)

// Expects a macro that itself expects:
//   - function name
//   - return type
//   - argument 1 type
//   - argument 2 type
//   - argument 3 type
//   - argument 4 type
//   - argument 5 type
//   - argument 6 type
//   - argument 7 type
//   - argument 8 type
//   - argument 9 type
//   - argument 10 type
#define FOR_EACH_10ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_10ARG_VOID_LIBRARY_FUNCTION(macro) \
  FOR_EACH_10ARG_NONVOID_LIBRARY_FUNCTION(macro)

// Expects a macro that itself expects:
//   - function name
//   - return type
//   - argument 1 type
//   - argument 2 type
//   - argument 3 type
//   - argument 4 type
//   - argument 5 type
//   - argument 6 type
//   - argument 7 type
//   - argument 8 type
//   - argument 9 type
//   - argument 10 type
//   - argument 11 type
#define FOR_EACH_11ARG_LIBRARY_FUNCTION(macro) \
  FOR_EACH_11ARG_VOID_LIBRARY_FUNCTION(macro) \
  FOR_EACH_11ARG_NONVOID_LIBRARY_FUNCTION(macro)
