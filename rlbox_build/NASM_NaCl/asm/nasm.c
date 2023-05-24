/* ----------------------------------------------------------------------- *
 *
 *   Copyright 1996-2017 The NASM Authors - All Rights Reserved
 *   See the file AUTHORS included with the NASM distribution for
 *   the specific copyright holders.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following
 *   conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *     CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *     INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *     MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *     CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *     SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *     NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *     HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *     OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *     EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ----------------------------------------------------------------------- */

/*
 * The Netwide Assembler main program module
 */

#include "compiler.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>

#include "nasm.h"
#include "nasmlib.h"
#include "error.h"
#include "saa.h"
#include "raa.h"
#include "float.h"
#include "stdscan.h"
#include "insns.h"
#include "preproc.h"
#include "parser.h"
#include "eval.h"
#include "assemble.h"
#include "labels.h"
#include "outform.h"
#include "listing.h"
#include "iflag.h"
#include "ver.h"

/*
 * This is the maximum number of optimization passes to do.  If we ever
 * find a case where the optimizer doesn't naturally converge, we might
 * have to drop this value so the assembler doesn't appear to just hang.
 */
#define MAX_OPTIMIZE (INT_MAX >> 1)

struct forwrefinfo {            /* info held on forward refs. */
    int lineno;
    int operand;
};

static void parse_cmdline(int, char **, int);
static void assemble_file(char *, StrList **);
static bool is_suppressed_warning(int severity);
static bool skip_this_pass(int severity);
static void nasm_verror_gnu(int severity, const char *fmt, va_list args);
static void nasm_verror_vc(int severity, const char *fmt, va_list args);
static void nasm_verror_common(int severity, const char *fmt, va_list args);
static void usage(void);

static bool using_debug_info, opt_verbose_info;
static const char *debug_format;

bool tasm_compatible_mode = false;
bool nacl_mode = false;
int pass0, passn;
static int pass1, pass2;	/* XXX: Get rid of these, they are redundant */
int globalrel = 0;
int globalbnd = 0;

static time_t official_compile_time;

static char inname[FILENAME_MAX];
static char outname[FILENAME_MAX];
static char listname[FILENAME_MAX];
static char errname[FILENAME_MAX];
static int globallineno;        /* for forward-reference tracking */
/* static int pass = 0; */
const struct ofmt *ofmt = &OF_DEFAULT;
const struct ofmt_alias *ofmt_alias = NULL;
const struct dfmt *dfmt;

static FILE *error_file;        /* Where to write error messages */

FILE *ofile = NULL;
int optimizing = MAX_OPTIMIZE; /* number of optimization passes to take */
static int cmd_sb = 16;    /* by default */

iflag_t cpu;
static iflag_t cmd_cpu;

struct location location;
bool in_absolute;                 /* Flag we are in ABSOLUTE seg */
struct location absolute;         /* Segment/offset inside ABSOLUTE */

static struct RAA *offsets;

static struct SAA *forwrefs;    /* keep track of forward references */
static const struct forwrefinfo *forwref;

static const struct preproc_ops *preproc;

#define OP_NORMAL           (1u << 0)
#define OP_PREPROCESS       (1u << 1)
#define OP_DEPEND           (1u << 2)

static unsigned int operating_mode;

/* Dependency flags */
static bool depend_emit_phony = false;
static bool depend_missing_ok = false;
static const char *depend_target = NULL;
static const char *depend_file = NULL;

static bool want_usage;
static bool terminate_after_phase;
bool user_nolist = false;

static char *quote_for_make(const char *str);

static int64_t get_curr_offs(void)
{
    return in_absolute ? absolute.offset : raa_read(offsets, location.segment);
}

static void set_curr_offs(int64_t l_off)
{
        if (in_absolute)
            absolute.offset = l_off;
        else
            offsets = raa_write(offsets, location.segment, l_off);
}

static void nasm_fputs(const char *line, FILE * outfile)
{
    if (outfile) {
        fputs(line, outfile);
        putc('\n', outfile);
    } else
        puts(line);
}

/* Convert a struct tm to a POSIX-style time constant */
static int64_t make_posix_time(struct tm *tm)
{
    int64_t t;
    int64_t y = tm->tm_year;

    /* See IEEE 1003.1:2004, section 4.14 */

    t = (y-70)*365 + (y-69)/4 - (y-1)/100 + (y+299)/400;
    t += tm->tm_yday;
    t *= 24;
    t += tm->tm_hour;
    t *= 60;
    t += tm->tm_min;
    t *= 60;
    t += tm->tm_sec;

    return t;
}

static void define_macros_early(void)
{
    char temp[128];
    struct tm lt, *lt_p, gm, *gm_p;
    int64_t posix_time;

    lt_p = localtime(&official_compile_time);
    if (lt_p) {
        lt = *lt_p;

        strftime(temp, sizeof temp, "__DATE__=\"%Y-%m-%d\"", &lt);
        preproc->pre_define(temp);
        strftime(temp, sizeof temp, "__DATE_NUM__=%Y%m%d", &lt);
        preproc->pre_define(temp);
        strftime(temp, sizeof temp, "__TIME__=\"%H:%M:%S\"", &lt);
        preproc->pre_define(temp);
        strftime(temp, sizeof temp, "__TIME_NUM__=%H%M%S", &lt);
        preproc->pre_define(temp);
    }

    gm_p = gmtime(&official_compile_time);
    if (gm_p) {
        gm = *gm_p;

        strftime(temp, sizeof temp, "__UTC_DATE__=\"%Y-%m-%d\"", &gm);
        preproc->pre_define(temp);
        strftime(temp, sizeof temp, "__UTC_DATE_NUM__=%Y%m%d", &gm);
        preproc->pre_define(temp);
        strftime(temp, sizeof temp, "__UTC_TIME__=\"%H:%M:%S\"", &gm);
        preproc->pre_define(temp);
        strftime(temp, sizeof temp, "__UTC_TIME_NUM__=%H%M%S", &gm);
        preproc->pre_define(temp);
    }

    if (gm_p)
        posix_time = make_posix_time(&gm);
    else if (lt_p)
        posix_time = make_posix_time(&lt);
    else
        posix_time = 0;

    if (posix_time) {
        snprintf(temp, sizeof temp, "__POSIX_TIME__=%"PRId64, posix_time);
        preproc->pre_define(temp);
    }
}

static void define_macros_late(void)
{
    char temp[128];

    /*
     * In case if output format is defined by alias
     * we have to put shortname of the alias itself here
     * otherwise ABI backward compatibility gets broken.
     */
    snprintf(temp, sizeof(temp), "__OUTPUT_FORMAT__=%s",
             ofmt_alias ? ofmt_alias->shortname : ofmt->shortname);
    preproc->pre_define(temp);
}

static void emit_dependencies(StrList *list)
{
    FILE *deps;
    int linepos, len;
    StrList *l, *nl;

    if (depend_file && strcmp(depend_file, "-")) {
        deps = nasm_open_write(depend_file, NF_TEXT);
        if (!deps) {
            nasm_error(ERR_NONFATAL|ERR_NOFILE|ERR_USAGE,
                       "unable to write dependency file `%s'", depend_file);
            return;
        }
    } else {
        deps = stdout;
    }

    linepos = fprintf(deps, "%s:", depend_target);
    list_for_each(l, list) {
        char *file = quote_for_make(l->str);
        len = strlen(file);
        if (linepos + len > 62 && linepos > 1) {
            fprintf(deps, " \\\n ");
            linepos = 1;
        }
        fprintf(deps, " %s", file);
        linepos += len+1;
        nasm_free(file);
    }
    fprintf(deps, "\n\n");

    list_for_each_safe(l, nl, list) {
        if (depend_emit_phony)
            fprintf(deps, "%s:\n\n", l->str);
        nasm_free(l);
    }

    if (deps != stdout)
        fclose(deps);
}

int main(int argc, char **argv)
{
    StrList *depend_list = NULL, **depend_ptr;

    time(&official_compile_time);

    iflag_set(&cpu, IF_PLEVEL);
    iflag_set(&cmd_cpu, IF_PLEVEL);

    pass0 = 0;
    want_usage = terminate_after_phase = false;
    nasm_set_verror(nasm_verror_gnu);

    error_file = stderr;

    tolower_init();
    src_init();

    offsets = raa_init();
    forwrefs = saa_init((int32_t)sizeof(struct forwrefinfo));

    preproc = &nasmpp;
    operating_mode = OP_NORMAL;

    parse_cmdline(argc, argv, 1);
    if (terminate_after_phase) {
        if (want_usage)
            usage();
        return 1;
    }

    /*
     * Define some macros dependent on the runtime, but not
     * on the command line (as those are scanned in cmdline pass 2.)
     */
    preproc->init();
    define_macros_early();

    parse_cmdline(argc, argv, 2);
    if (terminate_after_phase) {
        if (want_usage)
            usage();
        return 1;
    }

    /* Save away the default state of warnings */
    memcpy(warning_state_init, warning_state, sizeof warning_state);

    if (!using_debug_info) {
        /* No debug info, redirect to the null backend (empty stubs) */
        dfmt = &null_debug_form;
    } else if (!debug_format) {
        /* Default debug format for this backend */
	dfmt = ofmt->default_dfmt;
    } else {
        dfmt = dfmt_find(ofmt, debug_format);
        if (!dfmt) {
            nasm_fatal(ERR_NOFILE | ERR_USAGE,
                       "unrecognized debug format `%s' for"
                       " output format `%s'",
                       debug_format, ofmt->shortname);
        }
    }

    if (ofmt->stdmac)
        preproc->extra_stdmac(ofmt->stdmac);

    /* define some macros dependent of command-line */
    define_macros_late();

    depend_ptr = (depend_file || (operating_mode & OP_DEPEND)) ? &depend_list : NULL;
    if (!depend_target)
        depend_target = quote_for_make(outname);

    if (operating_mode & OP_DEPEND) {
            char *line;

            if (depend_missing_ok)
                preproc->include_path(NULL);    /* "assume generated" */

            preproc->reset(inname, 0, depend_ptr);
            if (outname[0] == '\0')
                ofmt->filename(inname, outname);
            ofile = NULL;
            while ((line = preproc->getline()))
                nasm_free(line);
            preproc->cleanup(0);
    } else if (operating_mode & OP_PREPROCESS) {
            char *line;
            const char *file_name = NULL;
            int32_t prior_linnum = 0;
            int lineinc = 0;

            if (*outname) {
                ofile = nasm_open_write(outname, NF_TEXT);
                if (!ofile)
                    nasm_fatal(ERR_NOFILE,
                                 "unable to open output file `%s'",
                                 outname);
            } else
                ofile = NULL;

            location.known = false;

            /* pass = 1; */
            preproc->reset(inname, 3, depend_ptr);

	    /* Revert all warnings to the default state */
	    memcpy(warning_state, warning_state_init, sizeof warning_state);

            while ((line = preproc->getline())) {
                /*
                 * We generate %line directives if needed for later programs
                 */
                int32_t linnum = prior_linnum += lineinc;
                int altline = src_get(&linnum, &file_name);
                if (altline) {
                    if (altline == 1 && lineinc == 1)
                        nasm_fputs("", ofile);
                    else {
                        lineinc = (altline != -1 || lineinc != 1);
                        fprintf(ofile ? ofile : stdout,
                                "%%line %"PRId32"+%d %s\n", linnum, lineinc,
                                file_name);
                    }
                    prior_linnum = linnum;
                }
                nasm_fputs(line, ofile);
                nasm_free(line);
            }
            preproc->cleanup(0);
            if (ofile)
                fclose(ofile);
            if (ofile && terminate_after_phase)
                remove(outname);
            ofile = NULL;
    }

    if (operating_mode & OP_NORMAL) {
        /*
         * We must call ofmt->filename _anyway_, even if the user
         * has specified their own output file, because some
         * formats (eg OBJ and COFF) use ofmt->filename to find out
         * the name of the input file and then put that inside the
         * file.
         */
        ofmt->filename(inname, outname);

        ofile = nasm_open_write(outname, (ofmt->flags & OFMT_TEXT) ? NF_TEXT : NF_BINARY);
        if (!ofile)
            nasm_fatal(ERR_NOFILE,
                       "unable to open output file `%s'", outname);

        /*
         * We must call init_labels() before ofmt->init() since
         * some object formats will want to define labels in their
         * init routines. (eg OS/2 defines the FLAT group)
         */
        init_labels();

        ofmt->init();
        dfmt->init();

        assemble_file(inname, depend_ptr);

        if (!terminate_after_phase) {
            ofmt->cleanup();
            cleanup_labels();
            fflush(ofile);
            if (ferror(ofile)) {
                nasm_error(ERR_NONFATAL|ERR_NOFILE,
                           "write error on output file `%s'", outname);
                terminate_after_phase = true;
            }
        }

        if (ofile) {
            fclose(ofile);
            if (terminate_after_phase)
                remove(outname);
            ofile = NULL;
        }
    }

    if (depend_list && !terminate_after_phase)
        emit_dependencies(depend_list);

    if (want_usage)
        usage();

    raa_free(offsets);
    saa_free(forwrefs);
    eval_cleanup();
    stdscan_cleanup();
    src_free();

    return terminate_after_phase;
}

/*
 * Get a parameter for a command line option.
 * First arg must be in the form of e.g. -f...
 */
static char *get_param(char *p, char *q, bool *advance)
{
    *advance = false;
    if (p[2]) /* the parameter's in the option */
        return nasm_skip_spaces(p + 2);
    if (q && q[0]) {
        *advance = true;
        return q;
    }
    nasm_error(ERR_NONFATAL | ERR_NOFILE | ERR_USAGE,
                 "option `-%c' requires an argument", p[1]);
    return NULL;
}

/*
 * Copy a filename
 */
static void copy_filename(char *dst, const char *src)
{
    size_t len = strlen(src);

    if (len >= (size_t)FILENAME_MAX) {
        nasm_fatal(ERR_NOFILE, "file name too long");
        return;
    }
    strncpy(dst, src, FILENAME_MAX);
}

/*
 * Convert a string to Make-safe form
 */
static char *quote_for_make(const char *str)
{
    const char *p;
    char *os, *q;

    size_t n = 1; /* Terminating zero */
    size_t nbs = 0;

    if (!str)
        return NULL;

    for (p = str; *p; p++) {
        switch (*p) {
        case ' ':
        case '\t':
            /* Convert N backslashes + ws -> 2N+1 backslashes + ws */
            n += nbs + 2;
            nbs = 0;
            break;
        case '$':
        case '#':
            nbs = 0;
            n += 2;
            break;
        case '\\':
            nbs++;
            n++;
            break;
        default:
            nbs = 0;
            n++;
            break;
        }
    }

    /* Convert N backslashes at the end of filename to 2N backslashes */
    if (nbs)
        n += nbs;

    os = q = nasm_malloc(n);

    nbs = 0;
    for (p = str; *p; p++) {
        switch (*p) {
        case ' ':
        case '\t':
            while (nbs--)
                *q++ = '\\';
            *q++ = '\\';
            *q++ = *p;
            break;
        case '$':
            *q++ = *p;
            *q++ = *p;
            nbs = 0;
            break;
        case '#':
            *q++ = '\\';
            *q++ = *p;
            nbs = 0;
            break;
        case '\\':
            *q++ = *p;
            nbs++;
            break;
        default:
            *q++ = *p;
            nbs = 0;
            break;
        }
    }
    while (nbs--)
        *q++ = '\\';

    *q = '\0';

    return os;
}

struct textargs {
    const char *label;
    int value;
};

enum text_options {
    OPT_PREFIX,
    OPT_POSTFIX
};
static const struct textargs textopts[] = {
    {"prefix", OPT_PREFIX},
    {"postfix", OPT_POSTFIX},
    {NULL, 0}
};

static void show_version(void)
{
    printf("NASM version %s compiled on %s%s\n",
           nasm_version, nasm_date, nasm_compile_options);
    exit(0);
}

static bool stopoptions = false;
static bool process_arg(char *p, char *q, int pass)
{
    char *param;
    int i;
    bool advance = false;

    if (!p || !p[0])
        return false;

    if (p[0] == '-' && !stopoptions) {
        if (strchr("oOfpPdDiIlFXuUZwW", p[1])) {
            /* These parameters take values */
            if (!(param = get_param(p, q, &advance)))
                return advance;
        }

        switch (p[1]) {
        case 's':
            if (pass == 1)
                error_file = stdout;
            break;

        case 'o':       /* output file */
            if (pass == 2)
                copy_filename(outname, param);
            break;

        case 'f':       /* output format */
            if (pass == 1) {
                ofmt = ofmt_find(param, &ofmt_alias);
                if (!ofmt) {
                    nasm_fatal(ERR_NOFILE | ERR_USAGE,
                               "unrecognised output format `%s' - "
                               "use -hf for a list", param);
                }
            }
            break;

        case 'O':       /* Optimization level */
            if (pass == 2) {
                int opt;

                if (!*param) {
                    /* Naked -O == -Ox */
                    optimizing = MAX_OPTIMIZE;
                } else {
                    while (*param) {
                        switch (*param) {
                        case '0': case '1': case '2': case '3': case '4':
                        case '5': case '6': case '7': case '8': case '9':
                            opt = strtoul(param, &param, 10);

                            /* -O0 -> optimizing == -1, 0.98 behaviour */
                            /* -O1 -> optimizing == 0, 0.98.09 behaviour */
                            if (opt < 2)
                                optimizing = opt - 1;
                            else
                                optimizing = opt;
                            break;

                        case 'v':
                        case '+':
                        param++;
                        opt_verbose_info = true;
                        break;

                        case 'x':
                            param++;
                            optimizing = MAX_OPTIMIZE;
                            break;

                        default:
                            nasm_fatal(0,
                                       "unknown optimization option -O%c\n",
                                       *param);
                            break;
                        }
                    }
                    if (optimizing > MAX_OPTIMIZE)
                        optimizing = MAX_OPTIMIZE;
                }
            }
            break;

        case 'p':       /* pre-include */
        case 'P':
            if (pass == 2)
                preproc->pre_include(param);
            break;

        case 'd':       /* pre-define */
        case 'D':
            if (pass == 2)
                preproc->pre_define(param);
            break;

        case 'u':       /* un-define */
        case 'U':
            if (pass == 2)
                preproc->pre_undefine(param);
            break;

        case 'i':       /* include search path */
        case 'I':
            if (pass == 2)
                preproc->include_path(param);
            break;

        case 'l':       /* listing file */
            if (pass == 2)
                copy_filename(listname, param);
            break;

        case 'Z':       /* error messages file */
            if (pass == 1)
                copy_filename(errname, param);
            break;

        case 'F':       /* specify debug format */
            if (pass == 2) {
                using_debug_info = true;
                debug_format = param;
            }
            break;

        case 'X':       /* specify error reporting format */
            if (pass == 1) {
                if (nasm_stricmp("vc", param) == 0)
                    nasm_set_verror(nasm_verror_vc);
                else if (nasm_stricmp("gnu", param) == 0)
                    nasm_set_verror(nasm_verror_gnu);
                else
                    nasm_fatal(ERR_NOFILE | ERR_USAGE,
                               "unrecognized error reporting format `%s'",
                               param);
            }
            break;

        case 'g':
            if (pass == 2) {
                using_debug_info = true;
                if (p[2])
                    debug_format = nasm_skip_spaces(p + 2);
            }
            break;

        case 'h':
            printf
                ("usage: nasm [-@ response file] [-o outfile] [-f format] "
                 "[-l listfile]\n"
                 "            [options...] [--] filename\n"
                 "    or nasm -v (or --v) for version info\n\n"
                 "    -t          assemble in SciTech TASM compatible mode\n"
                 "    -nacl       assemble in NACL compliant mode\n");
            printf
                ("    -E (or -e)  preprocess only (writes output to stdout by default)\n"
                 "    -a          don't preprocess (assemble only)\n"
                 "    -M          generate Makefile dependencies on stdout\n"
                 "    -MG         d:o, missing files assumed generated\n"
                 "    -MF <file>  set Makefile dependency file\n"
                 "    -MD <file>  assemble and generate dependencies\n"
                 "    -MT <file>  dependency target name\n"
                 "    -MQ <file>  dependency target name (quoted)\n"
                 "    -MP         emit phony target\n\n"
                 "    -Z<file>    redirect error messages to file\n"
                 "    -s          redirect error messages to stdout\n\n"
                 "    -g          generate debugging information\n\n"
                 "    -F format   select a debugging format\n\n"
                 "    -gformat    same as -g -F format\n\n"
                 "    -o outfile  write output to an outfile\n\n"
                 "    -f format   select an output format\n\n"
                 "    -l listfile write listing to a listfile\n\n"
                 "    -I<path>    adds a pathname to the include file path\n");
            printf
                ("    -O<digit>   optimize branch offsets\n"
                 "                -O0: No optimization\n"
                 "                -O1: Minimal optimization\n"
                 "                -Ox: Multipass optimization (default)\n\n"
                 "    -P<file>    pre-includes a file\n"
                 "    -D<macro>[=<value>] pre-defines a macro\n"
                 "    -U<macro>   undefines a macro\n"
                 "    -X<format>  specifies error reporting format (gnu or vc)\n"
                 "    -w+foo      enables warning foo (equiv. -Wfoo)\n"
                 "    -w-foo      disable warning foo (equiv. -Wno-foo)\n\n"
                 "    -w[+-]error[=foo] can be used to promote warnings to errors\n"
                "    -h           show invocation summary and exit\n\n"
                 "--prefix,--postfix\n"
                 "                these options prepend or append the given string\n"
                 "                to all extern and global variables\n"
		 "\n"
		 "Response files should contain command line parameters,\n"
                 "one per line.\n"
		 "\n"
                 "Warnings for the -W/-w options:\n");
            for (i = 0; i <= ERR_WARN_ALL; i++)
                printf("    %-23s %s%s\n",
                       warnings[i].name, warnings[i].help,
		       i == ERR_WARN_ALL ? "\n" :
                       warnings[i].enabled ? " (default on)" :
		       " (default off)");
            if (p[2] == 'f') {
                printf("valid output formats for -f are"
                       " (`*' denotes default):\n");
                ofmt_list(ofmt, stdout);
            } else {
                printf("For a list of valid output formats, use -hf.\n");
                printf("For a list of debug formats, use -f <form> -y.\n");
            }
            exit(0);    /* never need usage message here */
            break;

        case 'y':
            printf("\nvalid debug formats for '%s' output format are"
                   " ('*' denotes default):\n", ofmt->shortname);
            dfmt_list(ofmt, stdout);
            exit(0);
            break;

        case 't':
            if (pass == 2)
                tasm_compatible_mode = true;
            break;

        case 'n':
        	if(strcmp(p, "-nacl") == 0)
        		nacl_mode = true;
        	else
                nasm_error(ERR_NONFATAL | ERR_NOFILE | ERR_USAGE,
                           "unrecognised option `-%c'", p[1]);
        	break;

        case 'v':
            show_version();
            break;

        case 'e':       /* preprocess only */
        case 'E':
            if (pass == 1)
                operating_mode = OP_PREPROCESS;
            break;

        case 'a':       /* assemble only - don't preprocess */
            if (pass == 1)
                preproc = &preproc_nop;
            break;

        case 'w':
        case 'W':
            if (pass == 2) {
                if (!set_warning_status(param)) {
                    nasm_error(ERR_WARNING|ERR_NOFILE|ERR_WARN_UNK_WARNING,
			       "unknown warning option: %s", param);
                }
            }
        break;

        case 'M':
            if (pass == 2) {
                switch (p[2]) {
                case 0:
                    operating_mode = OP_DEPEND;
                    break;
                case 'G':
                    operating_mode = OP_DEPEND;
                    depend_missing_ok = true;
                    break;
                case 'P':
                    depend_emit_phony = true;
                    break;
                case 'D':
                    operating_mode = OP_NORMAL;
                    depend_file = q;
                    advance = true;
                    break;
                case 'F':
                    depend_file = q;
                    advance = true;
                    break;
                case 'T':
                    depend_target = q;
                    advance = true;
                    break;
                case 'Q':
                    depend_target = quote_for_make(q);
                    advance = true;
                    break;
                default:
                    nasm_error(ERR_NONFATAL|ERR_NOFILE|ERR_USAGE,
                               "unknown dependency option `-M%c'", p[2]);
                    break;
                }
                if (advance && (!q || !q[0])) {
                    nasm_error(ERR_NONFATAL|ERR_NOFILE|ERR_USAGE,
                               "option `-M%c' requires a parameter", p[2]);
                    break;
                }
            }
            break;

        case '-':
            {
                int s;

                if (p[2] == 0) {        /* -- => stop processing options */
                    stopoptions = 1;
                    break;
                }

                if (!nasm_stricmp(p, "--v"))
                    show_version();

                if (!nasm_stricmp(p, "--version"))
                    show_version();

                for (s = 0; textopts[s].label; s++) {
                    if (!nasm_stricmp(p + 2, textopts[s].label)) {
                        break;
                    }
                }

                switch (s) {
                case OPT_PREFIX:
                case OPT_POSTFIX:
                    {
                        if (!q) {
                            nasm_error(ERR_NONFATAL | ERR_NOFILE |
                                         ERR_USAGE,
                                         "option `--%s' requires an argument",
                                         p + 2);
                            break;
                        } else {
                            advance = 1, param = q;
                        }

                        switch (s) {
                        case OPT_PREFIX:
                            if (pass == 2)
                                strlcpy(lprefix, param, PREFIX_MAX);
                            break;
                        case OPT_POSTFIX:
                            if (pass == 2)
                                strlcpy(lpostfix, param, POSTFIX_MAX);
                            break;
                        default:
                            panic();
                            break;
                        }
                        break;
                    }

                default:
                    {
                        nasm_error(ERR_NONFATAL | ERR_NOFILE | ERR_USAGE,
                                     "unrecognised option `--%s'", p + 2);
                        break;
                    }
                }
                break;
            }

        default:
            nasm_error(ERR_NONFATAL | ERR_NOFILE | ERR_USAGE,
                       "unrecognised option `-%c'", p[1]);
            break;
        }
    } else if (pass == 2) {
        if (*inname) {
            nasm_error(ERR_NONFATAL | ERR_NOFILE | ERR_USAGE,
                         "more than one input file specified");
        } else {
            copy_filename(inname, p);
        }
    }

    return advance;
}

#define ARG_BUF_DELTA 128

static void process_respfile(FILE * rfile, int pass)
{
    char *buffer, *p, *q, *prevarg;
    int bufsize, prevargsize;

    bufsize = prevargsize = ARG_BUF_DELTA;
    buffer = nasm_malloc(ARG_BUF_DELTA);
    prevarg = nasm_malloc(ARG_BUF_DELTA);
    prevarg[0] = '\0';

    while (1) {                 /* Loop to handle all lines in file */
        p = buffer;
        while (1) {             /* Loop to handle long lines */
            q = fgets(p, bufsize - (p - buffer), rfile);
            if (!q)
                break;
            p += strlen(p);
            if (p > buffer && p[-1] == '\n')
                break;
            if (p - buffer > bufsize - 10) {
                int offset;
                offset = p - buffer;
                bufsize += ARG_BUF_DELTA;
                buffer = nasm_realloc(buffer, bufsize);
                p = buffer + offset;
            }
        }

        if (!q && p == buffer) {
            if (prevarg[0])
                process_arg(prevarg, NULL, pass);
            nasm_free(buffer);
            nasm_free(prevarg);
            return;
        }

        /*
         * Play safe: remove CRs, LFs and any spurious ^Zs, if any of
         * them are present at the end of the line.
         */
        *(p = &buffer[strcspn(buffer, "\r\n\032")]) = '\0';

        while (p > buffer && nasm_isspace(p[-1]))
            *--p = '\0';

        p = nasm_skip_spaces(buffer);

        if (process_arg(prevarg, p, pass))
            *p = '\0';

        if ((int) strlen(p) > prevargsize - 10) {
            prevargsize += ARG_BUF_DELTA;
            prevarg = nasm_realloc(prevarg, prevargsize);
        }
        strncpy(prevarg, p, prevargsize);
    }
}

/* Function to process args from a string of args, rather than the
 * argv array. Used by the environment variable and response file
 * processing.
 */
static void process_args(char *args, int pass)
{
    char *p, *q, *arg, *prevarg;
    char separator = ' ';

    p = args;
    if (*p && *p != '-')
        separator = *p++;
    arg = NULL;
    while (*p) {
        q = p;
        while (*p && *p != separator)
            p++;
        while (*p == separator)
            *p++ = '\0';
        prevarg = arg;
        arg = q;
        if (process_arg(prevarg, arg, pass))
            arg = NULL;
    }
    if (arg)
        process_arg(arg, NULL, pass);
}

static void process_response_file(const char *file, int pass)
{
    char str[2048];
    FILE *f = nasm_open_read(file, NF_TEXT);
    if (!f) {
        perror(file);
        exit(-1);
    }
    while (fgets(str, sizeof str, f)) {
        process_args(str, pass);
    }
    fclose(f);
}

static void parse_cmdline(int argc, char **argv, int pass)
{
    FILE *rfile;
    char *envreal, *envcopy = NULL, *p;
    int i;

    *inname = *outname = *listname = *errname = '\0';

    /* Initialize all the warnings to their default state */
    for (i = 0; i < ERR_WARN_ALL; i++) {
        warning_state_init[i] = warning_state[i] =
	    warnings[i].enabled ? WARN_ST_ENABLED : 0;
    }

    /*
     * First, process the NASMENV environment variable.
     */
    envreal = getenv("NASMENV");
    if (envreal) {
        envcopy = nasm_strdup(envreal);
        process_args(envcopy, pass);
        nasm_free(envcopy);
    }

    /*
     * Now process the actual command line.
     */
    while (--argc) {
        bool advance;
        argv++;
        if (argv[0][0] == '@') {
            /*
             * We have a response file, so process this as a set of
             * arguments like the environment variable. This allows us
             * to have multiple arguments on a single line, which is
             * different to the -@resp file processing below for regular
             * NASM.
             */
            process_response_file(argv[0]+1, pass);
            argc--;
            argv++;
        }
        if (!stopoptions && argv[0][0] == '-' && argv[0][1] == '@') {
            p = get_param(argv[0], argc > 1 ? argv[1] : NULL, &advance);
            if (p) {
                rfile = nasm_open_read(p, NF_TEXT);
                if (rfile) {
                    process_respfile(rfile, pass);
                    fclose(rfile);
                } else
                    nasm_error(ERR_NONFATAL | ERR_NOFILE | ERR_USAGE,
                                 "unable to open response file `%s'", p);
            }
        } else
            advance = process_arg(argv[0], argc > 1 ? argv[1] : NULL, pass);
        argv += advance, argc -= advance;
    }

    /*
     * Look for basic command line typos. This definitely doesn't
     * catch all errors, but it might help cases of fumbled fingers.
     */
    if (pass != 2)
        return;

    if (!*inname)
        nasm_error(ERR_NONFATAL | ERR_NOFILE | ERR_USAGE,
                   "no input file specified");
    else if (!strcmp(inname, errname)   ||
             !strcmp(inname, outname)   ||
             !strcmp(inname, listname)  ||
             (depend_file && !strcmp(inname, depend_file)))
        nasm_fatal(ERR_NOFILE | ERR_USAGE,
                   "file `%s' is both input and output file",
                   inname);

    if (*errname) {
        error_file = nasm_open_write(errname, NF_TEXT);
        if (!error_file) {
            error_file = stderr;        /* Revert to default! */
            nasm_fatal(ERR_NOFILE | ERR_USAGE,
                       "cannot open file `%s' for error messages",
                       errname);
        }
    }
}

#define isEAFlags(op, ea) (!(ea & ~op.eaflags))
#define isOpOfType(op, ty) (!(ty & ~op.type))
#define regName(reg) (nasm_reg_names[reg-EXPR_REG_START])

static const char * nacl_get_32_bit_reg(const char* registerToClear)
{
	const char * loweredRegName;
	if(strcmp(registerToClear, "rax") == 0)	{ loweredRegName = "eax"; }
	else if(strcmp(registerToClear, "rbx") == 0) { loweredRegName = "ebx"; }
	else if(strcmp(registerToClear, "rcx") == 0) { loweredRegName = "ecx"; }
	else if(strcmp(registerToClear, "rdx") == 0) { loweredRegName = "edx"; }
	else if(strcmp(registerToClear, "rsi") == 0) { loweredRegName = "esi"; }
	else if(strcmp(registerToClear, "rdi") == 0) { loweredRegName = "edi"; }
	else if(strcmp(registerToClear, "rsi") == 0) { loweredRegName = "esi"; }
	else if(strcmp(registerToClear, "rbp") == 0) { loweredRegName = "ebp"; }
	else if(strcmp(registerToClear, "rsp") == 0) { loweredRegName = "esp"; }
	else if(strcmp(registerToClear, "r8") == 0) { loweredRegName = "r8d"; }
	else if(strcmp(registerToClear, "r9") == 0) { loweredRegName = "r9d"; }
	else if(strcmp(registerToClear, "r10") == 0) { loweredRegName = "r10d"; }
	else if(strcmp(registerToClear, "r11") == 0) { loweredRegName = "r11d"; }
	else if(strcmp(registerToClear, "r12") == 0) { loweredRegName = "r12d"; }
	else if(strcmp(registerToClear, "r13") == 0) { loweredRegName = "r13d"; }
	else if(strcmp(registerToClear, "r14") == 0) { loweredRegName = "r14d"; }
	else if(strcmp(registerToClear, "r15") == 0) { loweredRegName = "r15d"; }
	else { nasm_fatal(0, "unknown 64 bit reg: %s\n", registerToClear); }
	return loweredRegName;
}
static void nacl_clear_64_bit_reg_top32(enum reg_enum registerToClear, insn* out)
{
	int pass1 = 0;
	const char* loweredRegName;
	char instString[256];

	loweredRegName = nacl_get_32_bit_reg(regName(registerToClear));

	sprintf(instString, "mov %s,%s", loweredRegName, loweredRegName);
	parse_line(pass1, instString, out, NULL /* label callback */);
}

static bool nacl_use_register_as_scratch(enum reg_enum registerToCheck)
{
	if(registerToCheck == R_none)
	{
		nasm_fatal(0, "nacl_use_register_as_scratch got null reg\n");
	}

	if(registerToCheck == R_RAX
		|| registerToCheck == R_RBX
		|| registerToCheck == R_RCX
		|| registerToCheck == R_RDX
		|| registerToCheck == R_RSI
		|| registerToCheck == R_RDI
		|| registerToCheck == R_RSI
		|| registerToCheck == R_RBP
		|| registerToCheck == R_R8
		|| registerToCheck == R_R9
		|| registerToCheck == R_R10
		|| registerToCheck == R_R11
		|| registerToCheck == R_R12
		|| registerToCheck == R_R13
		|| registerToCheck == R_R14
		/* || registerToCheck == R_R15 - not allowed */
		/* || registerToCheck == R_RSP - not allowed */
	)
	{
		return true;
	}

	return false;
}

static const char* nacl_gen_operand_size_modifier(const char* instrName, operand* op)
{
	if(strcmp(instrName, "movzx") == 0)
	{
		unsigned int opsize = ((op->type & SIZE_MASK) >> SIZE_SHIFT) * 8;
	    switch(opsize)
	    {
			case 8:
				return "BYTE";
			case 16:
				return "WORD";
			case 32:
				return "DWORD";
			case 64:
				return "QWORD";
			case 80:
				return "TWORD";
			case 128:
				return "OWORD";
			case 256:
				return "YWORD";
			case 512:
				return "ZWORD";
			default:
				nasm_fatal(0, "Unknown operand size type\n");
	    }

	}

	return "";
}

static void get_memory_string_rep(enum reg_enum baseReg, enum reg_enum indexReg, int scale, int64_t offset, char *str)
{
	char baseRegExp[256];

	if(baseReg != R_none)
	{
		sprintf(baseRegExp, "%s +", regName(baseReg));
	}
	else
	{
		baseRegExp[0] = '\0'; /* empty string */
	}

	if(indexReg == R_none)
	{
		sprintf(str, "[%s %ld]", baseRegExp, offset);
	}
	else
	{
		const char* indexRegStr = nasm_reg_names[indexReg-EXPR_REG_START];
		char scale_str[32];
		char offset_str[256];

		if (scale == 1) {
			scale_str[0] = '\0';
		} else {
			sprintf(scale_str, "*%d", scale);
		}

		if (offset == 0) {
			offset_str[0] = '\0';
		} else {
			sprintf(offset_str, "+ %ld", offset);
		}

		sprintf(str, "[%s %s%s %s]", baseRegExp, indexRegStr, scale_str, offset_str);
	}
}

static int is_rsp_rbp_inst(operand* op)
{
	return (op->basereg == R_RSP || op->basereg == R_RBP) && op->indexreg == R_none;
}

static void nacl_replace_instruction(insn* ins, int* count, insn* ret, bool* shouldBeInSameBlock)
{
	bool useDefault = false;
	int pass1 = 0;

    if(ofmt == &of_elf32)
    {
    	if(ins->opcode == I_RET || ins->opcode == I_RETN || ins->opcode == I_RETF)
    	{
    		*count = 3;
    		shouldBeInSameBlock[0] = false;
    		shouldBeInSameBlock[1] = shouldBeInSameBlock[2] = true;

            parse_line(pass1, "pop ecx", &(ret[0]), NULL /* label callback */);
            parse_line(pass1, "and ecx,0xFFFFFFE0", &(ret[1]), NULL /* label callback */);
            if(ins->opcode == I_RETN)
            	parse_line(pass1, "jmp near ecx", &(ret[2]), NULL /* label callback */);
            else if(ins->opcode == I_RETF)
    			parse_line(pass1, "jmp far ecx", &(ret[2]), NULL /* label callback */);
            else if(ins->opcode == I_RET)
    			parse_line(pass1, "jmp ecx", &(ret[2]), NULL /* label callback */);
            else
                nasm_fatal(0, "unknown return type code\n");
    	}
    	else
    	{
    		useDefault = true;
    	}
    }
    else if(ofmt == &of_elf64)
    {
    	if(ins->opcode == I_STOSB || ins->opcode == I_STOSD || ins->opcode == I_STOSQ || ins->opcode == I_STOSW)
    	{
    		*count = 3;
    		shouldBeInSameBlock[0] = shouldBeInSameBlock[1] = shouldBeInSameBlock[2] = true;

    		parse_line(pass1, "mov edi,edi", &(ret[0]), NULL /* label callback */);
    		parse_line(pass1, "lea rdi,[r15+rdi]", &(ret[1]), NULL /* label callback */);
    		ret[2] = *ins;
    	}
    	else if(ins->opcode == I_RET || ins->opcode == I_RETN || ins->opcode == I_RETF)
    	{
            *count = 4;
    		shouldBeInSameBlock[0] = false;
    		shouldBeInSameBlock[1] = shouldBeInSameBlock[2] = shouldBeInSameBlock[3] = true;

            parse_line(pass1, "pop r11", &(ret[0]), NULL /* label callback */);
            parse_line(pass1, "and r11d,0xFFFFFFE0", &(ret[1]), NULL /* label callback */);
            parse_line(pass1, "add r11,r15", &(ret[2]), NULL /* label callback */);
            if(ins->opcode == I_RETN)
                parse_line(pass1, "jmp near r11", &(ret[3]), NULL /* label callback */);
            else if(ins->opcode == I_RETF)
                parse_line(pass1, "jmp far r11", &(ret[3]), NULL /* label callback */);
            else if(ins->opcode == I_RET)
                parse_line(pass1, "jmp r11", &(ret[3]), NULL /* label callback */);
    	}
    	else if(ins->opcode == I_POP && ins->oprs[0].basereg == R_RSP)
    	{
    		*count = 2;
			shouldBeInSameBlock[0] = shouldBeInSameBlock[1] = true;

			parse_line(pass1, "mov esp,[rsp]", &(ret[0]), NULL /* label callback */);
			parse_line(pass1, "add rsp,r15", &(ret[1]), NULL /* label callback */);
    	}
    	else if(ins->opcode == I_POP && ins->oprs[0].basereg == R_RBP)
    	{
    		*count = 4;
    		shouldBeInSameBlock[0] = shouldBeInSameBlock[1] = true;
			shouldBeInSameBlock[2] = shouldBeInSameBlock[3] = true;

			parse_line(pass1, "mov ebp,[rsp]", &(ret[0]), NULL /* label callback */);
			parse_line(pass1, "add rbp,r15", &(ret[1]), NULL /* label callback */);
			parse_line(pass1, "add esp,8", &(ret[2]), NULL /* label callback */);
			parse_line(pass1, "add rsp,r15", &(ret[3]), NULL /* label callback */);
    	}
    	else if(ins->opcode == I_PREFETCH || ins->opcode == I_PREFETCHNTA ||
			ins->opcode == I_PREFETCHT0 || ins->opcode == I_PREFETCHT1 ||
			ins->opcode == I_PREFETCHT2 || ins->opcode == I_PREFETCHW ||
			ins->opcode == I_PREFETCHWT1
		)
    	{
    		if(ins->oprs[0].indexreg == R_none)
    		{
    			char instStr[256];
    			char memoryStringRep[256];
    			const char* instrName;

        		*count = 2;
        		/*this instruction does not modify memory so the top 32 bits clear does not need to be adjacent*/
        		shouldBeInSameBlock[0] = shouldBeInSameBlock[1] = false;

    			instrName = nasm_insn_names[ins->opcode];

        		nacl_clear_64_bit_reg_top32(ins->oprs[0].basereg, &(ret[0]));

        		get_memory_string_rep(R_R15, ins->oprs[0].basereg, 1 /* scale */, ins->oprs[0].offset, memoryStringRep);
        		sprintf(instStr, "%s %s", instrName, memoryStringRep);
    			parse_line(pass1, instStr, &(ret[1]), NULL /* label callback */);
    		}
    		else
    		{
    			/* prefetchnta that uses SIB scaling
				Not seeing anything like this for now so not building the conversion
				If needed, this should be easy enough to do
				This doesn't affect correctness - only perf, so ignoring
				*/

                /* *count = 2;
    			nasm_error(ERR_WARNING, "NaCl conversion of prefetch instructions that use SIB addressing not yet supported\n");*/

                /* Actually it seems that the nacl verifier does not have an issue with this sort of code
                so leaving it unchanged */
                useDefault = true;

    		}
    	}
    	else if(ins->operands == 1 && isOpOfType(ins->oprs[0], MEMORY))
    	{
    		/* one op instructions that modify memory
    		Not seeing anything like this for now so not building the conversion
    		If needed, this should be easy enough to do */
    		nasm_fatal(0, "NaCl conversion of Single instructions that access memory not yet supported");
    	}
    	else if(ins->operands == 2 && isOpOfType(ins->oprs[0], REGISTER) && ins->oprs[0].basereg == R_RSP)
    	{
    		/*some manipulation of rsp - should go through esp*/
			char instStr[256];
			const char* instrName;

			*count = 2;
    		shouldBeInSameBlock[0] = shouldBeInSameBlock[1] = true;

			instrName = nasm_insn_names[ins->opcode];

			if(isOpOfType(ins->oprs[1], IMMEDIATE))
			{
				sprintf(instStr, "%s esp,%ld", instrName, ins->oprs[1].offset);
			}
			else if(isOpOfType(ins->oprs[1], REGISTER))
			{
				const char* loweredReg = nacl_get_32_bit_reg(regName(ins->oprs[1].basereg));
				sprintf(instStr, "%s esp,%s", instrName, loweredReg);
			}
			else if(isOpOfType(ins->oprs[1], MEMORY))
			{
				char memoryStringRep[256];
				get_memory_string_rep(ins->oprs[1].basereg, ins->oprs[1].indexreg, ins->oprs[1].scale, ins->oprs[1].offset, memoryStringRep);

				sprintf(instStr, "%s esp,%s", instrName, memoryStringRep);
			}
			else
			{
				nasm_fatal(0, "Unexpected operand type in rsp manipulation\n");
			}

			parse_line(pass1, instStr, &(ret[0]), NULL /* label callback */);
			parse_line(pass1, "add rsp,r15", &(ret[1]), NULL /* label callback */);
    	}
        else if (ins->opcode == I_PINSRW)
        {
			char instStr[256];
			char memoryStringRep[256];
			const char* instrName;

			enum reg_enum srcBaseReg;
			int64_t srcOffset;
			enum reg_enum destReg;

			instrName = nasm_insn_names[ins->opcode];
			srcBaseReg = ins->oprs[1].basereg;
			srcOffset = ins->oprs[1].offset;
			destReg = ins->oprs[0].basereg;

			*count = 2;
			shouldBeInSameBlock[0] = shouldBeInSameBlock[1] = true;

			nacl_clear_64_bit_reg_top32(srcBaseReg, &(ret[0]));

			get_memory_string_rep(R_R15, srcBaseReg, 1 /* scale */, 0 /* offset specified as a different operand in this inst */, memoryStringRep);
			sprintf(instStr, "%s %s,%s,%ld", instrName, regName(destReg), memoryStringRep, srcOffset);
			parse_line(pass1, instStr, &(ret[1]), NULL /* label callback */);

        }
    	else if(
			ins->opcode != I_LEA && /* lea instructions look like memory ops but they arent*/
			isOpOfType(ins->oprs[0], MEMORY) && /*dest is mem loc*/
			!is_rsp_rbp_inst(&(ins->oprs[0])) && /*dest referred to by rsp/rbp which has the full pointer so no change necessary*/
			!isEAFlags(ins->oprs[0], EAF_REL) && /*ignore instr pointer relative addressing*/
			isOpOfType(ins->oprs[1], REGISTER) && /*src is a register*/
			ins->oprs[0].indexreg == R_none && ins->oprs[0].offset >= 0 /* easy case - does not use SIB addressing and positive offset */
    	)
    	{
    		char instStr[256];
    		char memoryStringRep[256];
    		const char* instrName;

			enum reg_enum srcReg;
			enum reg_enum destBaseReg;
			int64_t destOffset;

			instrName = nasm_insn_names[ins->opcode];
			srcReg = ins->oprs[1].basereg;
			destBaseReg = ins->oprs[0].basereg;
			destOffset = ins->oprs[0].offset;

			*count = 2;
			shouldBeInSameBlock[0] = shouldBeInSameBlock[1] = true;

			nacl_clear_64_bit_reg_top32(destBaseReg, &(ret[0]));

			get_memory_string_rep(R_R15, destBaseReg, 1 /* scale */, destOffset, memoryStringRep);
			sprintf(instStr, "%s %s,%s", instrName, memoryStringRep, regName(srcReg));
			parse_line(pass1, instStr, &(ret[1]), NULL /* label callback */);
    	}
    	else if(
			ins->opcode != I_LEA && /* lea instructions look like memory ops but they arent*/
			isOpOfType(ins->oprs[0], REGISTER) && /*dest is a register*/
			isOpOfType(ins->oprs[1], MEMORY) && /*src is mem loc*/
			!is_rsp_rbp_inst(&(ins->oprs[1])) && /*src referred to by rsp/rbp which has the full pointer so no change necessary*/
			!isEAFlags(ins->oprs[1], EAF_REL) && /*ignore instr pointer relative addressing*/
			ins->oprs[1].indexreg == R_none && ins->oprs[1].offset >= 0 /* easy case - does not use SIB addressing and positive offset */
		)
    	{
            enum reg_enum srcBaseReg;
            srcBaseReg = ins->oprs[1].basereg;
            if (srcBaseReg == R_none) {
                /* There is an edge case where SSE loads of constant data look like this
                Easy way to identify this by checking there are no registers */
                useDefault = true;
            } else {
                char instStr[256];
                char memoryStringRep[256];
                const char* instrName;

                int64_t srcOffset;
                enum reg_enum destReg;

                instrName = nasm_insn_names[ins->opcode];
                srcOffset = ins->oprs[1].offset;
                destReg = ins->oprs[0].basereg;

                *count = 2;
                shouldBeInSameBlock[0] = shouldBeInSameBlock[1] = true;

                nacl_clear_64_bit_reg_top32(srcBaseReg, &(ret[0]));

                get_memory_string_rep(R_R15, srcBaseReg, 1 /* scale */, srcOffset, memoryStringRep);
                sprintf(instStr, "%s %s,%s", instrName, regName(destReg), memoryStringRep);
                parse_line(pass1, instStr, &(ret[1]), NULL /* label callback */);
            }
    	}
    	else if(
			/*like before but uses SIB addressing or negative offset*/
			ins->opcode != I_LEA && /* lea instructions look like memory ops but they arent*/
			isOpOfType(ins->oprs[0], MEMORY) && /*dest is mem loc*/
			!is_rsp_rbp_inst(&(ins->oprs[0])) && /*dest referred to by rsp/rbp which has the full pointer so no change necessary*/
			!isEAFlags(ins->oprs[0], EAF_REL) && /*ignore instr pointer relative addressing*/
			isOpOfType(ins->oprs[1], REGISTER)/*src is a register*/
    	)
    	{
    		char instStr[256];
    		char memoryStringRep[256];
    		const char* instrName;

			enum reg_enum srcReg;
			enum reg_enum destBaseReg;
			enum reg_enum destIndexReg;
			int64_t destOffset;
			int destScale;
			enum reg_enum scratchReg;
			const char* operandSizeModifier;

			instrName = nasm_insn_names[ins->opcode];
			srcReg = ins->oprs[1].basereg;
			destBaseReg = ins->oprs[0].basereg;
			destIndexReg = ins->oprs[0].indexreg;
			destOffset = ins->oprs[0].offset;
			destScale = ins->oprs[0].scale;

			*count = 5;
			shouldBeInSameBlock[0] = shouldBeInSameBlock[1] = false;
			shouldBeInSameBlock[2] = shouldBeInSameBlock[3] = true;
			shouldBeInSameBlock[4] = false;

			/* find a scratch register to compute the right address */
			if(destIndexReg != R_none && destIndexReg != R_R15 && destIndexReg != R_RSP && destIndexReg != R_RBP)
			{
				scratchReg = destIndexReg;
			}
			else
			{
				/* pick a scratch register at random */
				scratchReg = R_R14;
			}

			sprintf(instStr, "push %s", regName(scratchReg));
			parse_line(pass1, instStr, &(ret[0]), NULL /* label callback */);

			get_memory_string_rep(destBaseReg, destIndexReg, destScale, destOffset, memoryStringRep);
			sprintf(instStr, "lea %s,%s", regName(scratchReg), memoryStringRep);
			parse_line(pass1, instStr, &(ret[1]), NULL /* label callback */);

			nacl_clear_64_bit_reg_top32(scratchReg, &(ret[2]));

			operandSizeModifier = nacl_gen_operand_size_modifier(instrName, &(ins->oprs[0]));
			sprintf(instStr, "%s %s[r15 + %s],%s", instrName, operandSizeModifier, regName(scratchReg), regName(srcReg));
			parse_line(pass1, instStr, &(ret[3]), NULL /* label callback */);

			sprintf(instStr, "pop %s", regName(scratchReg));
			parse_line(pass1, instStr, &(ret[4]), NULL /* label callback */);
    	}
    	else if(
			/*like before but uses SIB addressing or negative offset*/
			ins->opcode != I_LEA && /* lea instructions look like memory ops but they arent*/
			isOpOfType(ins->oprs[0], REGISTER) && /*dest is a register*/
			isOpOfType(ins->oprs[1], MEMORY) && /*src is mem loc*/
			!is_rsp_rbp_inst(&(ins->oprs[1])) && /*src referred to by rsp/rbp which has the full pointer so no change necessary*/
			!isEAFlags(ins->oprs[1], EAF_REL)/*ignore instr pointer relative addressing*/
		)
    	{
    		char instStr[256];
    		char memoryStringRep[256];
    		const char* instrName;

			enum reg_enum srcBaseReg;
			enum reg_enum srcIndexReg;
			int64_t srcOffset;
			int srcScale;
			enum reg_enum destReg;
			bool destinationRegIsTemp;
			enum reg_enum scratchReg;
			const char* operandSizeModifier;

			instrName = nasm_insn_names[ins->opcode];
			srcBaseReg = ins->oprs[1].basereg;
			srcIndexReg = ins->oprs[1].indexreg;
			srcOffset = ins->oprs[1].offset;
			srcScale = ins->oprs[1].scale;
			destReg = ins->oprs[0].basereg;

			destinationRegIsTemp = nacl_use_register_as_scratch(destReg);

			/* find a scratch register to compute the right address */
			if(destinationRegIsTemp)
			{
				*count = 3;
				shouldBeInSameBlock[0] = false;
				shouldBeInSameBlock[1] = shouldBeInSameBlock[2] = true;
				scratchReg = destReg;
			}
			else
			{
				*count = 5;
				shouldBeInSameBlock[0] = shouldBeInSameBlock[1] = false;
				shouldBeInSameBlock[2] = shouldBeInSameBlock[3] = true;
				shouldBeInSameBlock[4] = false;

				if(srcIndexReg != R_none && srcIndexReg != R_R15 && srcIndexReg != R_RSP && srcIndexReg != R_RBP)
				{
					scratchReg = srcIndexReg;
				}
				else if(srcBaseReg != R_none && srcBaseReg != R_R15 && srcBaseReg != R_RSP && srcBaseReg != R_RBP)
				{
					scratchReg = srcBaseReg;
				}
				else
				{
					/* pick a scratch register at random */
					scratchReg = R_R14;
				}

				sprintf(instStr, "push %s", regName(scratchReg));
				parse_line(pass1, instStr, &(ret[0]), NULL /* label callback */);

				/* if we increment ret here instructions after the branch can start writing to base of ret
				   independent of whether we came through the if or else */
				ret++;
			}


			get_memory_string_rep(srcBaseReg, srcIndexReg, srcScale, srcOffset, memoryStringRep);
			sprintf(instStr, "lea %s,%s", regName(scratchReg), memoryStringRep);
			parse_line(pass1, instStr, &(ret[0]), NULL /* label callback */);

			nacl_clear_64_bit_reg_top32(scratchReg, &(ret[1]));

			operandSizeModifier = nacl_gen_operand_size_modifier(instrName, &(ins->oprs[1]));
			sprintf(instStr, "%s %s,%s[r15 + %s]", instrName, regName(destReg), operandSizeModifier, regName(scratchReg));
			parse_line(pass1, instStr, &(ret[2]), NULL /* label callback */);

			if(!destinationRegIsTemp)
			{
				sprintf(instStr, "pop %s", regName(scratchReg));
				parse_line(pass1, instStr, &(ret[3]), NULL /* label callback */);
			}
    	}
		else
		{
			useDefault = true;
		}
    }
    else
    {
    	nasm_fatal(0, "unknown file format\n");
    }

    if(useDefault)
    {
		*count = 1;
		shouldBeInSameBlock[0] = false;
		ret[0] = *ins;
    }
}

static int get_nacl_instruction_padding(int64_t offset, int minSpaceInCurrBlock, insn* output_ins, int64_t rawInstrSize)
{
	if(ofmt == &of_elf32 || ofmt == &of_elf64)
	{
		if(rawInstrSize >= 32)
		{
			nasm_error(ERR_NONFATAL, "Instruction size greater than 32");
			return 0;
		}

		if(minSpaceInCurrBlock > 32)
		{
			nasm_error(ERR_NONFATAL, "More than 32 bytes of instructions that can't be separated");
			return 0;
		}

		if(rawInstrSize <= 0)
		{
			return 0;
		}

		{
			/* Values from 0 to 31 */
			int previousFinishOffset = (offset + 31) % 32;
			/* Values from 0 to 31 */
			int currentStartOffset = (previousFinishOffset + 1) % 32;

			/* Values from 1 to 32 */
			int remainingSpace = 32 - currentStartOffset;
			if(minSpaceInCurrBlock > remainingSpace)
			{
				return remainingSpace;
			}

			if(output_ins->opcode == I_CALL)
			{
				/* rawInstrSize for call is usually 5 for 32 bit elf, so targetStartOffset is usually 27 */
				int targetStartOffset = 32 - rawInstrSize;
				int paddingRequired = (targetStartOffset - currentStartOffset + 32) % 32;
				return paddingRequired;
			}
			else
			{
				/* Values from 0 to 31 */
				int currentInstructionFinishOffset = (currentStartOffset + rawInstrSize + 31) % 32;

				if(currentInstructionFinishOffset < rawInstrSize - 1)
				{
					/* instruction would straddle 32 bit boundary */
					/* return the padding required to nearest 32 byte boundary */
					return 32 - previousFinishOffset - 1;
				}
				else
				{
					/* Instruction fits before boundary - no padding needed */
					return 0;
				}
			}
		}
	}
	else
	{
		nasm_error(ERR_NONFATAL, "Boundary checks not supported in formats other than elf32 or elf64");
		return 0;
	}
}

static void process_non_directive_line(char *line, int64_t* offs, insn* output_ins, insn* noop_ins, ldfunc def_label)
{
    int i;

	if (optimizing > 0) {
		if (forwref != NULL && globallineno == forwref->lineno) {
			output_ins->forw_ref = true;
			do {
				output_ins->oprs[forwref->operand].opflags |= OPFLAG_FORWARD;
				forwref = saa_rstruct(forwrefs);
			} while (forwref != NULL
					 && forwref->lineno == globallineno);
		} else
			output_ins->forw_ref = false;

		if (output_ins->forw_ref) {
			if (passn == 1) {
				for (i = 0; i < output_ins->operands; i++) {
					if (output_ins->oprs[i].opflags & OPFLAG_FORWARD) {
						struct forwrefinfo *fwinf = (struct forwrefinfo *)saa_wstruct(forwrefs);
						fwinf->lineno = globallineno;
						fwinf->operand = i;
					}
				}
			}
		}
	}

	/*  forw_ref */
	if (output_ins->opcode == I_EQU) {
		if (pass1 == 1) {
			/*
			 * Special `..' EQUs get processed in pass two,
			 * except `..@' macro-processor EQUs which are done
			 * in the normal place.
			 */
			if (!output_ins->label)
				nasm_error(ERR_NONFATAL,
						   "EQU not preceded by label");

			else if (output_ins->label[0] != '.' ||
					 output_ins->label[1] != '.' ||
					 output_ins->label[2] == '@') {
				if (output_ins->operands == 1 &&
					(output_ins->oprs[0].type & IMMEDIATE) &&
					output_ins->oprs[0].wrt == NO_SEG) {
					bool isext = !!(output_ins->oprs[0].opflags & OPFLAG_EXTERN);
					def_label(output_ins->label,
							  output_ins->oprs[0].segment,
							  output_ins->oprs[0].offset, NULL,
							  false, isext);
				} else if (output_ins->operands == 2
						   && (output_ins->oprs[0].type & IMMEDIATE)
						   && (output_ins->oprs[0].type & COLON)
						   && output_ins->oprs[0].segment == NO_SEG
						   && output_ins->oprs[0].wrt == NO_SEG
						   && (output_ins->oprs[1].type & IMMEDIATE)
						   && output_ins->oprs[1].segment == NO_SEG
						   && output_ins->oprs[1].wrt == NO_SEG) {
					def_label(output_ins->label,
							  output_ins->oprs[0].offset | SEG_ABS,
							  output_ins->oprs[1].offset,
							  NULL, false, false);
				} else
					nasm_error(ERR_NONFATAL,
							   "bad syntax for EQU");
			}
		} else {
			/*
			 * Special `..' EQUs get processed here, except
			 * `..@' macro processor EQUs which are done above.
			 */
			if (output_ins->label[0] == '.' &&
				output_ins->label[1] == '.' &&
				output_ins->label[2] != '@') {
				if (output_ins->operands == 1 &&
					(output_ins->oprs[0].type & IMMEDIATE)) {
					define_label(output_ins->label,
								 output_ins->oprs[0].segment,
								 output_ins->oprs[0].offset,
								 NULL, false, false);
				} else if (output_ins->operands == 2
						   && (output_ins->oprs[0].type & IMMEDIATE)
						   && (output_ins->oprs[0].type & COLON)
						   && output_ins->oprs[0].segment == NO_SEG
						   && (output_ins->oprs[1].type & IMMEDIATE)
						   && output_ins->oprs[1].segment == NO_SEG) {
					define_label(output_ins->label,
								 output_ins->oprs[0].offset | SEG_ABS,
								 output_ins->oprs[1].offset,
								 NULL, false, false);
				} else
					nasm_error(ERR_NONFATAL,
							   "bad syntax for EQU");
			}
		}
	} else {        /* instruction isn't an EQU */

		if (pass1 == 1) {

			int64_t l = 0;

			if(nacl_mode)
			{
				int32_t timesCopy = output_ins->times;
				int64_t offsetCopy = *offs;
				int64_t instructionSize = insn_size(location.segment, *offs, globalbits,
						output_ins);

				if(instructionSize > 0)
				{
					int replaceCount;
					insn replacedInstructions[16];
					bool shouldBeInSameBlock[16];
					nacl_replace_instruction(output_ins, &replaceCount, replacedInstructions, shouldBeInSameBlock);

					/* Figure out the size of the instruction by computing the instruction size
					*  multiplied by repeat count, accounting for padding required
					*  to avoid straddling 32 byte boundaries
					*/
					while(timesCopy--)
					{
						for(int i = 0; i < replaceCount; i++)
						{
							int64_t currInstSize;
							int paddingSize;
							int minSpaceInCurrBlock = 0;

							for(int j = i; j < replaceCount; j++)
							{
								if(!shouldBeInSameBlock[i])
								{
									break;
								}

								minSpaceInCurrBlock += insn_size(location.segment, offsetCopy, globalbits, &(replacedInstructions[j]));
							}

							currInstSize = insn_size(location.segment, offsetCopy, globalbits, &(replacedInstructions[i]));
							paddingSize = get_nacl_instruction_padding(offsetCopy, minSpaceInCurrBlock, &(replacedInstructions[i]), currInstSize);
							l += currInstSize + paddingSize;
							offsetCopy += currInstSize + paddingSize;
						}
					}
				}
				else
				{
					/* 0 This happens for some instructions
					 * -1 Apparently this happens on an error
					 *  Resort to the usual NASM behavior for these cases
					 */
					l = instructionSize;
					l *= timesCopy;
				}
			}
			else
			{
				l = insn_size(location.segment, *offs, globalbits,
									  output_ins);
				l *= output_ins->times;
			}

			/* if (using_debug_info)  && output_ins->opcode != -1) */
			if (using_debug_info)
			{       /* fbk 03/25/01 */
					/* this is done here so we can do debug type info */
				int32_t typeinfo =
					TYS_ELEMENTS(output_ins->operands);
				switch (output_ins->opcode) {
				case I_RESB:
					typeinfo =
						TYS_ELEMENTS(output_ins->oprs[0].offset) | TY_BYTE;
					break;
				case I_RESW:
					typeinfo =
						TYS_ELEMENTS(output_ins->oprs[0].offset) | TY_WORD;
					break;
				case I_RESD:
					typeinfo =
						TYS_ELEMENTS(output_ins->oprs[0].offset) | TY_DWORD;
					break;
				case I_RESQ:
					typeinfo =
						TYS_ELEMENTS(output_ins->oprs[0].offset) | TY_QWORD;
					break;
				case I_REST:
					typeinfo =
						TYS_ELEMENTS(output_ins->oprs[0].offset) | TY_TBYTE;
					break;
				case I_RESO:
					typeinfo =
						TYS_ELEMENTS(output_ins->oprs[0].offset) | TY_OWORD;
					break;
				case I_RESY:
					typeinfo =
						TYS_ELEMENTS(output_ins->oprs[0].offset) | TY_YWORD;
					break;
				case I_DB:
					typeinfo |= TY_BYTE;
					break;
				case I_DW:
					typeinfo |= TY_WORD;
					break;
				case I_DD:
					if (output_ins->eops_float)
						typeinfo |= TY_FLOAT;
					else
						typeinfo |= TY_DWORD;
					break;
				case I_DQ:
					typeinfo |= TY_QWORD;
					break;
				case I_DT:
					typeinfo |= TY_TBYTE;
					break;
				case I_DO:
					typeinfo |= TY_OWORD;
					break;
				case I_DY:
					typeinfo |= TY_YWORD;
					break;
				default:
					typeinfo = TY_LABEL;

				}

				dfmt->debug_typevalue(typeinfo);
			}
			if (l != -1) {
				*offs += l;
				set_curr_offs(*offs);
			}
			/*
			 * else l == -1 => invalid instruction, which will be
			 * flagged as an error on pass 2
			 */

		} else {
			if(nacl_mode)
			{
				int64_t instructionSize = insn_size(location.segment, *offs, globalbits,
									  output_ins);

				if(instructionSize > 0 && output_ins->times > 0)
				{
					const int32_t timesCopy = output_ins->times;

					if(output_ins->times > 1)
					{
						/* Overwrite the times to 1 for now, as we are generating the assembly
						*  one at a time, so that we can insert appropriate padding
						*  to avoid straddling 32 byte boundaries. We will restore this later
						*/
						output_ins->times = 1;
					}

					{
						int replaceCount;
						insn replacedInstructions[16];
						bool shouldBeInSameBlock[16];
						nacl_replace_instruction(output_ins, &replaceCount, replacedInstructions, shouldBeInSameBlock);

						/* generate instructions as many times as necessary */
						for(int32_t ignor = 0; ignor< timesCopy; ignor++)
						{
							for(int i = 0; i < replaceCount; i++)
							{
								/* If the instruction doesn't fit before the next 32 byte boundary,
								*  this function gives the amount of padding required
								*/
								int64_t currInstSize;
								int paddingSize;
								int minSpaceInCurrBlock = 0;

								for(int j = i; j < replaceCount; j++)
								{
									if(!shouldBeInSameBlock[i])
									{
										break;
									}

									minSpaceInCurrBlock += insn_size(location.segment, *offs, globalbits, &(replacedInstructions[j]));
								}

								currInstSize = insn_size(location.segment, *offs, globalbits, &(replacedInstructions[i]));
								paddingSize = get_nacl_instruction_padding(*offs, minSpaceInCurrBlock, &(replacedInstructions[i]), currInstSize);

								/* assemble the appropriate amount of noop instructions */
								for(int j = 0; j < paddingSize; j++)
								{
									*offs += assemble(location.segment, *offs, globalbits, noop_ins);
								}

								*offs += assemble(location.segment, *offs, globalbits, &(replacedInstructions[i]));
							}
						}
					}

					/* restore the output instruction repeat count in case something else depends on this */
					output_ins->times = timesCopy;
				}
				else
				{
					*offs += assemble(location.segment, *offs, globalbits, output_ins);
				}
			}
			else
			{
				*offs += assemble(location.segment, *offs, globalbits, output_ins);
			}
			set_curr_offs(*offs);

		}
	}               /* not an EQU */
	cleanup_insn(output_ins);

	nasm_free(line);
	location.offset = *offs = get_curr_offs();
}

static void assemble_file(char *fname, StrList **depend_ptr)
{
    char *line;
    insn output_ins;
    int64_t offs;
    int pass_max;
    uint64_t prev_offset_changed;
    unsigned int stall_count = 0; /* Make sure we make forward progress... */

    if (cmd_sb == 32 && iflag_ffs(&cmd_cpu) < IF_386)
	nasm_fatal(0, "command line: 32-bit segment size requires a higher cpu");

    pass_max = prev_offset_changed = (INT_MAX >> 1) + 2; /* Almost unlimited */
    for (passn = 1; pass0 <= 2; passn++) {
        ldfunc def_label;
        insn noop_ins;

        pass1 = pass0 == 2 ? 2 : 1;     /* 1, 1, 1, ..., 1, 2 */
        pass2 = passn > 1  ? 2 : 1;     /* 1, 2, 2, ..., 2, 2 */
        /* pass0                           0, 0, 0, ..., 1, 2 */

        def_label = passn > 1 ? redefine_label : define_label;

        globalbits = cmd_sb;  /* set 'bits' to command line default */
        cpu = cmd_cpu;
        if (pass0 == 2) {
	    lfmt->init(listname);
        } else if (passn == 1 && *listname) {
            /* Remove the list file in case we die before the output pass */
            remove(listname);
        }
        in_absolute = false;
        global_offset_changed = 0;  /* set by redefine_label */
        location.segment = ofmt->section(NULL, pass2, &globalbits);
        if (passn > 1) {
            saa_rewind(forwrefs);
            forwref = saa_rstruct(forwrefs);
            raa_free(offsets);
            offsets = raa_init();
        }
        preproc->reset(fname, pass1, pass1 == 2 ? depend_ptr : NULL);

	/* Revert all warnings to the default state */
	memcpy(warning_state, warning_state_init, sizeof warning_state);

        globallineno = 0;
        if (passn == 1)
            location.known = true;
        location.offset = offs = get_curr_offs();

        parse_line(pass1, "nop", &noop_ins, def_label);

        while ((line = preproc->getline())) {
            globallineno++;

            /*
             * Here we parse our directives; this is not handled by the
             * main parser.
             */

            if(nacl_mode && parse_check_is_label(line) && !is_local_label(line))
            {
				/* if label is not aligned, align it */
				int paddingRequired = (32 - (offs % 32)) %32;

				if(paddingRequired > 0)
				{
					insn padding_ins;
					char* paddingInstruction = malloc(128 * sizeof(char));

					sprintf(paddingInstruction, "times %d nop", paddingRequired);
					parse_line(pass1, paddingInstruction, &padding_ins, def_label);
					process_non_directive_line(paddingInstruction, &offs, &padding_ins, &noop_ins, def_label);
				}
            }

            if (process_directives(line))
            {
				nasm_free(line);
				location.offset = offs = get_curr_offs();
            }
            else
            {
				parse_line(pass1, line, &output_ins, def_label);
				process_non_directive_line(line, &offs, &output_ins, &noop_ins, def_label);
            }
        }                       /* end while (line = preproc->getline... */

        if (pass0 == 2 && global_offset_changed && !terminate_after_phase)
            nasm_error(ERR_NONFATAL,
                       "phase error detected at end of assembly.");

        if (pass1 == 1)
            preproc->cleanup(1);

        if ((passn > 1 && !global_offset_changed) || pass0 == 2) {
            pass0++;
        } else if (global_offset_changed &&
                   global_offset_changed < prev_offset_changed) {
            prev_offset_changed = global_offset_changed;
            stall_count = 0;
        } else {
            stall_count++;
        }

        if (terminate_after_phase)
            break;

        if ((stall_count > 997U) || (passn >= pass_max)) {
            /* We get here if the labels don't converge
             * Example: FOO equ FOO + 1
             */
             nasm_error(ERR_NONFATAL,
                          "Can't find valid values for all labels "
                          "after %d passes, giving up.", passn);
             nasm_error(ERR_NONFATAL,
                        "Possible causes: recursive EQUs, macro abuse.");
             break;
        }
    }

    preproc->cleanup(0);
    lfmt->cleanup();
    if (!terminate_after_phase && opt_verbose_info) {
        /*  -On and -Ov switches */
        fprintf(stdout, "info: assembly required 1+%d+1 passes\n", passn-3);
    }
}

/**
 * gnu style error reporting
 * This function prints an error message to error_file in the
 * style used by GNU. An example would be:
 * file.asm:50: error: blah blah blah
 * where file.asm is the name of the file, 50 is the line number on
 * which the error occurs (or is detected) and "error:" is one of
 * the possible optional diagnostics -- it can be "error" or "warning"
 * or something else.  Finally the line terminates with the actual
 * error message.
 *
 * @param severity the severity of the warning or error
 * @param fmt the printf style format string
 */
static void nasm_verror_gnu(int severity, const char *fmt, va_list ap)
{
    const char *currentfile = NULL;
    int32_t lineno = 0;

    if (is_suppressed_warning(severity))
        return;

    if (!(severity & ERR_NOFILE))
	src_get(&lineno, &currentfile);

    if (!skip_this_pass(severity)) {
	if (currentfile) {
	    fprintf(error_file, "%s:%"PRId32": ", currentfile, lineno);
	} else {
	    fputs("nasm: ", error_file);
	}
    }

    nasm_verror_common(severity, fmt, ap);
}

/**
 * MS style error reporting
 * This function prints an error message to error_file in the
 * style used by Visual C and some other Microsoft tools. An example
 * would be:
 * file.asm(50) : error: blah blah blah
 * where file.asm is the name of the file, 50 is the line number on
 * which the error occurs (or is detected) and "error:" is one of
 * the possible optional diagnostics -- it can be "error" or "warning"
 * or something else.  Finally the line terminates with the actual
 * error message.
 *
 * @param severity the severity of the warning or error
 * @param fmt the printf style format string
 */
static void nasm_verror_vc(int severity, const char *fmt, va_list ap)
{
    const char *currentfile = NULL;
    int32_t lineno = 0;

    if (is_suppressed_warning(severity))
        return;

    if (!(severity & ERR_NOFILE))
        src_get(&lineno, &currentfile);

    if (!skip_this_pass(severity)) {
        if (currentfile) {
	    fprintf(error_file, "%s(%"PRId32") : ", currentfile, lineno);
	} else {
	    fputs("nasm: ", error_file);
	}
    }

    nasm_verror_common(severity, fmt, ap);
}

/*
 * check to see if this is a suppressable warning
 */
static inline bool is_valid_warning(int severity)
{
    /* Not a warning at all */
    if ((severity & ERR_MASK) != ERR_WARNING)
        return false;

    return WARN_IDX(severity) < ERR_WARN_ALL;
}

/**
 * check for suppressed warning
 * checks for suppressed warning or pass one only warning and we're
 * not in pass 1
 *
 * @param severity the severity of the warning or error
 * @return true if we should abort error/warning printing
 */
static bool is_suppressed_warning(int severity)
{
    /* Might be a warning but suppresed explicitly */
    if (is_valid_warning(severity))
        return !(warning_state[WARN_IDX(severity)] & WARN_ST_ENABLED);
    else
        return false;
}

static bool warning_is_error(int severity)
{
    if (is_valid_warning(severity))
        return !!(warning_state[WARN_IDX(severity)] & WARN_ST_ERROR);
    else
        return false;
}

static bool skip_this_pass(int severity)
{
    /*
     * See if it's a pass-specific error or warning which should be skipped.
     * We cannot skip errors stronger than ERR_NONFATAL as by definition
     * they cannot be resumed from.
     */
    if ((severity & ERR_MASK) > ERR_NONFATAL)
	return false;

    /*
     * passn is 1 on the very first pass only.
     * pass0 is 2 on the code-generation (final) pass only.
     * These are the passes we care about in this case.
     */
    return (((severity & ERR_PASS1) && passn != 1) ||
	    ((severity & ERR_PASS2) && pass0 != 2));
}

/**
 * common error reporting
 * This is the common back end of the error reporting schemes currently
 * implemented.  It prints the nature of the warning and then the
 * specific error message to error_file and may or may not return.  It
 * doesn't return if the error severity is a "panic" or "debug" type.
 *
 * @param severity the severity of the warning or error
 * @param fmt the printf style format string
 */
static void nasm_verror_common(int severity, const char *fmt, va_list args)
{
    char msg[1024];
    const char *pfx;

    switch (severity & (ERR_MASK|ERR_NO_SEVERITY)) {
    case ERR_WARNING:
        pfx = "warning: ";
        break;
    case ERR_NONFATAL:
        pfx = "error: ";
        break;
    case ERR_FATAL:
        pfx = "fatal: ";
        break;
    case ERR_PANIC:
        pfx = "panic: ";
        break;
    case ERR_DEBUG:
        pfx = "debug: ";
        break;
    default:
        pfx = "";
        break;
    }

    vsnprintf(msg, sizeof msg - 64, fmt, args);
    if (is_valid_warning(severity) && WARN_IDX(severity) != ERR_WARN_OTHER) {
        char *p = strchr(msg, '\0');
	snprintf(p, 64, " [-w+%s]", warnings[WARN_IDX(severity)].name);
    }

    if (!skip_this_pass(severity))
	fprintf(error_file, "%s%s\n", pfx, msg);

    /* Are we recursing from error_list_macros? */
    if (severity & ERR_PP_LISTMACRO)
	return;

    /*
     * Don't suppress this with skip_this_pass(), or we don't get
     * pass1 or preprocessor warnings in the list file
     */
    lfmt->error(severity, pfx, msg);

    if (skip_this_pass(severity))
        return;

    if (severity & ERR_USAGE)
        want_usage = true;

    preproc->error_list_macros(severity);

    switch (severity & ERR_MASK) {
    case ERR_DEBUG:
        /* no further action, by definition */
        break;
    case ERR_WARNING:
        /* Treat warnings as errors */
        if (warning_is_error(severity))
            terminate_after_phase = true;
        break;
    case ERR_NONFATAL:
        terminate_after_phase = true;
        break;
    case ERR_FATAL:
        if (ofile) {
            fclose(ofile);
            remove(outname);
            ofile = NULL;
        }
        if (want_usage)
            usage();
        exit(1);                /* instantly die */
        break;                  /* placate silly compilers */
    case ERR_PANIC:
        fflush(NULL);
        /* abort(); */          /* halt, catch fire, and dump core */
        if (ofile) {
            fclose(ofile);
            remove(outname);
            ofile = NULL;
        }
        exit(3);
        break;
    }
}

static void usage(void)
{
    fputs("type `nasm -h' for help\n", error_file);
}

