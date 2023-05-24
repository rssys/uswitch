#include <string>
#include <fstream>
#include <vector>
#include <unordered_map>
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

#define USE_LIBEVENT
#include "ProcessSandbox.h"

static const char *StringLiterals[] = {
    "Content-Type",
    "text/plain",
    "OK",
    "free"
};
std::unordered_map<const char *, char *> sandbox_strings;
void *free_symbol;

static void load_sandbox_strings(EVENTProcessSandbox *sandbox) {
    for (const char *s : StringLiterals) {
        size_t len = strlen(s) + 1;
        char *buf = (char *)sandbox->mallocInSandbox(len);
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

static bool get_path(EVENTProcessSandbox *sandbox, const char *uri, struct options *o, std::string &res) {
    struct evhttp_uri *decoded = sandbox->inv_evhttp_uri_parse(uri);
    if (!decoded) {
        return false;
    }
    std::string path;
    if (const char *p = sandbox->inv_evhttp_uri_get_path(decoded); p) {
        path = p;
    } else {
        path = "/";
    }
    sandbox->inv_evhttp_uri_free(decoded);
    res = o->docroot + path;
    return true;
}

struct Data {
    EVENTProcessSandbox *sandbox;
    struct options *options;
};

static void
send_document_cb(struct evhttp_request *req, void *arg, void *data)
{
    EVENTProcessSandbox *sandbox = ((Data *)data)->sandbox;
    struct options *o = ((Data *)data)->options;
    const char *uri = sandbox->inv_evhttp_request_get_uri(req);
    std::string path;
    if (!get_path(sandbox, uri, o, path)) {
        sandbox->inv_evhttp_send_error(req, HTTP_BADREQUEST, nullptr);
        return;
    }

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        sandbox->inv_evhttp_send_error(req, HTTP_NOTFOUND, nullptr);
        return;
    }
    evkeyvalq *headers = sandbox->inv_evhttp_request_get_output_headers(req);
    sandbox->inv_evhttp_add_header(headers,
        sandbox_strings["Content-Type"], sandbox_strings["text/plain"]);
    
    ifs.seekg(0, std::ios_base::end);
    size_t size = ifs.tellg();
    ifs.seekg(0, std::ios_base::beg);
    uint8_t *input = (uint8_t *)sandbox->mallocInSandbox(size);
    if (!ifs.read((char *)input, size)) {
        sandbox->inv_evhttp_send_error(req, HTTP_INTERNAL, nullptr);
        delete[] input;
        return;
    }
    struct evbuffer *evb = sandbox->inv_evbuffer_new();
    sandbox->inv_evbuffer_add_reference(evb, input, size, (evbuffer_ref_cleanup_cb)free_symbol, input);
    sandbox->inv_evhttp_send_reply(req, 200, sandbox_strings["OK"], evb);
    sandbox->inv_evbuffer_free(evb);
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

    EVENTProcessSandbox sandbox("./ProcessSandbox/ProcessSandbox_otherside_event64", 9999, 0);
    load_sandbox_strings(&sandbox);
    free_symbol = sandbox.inv_invokeDlSym(sandbox_strings["free"]);

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        return 1;
    }

    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    /** Read env like in regress */
    if (o.verbose || getenv("EVENT_DEBUG_LOGGING_ALL"))
        sandbox.inv_event_enable_debug_logging(EVENT_DBG_ALL);

    cfg = sandbox.inv_event_config_new();

    base = sandbox.inv_event_base_new_with_config(cfg);
    if (!base) {
        fprintf(stderr, "Couldn't create an event_base: exiting\n");
        return 1;
    }
    sandbox.inv_event_config_free(cfg);
    cfg = NULL;

    http = sandbox.inv_evhttp_new(base);
    if (!http) {
        fprintf(stderr, "couldn't create evhttp. Exiting.\n");
        return -1;
    }
    Data data;
    data.sandbox = &sandbox;
    data.options = &o;

    void (*send_document_cb_s)(struct evhttp_request *req, void *arg);
    send_document_cb_s = sandbox.registerCallback((decltype(send_document_cb_s))send_document_cb, &data);
    sandbox.inv_evhttp_set_gencb(http, send_document_cb_s, nullptr);
    if (o.max_body_size)
        sandbox.inv_evhttp_set_max_body_size(http, o.max_body_size);

    char *bind_s = nullptr;
    if (o.bind) {
        size_t len = strlen(o.bind) + 1;
        bind_s = (char *)sandbox.mallocInSandbox(len);
        memcpy(bind_s, o.bind, len);
    }
    handle = sandbox.inv_evhttp_bind_socket_with_handle(http, bind_s, o.port);
    if (!handle) {
        fprintf(stderr, "couldn't bind to %s:%d. Exiting.\n", o.bind, o.port);
        return -1;
    }

    sandbox.inv_event_base_dispatch(base);
    return 0;
}