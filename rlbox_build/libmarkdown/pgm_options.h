#ifndef PGM_OPTIONS_D
#define PGM_OPTIONS_D

#include "mkdio.h"

#ifdef __cplusplus
extern "C" {
#endif

char *set_flag(mkd_flag_t *flags, char *optionstring);
void show_flags(int byname, int verbose);

#ifdef __cplusplus
}
#endif


#endif/*PGM_OPTIONS_D*/
