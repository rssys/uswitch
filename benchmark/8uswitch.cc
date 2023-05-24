#include <string>
#include <fstream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>

#include <sys/un.h>
#include <sys/syscall.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "uswitchsandbox.h"
#include "uswitch.hpp"

#define DEF_FUNC_PTR(name) decltype(name) *name##_s;
#define SET_FUNC_PTR(name) name##_s = (decltype(name) *)sandbox.get_symbol_addr(#name)

DEF_FUNC_PTR(evhttp_uri_parse);
DEF_FUNC_PTR(evhttp_uri_get_path);
DEF_FUNC_PTR(evhttp_uri_free);
DEF_FUNC_PTR(evhttp_request_get_uri);
DEF_FUNC_PTR(evhttp_send_error);
DEF_FUNC_PTR(evhttp_add_header);
DEF_FUNC_PTR(evhttp_request_get_output_headers);
DEF_FUNC_PTR(evbuffer_new);
DEF_FUNC_PTR(evbuffer_add_reference);
DEF_FUNC_PTR(evhttp_send_reply);
DEF_FUNC_PTR(evbuffer_free);
DEF_FUNC_PTR(event_enable_debug_logging);
DEF_FUNC_PTR(event_config_new);
DEF_FUNC_PTR(event_base_new_with_config);
DEF_FUNC_PTR(event_config_free);
DEF_FUNC_PTR(evhttp_new);
DEF_FUNC_PTR(evhttp_set_gencb);
DEF_FUNC_PTR(evhttp_set_max_body_size);
DEF_FUNC_PTR(evhttp_bind_socket_with_handle);
DEF_FUNC_PTR(event_base_dispatch);

static const char *StringLiterals[] = {
    "Content-Type",
    "text/plain",
    "OK"
};
std::unordered_map<const char *, char *> sandbox_strings;

static void load_sandbox_strings(USwitchSandbox *sandbox) {
    for (const char *s : StringLiterals) {
        size_t len = strlen(s) + 1;
        char *buf = (char *)sandbox->malloc_in_sandbox(len);
        memcpy(buf, s, len);
        sandbox_strings[s] = buf;
    }
}

struct options {
    int port;
    int verbose;
    int max_body_size;

    const char *bind;
    const char *docroot;
};

static bool get_path(uswctx_t ctx, const char *uri, struct options *o, std::string &res) {
    struct evhttp_uri *decoded;
    uswitch_call_dynamic(ctx, evhttp_uri_parse_s, decoded, uri);
    if (!decoded) {
        return false;
    }
    std::string path;
    const char *p = nullptr;
    uswitch_call_dynamic(ctx, evhttp_uri_get_path_s, p, decoded);
    if (p) {
        path = p;
    } else {
        path = "/";
    }
    uswitch_call_dynamic(ctx, evhttp_uri_free_s, decoded);
    res = o->docroot + path;
    return true;
}

static void
send_document_cb(uswctx_t ctx, void *data, struct evhttp_request *req, void *arg)
{
    USwitchSandbox *sandbox = (USwitchSandbox *)data;
    struct options *o = (struct options *)sandbox->custom_data;
    const char *uri;
    uswitch_call_dynamic(ctx, evhttp_request_get_uri_s, uri, req);
    std::string path;
    if (!get_path(ctx, uri, o, path)) {
        uswitch_call_dynamic(ctx, evhttp_send_error_s, req, HTTP_BADREQUEST, nullptr);
        return;
    }

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        uswitch_call_dynamic(ctx, evhttp_send_error_s, req, HTTP_NOTFOUND, nullptr);
        return;
    }
    evkeyvalq *headers;
    uswitch_call_dynamic(ctx, evhttp_request_get_output_headers_s, headers, req);
    uswitch_call_dynamic(ctx, evhttp_add_header_s, nullptr, headers,
        sandbox_strings["Content-Type"], sandbox_strings["text/plain"]);
    
    ifs.seekg(0, std::ios_base::end);
    size_t size = ifs.tellg();
    ifs.seekg(0, std::ios_base::beg);
    uint8_t *input = (uint8_t *)sandbox->malloc_in_sandbox(size);
    if (!input || !ifs.read((char *)input, size)) {
        uswitch_call_dynamic(ctx, evhttp_send_error_s, req, HTTP_INTERNAL, nullptr);
        delete[] input;
        return;
    }
    struct evbuffer *evb;
    uswitch_call_dynamic(ctx, evbuffer_new_s, evb);
    uswitch_call_dynamic(ctx, evbuffer_add_reference_s, nullptr, evb, input, size,
        (evbuffer_ref_cleanup_cb)sandbox->get_symbol_addr("free"), input);
    uswitch_call_dynamic(ctx, evhttp_send_reply_s, req, 200, sandbox_strings["OK"], evb);
    uswitch_call_dynamic(ctx, evbuffer_free_s, evb);
}

static void
print_usage(FILE *out, const char *prog, int exit_code)
{
    fprintf(out,
        "Syntax: %s [ OPTS ] <docroot>\n"
        " -p      - port\n"
        " -H      - address to bind (default: 0.0.0.0)\n"
        " -u      - unlink unix socket before bind\n"
        " -m      - max body size\n"
        " -v      - verbosity, enables libevent debug logging too\n", prog);
    exit(exit_code);
}
static struct options
parse_opts(int argc, char **argv)
{
    struct options o;
    int opt;

    memset(&o, 0, sizeof(o));

    while ((opt = getopt(argc, argv, "hp:m:vH:")) != -1) {
        switch (opt) {
            case 'p': o.port = atoi(optarg); break;
            case 'm': o.max_body_size = atoi(optarg); break;
            case 'v': ++o.verbose; break;
            case 'H': o.bind = optarg; break;
            case 'h': print_usage(stdout, argv[0], 0); break;
            default : fprintf(stderr, "Unknown option %c\n", opt); break;
        }
    }

    if (optind >= argc || (argc - optind) > 1) {
        print_usage(stdout, argv[0], 1);
    }
    o.docroot = argv[optind];

    return o;
}

int
main(int argc, char **argv)
{
    struct event_config *cfg = NULL;
    struct event_base *base = NULL;
    struct evhttp *http = NULL;
    struct evhttp_bound_socket *handle = NULL;
    struct evconnlistener *lev = NULL;
    struct options o = parse_opts(argc, argv);

    USwitchSandbox sandbox("../libraries_uswitch/libevent/libevent.so", 8192l << 20, 2l << 20);
    sandbox.init();
    static const std::vector<unsigned int> AllowedSyscalls {
        __NR_readv, __NR_writev, __NR_close,
        __NR_epoll_pwait, __NR_ioctl,
        __NR_clock_gettime,
        __NR_socket, __NR_connect, __NR_shutdown,
        __NR_bind, __NR_listen, __NR_getsockname,
        __NR_setsockopt,
        __NR_exit, __NR_futex, __NR_sched_yield,
        __NR_epoll_create1, __NR_epoll_ctl,
        __NR_accept4, __NR_pipe2,
#ifdef ONLYMEMPROT
        __NR_rt_sigaction, __NR_openat, __NR_lseek, __NR_read, __NR_newfstatat,
#endif
        451};
    sandbox.init_seccomp(AllowedSyscalls);
    load_sandbox_strings(&sandbox);
    uswctx_t ctx = sandbox.get_context();
    SET_FUNC_PTR(evhttp_uri_parse);
    SET_FUNC_PTR(evhttp_uri_get_path);
    SET_FUNC_PTR(evhttp_uri_free);
    SET_FUNC_PTR(evhttp_request_get_uri);
    SET_FUNC_PTR(evhttp_send_error);
    SET_FUNC_PTR(evhttp_add_header);
    SET_FUNC_PTR(evhttp_request_get_output_headers);
    SET_FUNC_PTR(evbuffer_new);
    SET_FUNC_PTR(evbuffer_add_reference);
    SET_FUNC_PTR(evhttp_send_reply);
    SET_FUNC_PTR(evbuffer_free);
    SET_FUNC_PTR(event_enable_debug_logging);
    SET_FUNC_PTR(event_config_new);
    SET_FUNC_PTR(event_base_new_with_config);
    SET_FUNC_PTR(event_config_free);
    SET_FUNC_PTR(evhttp_new);
    SET_FUNC_PTR(evhttp_set_gencb);
    SET_FUNC_PTR(evhttp_set_max_body_size);
    SET_FUNC_PTR(evhttp_bind_socket_with_handle);
    SET_FUNC_PTR(event_base_dispatch);

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        return 1;
    }

    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    /** Read env like in regress */
    if (o.verbose || getenv("EVENT_DEBUG_LOGGING_ALL"))
        uswitch_call_dynamic(ctx, event_enable_debug_logging_s, EVENT_DBG_ALL);

    uswitch_call_dynamic(ctx, event_config_new_s, cfg);

    uswitch_call_dynamic(ctx, event_base_new_with_config_s, base, cfg);
    if (!base) {
        fprintf(stderr, "Couldn't create an event_base: exiting\n");
        return 1;
    }
    uswitch_call_dynamic(ctx, event_config_free_s, cfg);
    cfg = NULL;

    uswitch_call_dynamic(ctx, evhttp_new_s, http, base);
    if (!http) {
        fprintf(stderr, "couldn't create evhttp. Exiting.\n");
        return -1;
    }
    sandbox.custom_data = (void *)&o;

    auto send_document_cb_s = uswitch_register_callback_get_fp(16, ctx, (void *)&sandbox, send_document_cb);
    uswitch_call_dynamic(ctx, evhttp_set_gencb_s, http, send_document_cb_s, nullptr);
    if (o.max_body_size)
        uswitch_call_dynamic(ctx, evhttp_set_max_body_size_s, http, o.max_body_size);

    char *bind_s = nullptr;
    if (o.bind) {
        size_t len = strlen(o.bind) + 1;
        bind_s = (char *)sandbox.malloc_in_sandbox(len);
        memcpy(bind_s, o.bind, len);
    }
    uswitch_call_dynamic(ctx, evhttp_bind_socket_with_handle_s, handle, http, bind_s, o.port);
    if (!handle) {
        fprintf(stderr, "couldn't bind to %s:%d. Exiting.\n", o.bind, o.port);
        return -1;
    }

    uswitch_call_dynamic(ctx, event_base_dispatch_s, nullptr, base);
    return 0;
}