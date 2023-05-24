#include <string>
#include <fstream>
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

#include <event2/event.h>
#include <event2/http.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef ENABLE_SECCOMP
#include <vector>
#include <sys/syscall.h>
#include "seccomp-bpf.h"
bool init_seccomp(const std::vector<unsigned int> &allowed_syscalls) {
    std::vector<sock_filter> filter {
        VALIDATE_ARCHITECTURE,
        EXAMINE_SYSCALL
    };
    for (unsigned int s : allowed_syscalls) {
        sock_filter filter_allow[] = {ALLOW_SYSCALL_NUM(s)};
        for (int i = 0; i < sizeof(filter_allow) / sizeof(filter_allow[0]); ++i) {
            filter.push_back(filter_allow[i]);
        }
    }
    sock_filter filter_default = BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_KILL);
    filter.push_back(filter_default);
    struct sock_fprog prog = {
        .len = (unsigned short)filter.size(),
        .filter = filter.data(),
    };
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
        return false;
    }
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog)) {
        return false;
    }
    return true;
}
#endif

struct options {
    int port;
    int verbose;
    int max_body_size;

    const char *bind;
    const char *docroot;
};

static bool get_path(const char *uri, struct options *o, std::string &res) {
    struct evhttp_uri *decoded = evhttp_uri_parse(uri);
    if (!decoded) {
        return false;
    }
    std::string path;
    if (const char *p = evhttp_uri_get_path(decoded); p) {
        path = p;
    } else {
        path = "/";
    }
    evhttp_uri_free(decoded);
    res = o->docroot + path;
    return true;
}

static void
send_document_cb(struct evhttp_request *req, void *arg)
{
    struct options *o = (struct options *)arg;
    const char *uri = evhttp_request_get_uri(req);
    std::string path;
    if (!get_path(uri, o, path)) {
        evhttp_send_error(req, HTTP_BADREQUEST, 0);
        return;
    }

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        evhttp_send_error(req, HTTP_NOTFOUND, 0);
        return;
    }
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/plain");
    
    ifs.seekg(0, std::ios_base::end);
    size_t size = ifs.tellg();
    ifs.seekg(0, std::ios_base::beg);
    uint8_t *input = new uint8_t[size];
    if (!ifs.read((char *)input, size)) {
        evhttp_send_error(req, HTTP_INTERNAL, 0);
        delete[] input;
        return;
    }
    struct evbuffer *evb = evbuffer_new();
    evbuffer_add_reference(evb, input, size, [] (const void *data, size_t, void *) {
        delete[] (uint8_t *)data;
    }, input);
    evhttp_send_reply(req, 200, "OK", evb);
    evbuffer_free(evb);
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

#ifdef ENABLE_SECCOMP
    static const std::vector<unsigned int> AllowedSyscalls {
        __NR_readv, __NR_writev, __NR_close,
        __NR_epoll_wait, __NR_ioctl,
        __NR_clock_gettime,
        __NR_socket, __NR_connect, __NR_shutdown,
        __NR_bind, __NR_listen, __NR_getsockname,
        __NR_setsockopt,
        __NR_exit, __NR_futex, __NR_sched_yield,
        __NR_epoll_create1, __NR_epoll_ctl,
        __NR_accept4, __NR_pipe2,
        __NR_rt_sigaction, __NR_openat, __NR_lseek, __NR_read,
        __NR_getuid, __NR_getgid, __NR_geteuid, __NR_getegid,
        __NR_sendto, __NR_recvmsg, __NR_fstat, __NR_brk, __NR_mmap,
	    __NR_munmap, __NR_newfstatat
    };
    init_seccomp(AllowedSyscalls);
#endif

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        return 1;
    }

    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    /** Read env like in regress */
    if (o.verbose || getenv("EVENT_DEBUG_LOGGING_ALL"))
        event_enable_debug_logging(EVENT_DBG_ALL);

    cfg = event_config_new();

    base = event_base_new_with_config(cfg);
    if (!base) {
        fprintf(stderr, "Couldn't create an event_base: exiting\n");
        return 1;
    }
    event_config_free(cfg);
    cfg = NULL;

    http = evhttp_new(base);
    if (!http) {
        fprintf(stderr, "couldn't create evhttp. Exiting.\n");
        return 1;
    }

    evhttp_set_gencb(http, send_document_cb, &o);
    if (o.max_body_size)
        evhttp_set_max_body_size(http, o.max_body_size);

    handle = evhttp_bind_socket_with_handle(http, o.bind, o.port);
    if (!handle) {
        fprintf(stderr, "couldn't bind to %s:%d. Exiting.\n", o.bind, o.port);
        return -1;
    }

    event_base_dispatch(base);
    return 0;
}
