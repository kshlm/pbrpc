#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <endian.h>
#include <errno.h>
#include <arpa/inet.h>

#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include "rpc.h"
#include "pbrpc.pb-c.h"
#include "calc.pb-c.h"


static void
svc_read_cb(struct bufferevent *bev, void *ctx)
{
        rpcsvc          *svc  = ctx;
        char            *buf  = NULL;
        struct evbuffer *in   = NULL;
        size_t           len  = 0;
        size_t           read = 0;

        in = bufferevent_get_input (bev);
        len = evbuffer_get_length (in);
        buf = calloc (1, len);
        if (!buf) {
                fprintf (stderr, "Failed to allocate memory\n");
                return;
        }
        read = bufferevent_read (bev, buf, len);
        if (read < 0) {
                perror("bufferevent_read failed");

        } else {
                int ret;

                Pbcodec__PbRpcRequest *reqhdr = rpc_read_req (svc, buf, read);
                Pbcodec__PbRpcResponse rsphdr = PBCODEC__PB_RPC_RESPONSE__INIT;
                ret = rpc_invoke_call (svc, reqhdr, &rsphdr);
                if (ret) {
                        fprintf(stderr, "ret = %d: rpc_invoke_call failed\n", ret);
                }
                char *outbuf = NULL;
                ret = rpc_write_reply (svc, &rsphdr, &outbuf);
                if (ret <= 0) {
                        fprintf(stderr, "ret = %d: rpc_write_reply failed\n", ret);
                }

                pbcodec__pb_rpc_request__free_unpacked (reqhdr, NULL);
                bufferevent_write (bev, outbuf, ret);
                free(outbuf);
        }
        free (buf);
}


static void
svc_event_cb(struct bufferevent *bev, short events, void *ctx)
{
        if (events & BEV_EVENT_ERROR)
                perror("Error from bufferevent");
        if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
                bufferevent_free(bev);
        }
}


static enum bufferevent_filter_result
filter_pbrpc_messages (struct evbuffer *src,
                struct evbuffer *dst, ev_ssize_t dst_limit,
                enum bufferevent_flush_mode mode, void *ctx)
{
        uint64_t message_len = 0;
        size_t buf_len = 0;
        size_t expected_len = 0;
        size_t tmp = 0;
        unsigned char *lenbuf = NULL;

        /**
         * Read the message len evbuffer_pullup doesn't drain the buffer. It
         * just makes sure that given number of bytes are contiguous, which
         * allows us to easily copy out stuff.
         */
        lenbuf = evbuffer_pullup (src, sizeof(message_len)); if (!lenbuf)
                return BEV_ERROR;

        memcpy (&message_len, lenbuf, sizeof(message_len));

        message_len = be64toh (message_len);

        expected_len = sizeof (message_len) + message_len;

        buf_len = evbuffer_get_length (src);

        if (buf_len < expected_len)
                return BEV_NEED_MORE;

        tmp = evbuffer_remove_buffer
                (src, dst, (sizeof(message_len) + message_len));
        if (tmp != expected_len)
                return BEV_ERROR;

        return BEV_OK;
}


static void
accept_conn_cb(struct evconnlistener *listener,
    evutil_socket_t fd, struct sockaddr *address, int socklen,
    void *ctx)
{
        rpcsvc *svc = ctx;

        /* We got a new connection! Set up a bufferevent for it. */
        struct event_base *base = evconnlistener_get_base(listener);
        struct bufferevent *bev = bufferevent_socket_new(
                base, fd, BEV_OPT_CLOSE_ON_FREE);

        struct bufferevent *filtered_bev = bufferevent_filter_new (
                        bev, filter_pbrpc_messages, NULL, BEV_OPT_CLOSE_ON_FREE,
                        NULL, NULL);

        bufferevent_setcb(filtered_bev, svc->reader, NULL, svc->notifier, svc);
        bufferevent_enable(filtered_bev, EV_READ|EV_WRITE);
}


static void
accept_error_cb(struct evconnlistener *listener, void *ctx)
{
        int err = EVUTIL_SOCKET_ERROR();
        fprintf(stderr, "Got an error %d (%s) on the listener. "
                "Shutting down.\n", err, evutil_socket_error_to_string(err));
        fflush(stderr);
        rpcsvc_destroy ((rpcsvc*) ctx);
}


int
rpcsvc_register_methods (rpcsvc *svc, rpcsvc_fn_obj *methods)
{
        svc->methods = methods;
        return 0;
}


int
rpcsvc_destroy (rpcsvc *svc)
{
        struct event_base *base = evconnlistener_get_base(svc->listener);

        event_base_loopexit(base, NULL);
        event_base_free (base);
        free (svc);
        return 0;
}


static void
rpcsvc_init (rpcsvc *svc, struct evconnlistener *listener,
             bufferevent_data_cb reader, bufferevent_event_cb notifier)
{
        svc->listener = listener;
        svc->reader   = reader;
        svc->notifier = notifier;
}


rpcsvc*
rpcsvc_new (const char *name, int16_t port)
{
        struct event_base *base = NULL;
        struct sockaddr_in sin;
        struct evconnlistener *listener = NULL;

        rpcsvc *new = calloc (1, sizeof (*new));
        if (!new)
                return NULL;

        base = event_base_new();
        if (!base)
                goto err;

        /* Clear the sockaddr before using it, in case there are extra
         * platform-specific fields that can mess us up. */
        memset(&sin, 0, sizeof(sin));
        /* This is an INET address */
        sin.sin_family = AF_INET;
        /* Listen on 0.0.0.0 */
        //FIXME: @name is ignored. Use getaddrinfo(3) to fill in_addr appropriately.
        sin.sin_addr.s_addr = htonl(0);
        /* Listen on the given port. */
        sin.sin_port = htons(port);

        listener = evconnlistener_new_bind(base, accept_conn_cb, new,
            LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
            (struct sockaddr*)&sin, sizeof(sin));
        if (!listener) {
                perror("Couldn't create listener");
                goto err;
        }
        evconnlistener_set_error_cb(listener, accept_error_cb);

        rpcsvc_init (new, listener, svc_read_cb, svc_event_cb);

        return new;
err:
        free (new);
        return NULL;
}


int
rpcsvc_serve (rpcsvc *svc)
{
        struct evconnlistener *listener = svc->listener;
        struct event_base *base = evconnlistener_get_base (listener);

        return event_base_dispatch (base);
}


/*
 * RPC Format
 * -----------------------------------------------------
 * | Length: uint64                                    |
 * -----------------------------------------------------
 * |                                                   |
 * | Header:  Id uint64, Method int32, Params []bytes |
 * |                                                   |
 * -----------------------------------------------------
 * |                                                   |
 * | Body: Method dependent representation             |
 * |                                                   |
 * -----------------------------------------------------
 *
 **/

/*
 * pseudo code:
 *
 * read buffer from network
 * reqhdr = rpc_read_req (svc, msg, len);
 * rsphdr = RSP_HEADER__INIT;
 * ret = rpc_invoke_call (svc, reqhdr, &rsphdr)
 * if (ret)
 *   goto out;
 * char *buf = NULL;
 * ret = rpc_write_reply (svc, &rsphdr, &buf);
 * if (ret != -1)
 *   goto out;
 *
 * send buf over n/w
 *
 * free(buf);
 * free(rsp->result->data);
 *
 *
 * ...
 * clean up protobuf buffers on finish
 *
 * */

/* returns the no. of bytes written to @buf
 * @buf is allocated by this function.
 * */
int
rpc_write_request (rpcclnt *clnt, Pbcodec__PbRpcRequest *reqhdr, char **buf)
{
        uint64_t be_len = 0;
        if (!buf)
                return -1;

        size_t reqlen = pbcodec__pb_rpc_request__get_packed_size (reqhdr);
        *buf = calloc (1, reqlen+8);
        if (!*buf)
                return -1;

        pbcodec__pb_rpc_request__pack(reqhdr, *buf+8);
        be_len = htobe64 (reqlen);
        memcpy(*buf, &be_len, sizeof(uint64_t));
        bufferevent_write(clnt->bev, *buf, reqlen + sizeof (uint64_t));

        return reqlen + sizeof(uint64_t);
}


/* returns the no. of bytes written to @buf
 * @buf is allocated by this function.
 * */
int
rpc_write_reply (rpcsvc *svc, Pbcodec__PbRpcResponse *rsphdr, char **buf)
{
        uint64_t be_len = 0;
        if (!buf)
                return -1;

        size_t rsplen = pbcodec__pb_rpc_response__get_packed_size (rsphdr);
        *buf = calloc (1, rsplen+sizeof(uint64_t));
        if (!*buf)
                return -1;

        be_len = htobe64 (rsplen);
        pbcodec__pb_rpc_response__pack (rsphdr, *buf + sizeof(uint64_t));
        memcpy (*buf, &be_len, sizeof(be_len));
        return rsplen+sizeof(uint64_t);
}

int
rpc_invoke_call (rpcsvc *svc, Pbcodec__PbRpcRequest *reqhdr,
                 Pbcodec__PbRpcResponse *rsphdr)
{
        int ret;

        if (!reqhdr->has_params){
                fprintf(stderr, "no params passed\n");
                return -1;
        }
        rsphdr->id = reqhdr->id;

        rpcsvc_fn_obj *method;
        for (method = svc->methods; method; method++)
                if (!strcmp (method->name, reqhdr->method))
                        break;

        if (!method)
                return -1;

        ret = method->fn (&reqhdr->params, &rsphdr->result);

        return ret;
}

Pbcodec__PbRpcResponse *
rpc_read_rsp (const char *msg, size_t msg_len)
{
        char *hdr;
        uint64_t proto_len = 0;

        memcpy (&proto_len, msg, sizeof(uint64_t));
        proto_len = be64toh (proto_len);

        return pbcodec__pb_rpc_response__unpack(NULL, proto_len, msg+sizeof(proto_len));
}

Pbcodec__PbRpcRequest *
rpc_read_req (rpcsvc *svc, const char* msg, size_t msg_len)
{
        char *hdr;
        uint64_t proto_len = 0;

        memcpy (&proto_len, msg, sizeof(uint64_t));
        proto_len = be64toh (proto_len);
        return pbcodec__pb_rpc_request__unpack (NULL, proto_len, msg+sizeof(proto_len));
}


void
clnt_read_cb (struct bufferevent *bev, void *ctx)
{
        rpcclnt         *clnt = ctx;
        char            *buf  = NULL;
        struct evbuffer *in   = NULL;
        size_t           len  = 0;
        size_t           read = 0;
        Pbcodec__PbRpcResponse *rsp = NULL;

        in = bufferevent_get_input (bev);
        len = evbuffer_get_length (in);
        buf = calloc (1, len);
        if (!buf) {
                fprintf (stderr, "Failed to allocate memory\n");
                return;
        }
        read = bufferevent_read (bev, buf, len);
        if (read < 0) {
                perror("bufferevent_read failed");
                goto out;

        }
        int ret;
        rsp = rpc_read_rsp (buf, read);
        if (!rsp) {
                fprintf (stderr, "Failed to parse response "
                         "from server\n");
                return;
        }

        struct saved_req *call, *tmp;
        char found = 0;

        list_for_each_entry_safe (call, tmp, &clnt->outstanding, list) {
                if (call->id == rsp->id) {
                        list_del_init (&call->list);
                        found = 1;
                        break;
                }

        }
        if (!found) {
                fprintf (stderr, "Failed to find the corresponding "
                         "pending call request\n");
                goto out;
        }

        call->cbk (clnt, &rsp->result, 0);
        free (call);

out:
        pbcodec__pb_rpc_response__free_unpacked (rsp, NULL);
        free (buf);
}


void
clnt_event_cb (struct bufferevent *bev, short events, void *ctx)
{
        if (events & BEV_EVENT_ERROR)
                perror("Error from bufferevent");
        if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
                bufferevent_free(bev);
        }
}


int
rpcclnt_destroy (rpcclnt *clnt)
{
        struct event_base *base;
        struct bufferevent *bev;

        bev = clnt->bev;
        clnt->bev = NULL;

        base = bufferevent_get_base (bev);
        event_base_loopexit (base, NULL);
        event_base_free (base);
        bufferevent_free (bev);
        free (clnt);

        return 0;
}


rpcclnt *
rpcclnt_new (const char *host, int16_t port)
{
        rpcclnt *clnt = NULL;
        struct event_base *base;
        int ret;

        clnt = calloc (1, sizeof (*clnt));
        if (!clnt)
                return NULL;

        base = event_base_new ();
        if (!base) {
                fprintf(stderr, "failed to create event base\n");
                goto err;
        }

        struct bufferevent *bev = bufferevent_socket_new (base, -1,
                                                          BEV_OPT_CLOSE_ON_FREE);
        if (!bev) {
                fprintf(stderr, "failed to create bufferevent\n");
                goto err;
        }
        //FIXME: connect to @host supplied
        ret = bufferevent_socket_connect_hostname (bev, NULL, AF_INET,
                                                   "localhost", 9876);
        if (ret) {
                perror("connect failed");
                goto err;
        }

        struct bufferevent *filtered_bev = bufferevent_filter_new (
                        bev, filter_pbrpc_messages, NULL, BEV_OPT_CLOSE_ON_FREE,
                        NULL, NULL);

        bufferevent_setcb (filtered_bev, clnt_read_cb, NULL, clnt_event_cb, clnt);
        bufferevent_enable (filtered_bev, EV_READ|EV_WRITE);

        INIT_LIST_HEAD (&clnt->outstanding);
        clnt->bev = filtered_bev;
        return clnt;
err:
        if (bev)
                bufferevent_free (bev);

        if (base)
                event_base_free (base);

        return NULL;
}


int
rpcclnt_mainloop (rpcclnt *clnt)
{
        struct event_base *base = bufferevent_get_base (clnt->bev);

        if (!base)
                return -1;

        return event_base_dispatch(base);
}


int
rpcclnt_call (rpcclnt *clnt, const char *method, ProtobufCBinaryData *msg,
              rpcclnt_cbk cbk)
{
        Pbcodec__PbRpcRequest reqhdr = PBCODEC__PB_RPC_REQUEST__INIT;
        int       ret = -1;
        size_t    rlen = 0;
        char     *rbuf = NULL;
        char     *mth  = NULL;

        mth = strdup (method);
        if (!mth)
                return -1;

        reqhdr.id = 1;
        reqhdr.has_params = 1;
        reqhdr.params.data = msg->data ;
        reqhdr.params.len = msg->len;
        reqhdr.method = mth;

        struct saved_req *entry = calloc (1, sizeof (*entry));
        if (!entry) {
                return -1;
        }

        list_add_tail (&entry->list, &clnt->outstanding);
        entry->id = reqhdr.id;
        entry->cbk = cbk;

        rlen = rpc_write_request (clnt, &reqhdr, &rbuf);

        free (mth);
        free (rbuf);
        return 0;
}
