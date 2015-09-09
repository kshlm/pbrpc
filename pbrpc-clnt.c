#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include "pbrpc-clnt.h"

static Pbcodec__PbRpcResponse *
rpc_read_rsp (const char *msg, uint64_t *bytes_read);

static uint64_t
next_id (void)
{
        static uint64_t id_gen = 0;
        static pthread_mutex_t id_lock = PTHREAD_MUTEX_INITIALIZER;

        uint64_t id = 0;

        pthread_mutex_lock (&id_lock);
        {
                id = ++id_gen;
        }
        pthread_mutex_unlock (&id_lock);

        return id;
}


static int
process_response (void *handle, struct bufferevent *bev, char *msg)
{
        struct cb_closure *cls = handle;
        pbrpc_clnt *clnt = NULL;
        uint64_t bytes_read = 0;
        Pbcodec__PbRpcResponse *rsp = NULL;

        clnt = list_entry (cls, pbrpc_clnt, closure);

        rsp = rpc_read_rsp (msg, &bytes_read);
        if (!rsp) {
                fprintf (stderr, "Failed to parse response "
                         "from server\n");
                return bytes_read;
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
        return bytes_read;
}


int
pbrpc_clnt_destroy (pbrpc_clnt *clnt)
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


pbrpc_clnt *
pbrpc_clnt_new (const char *host, int16_t port)
{
        pbrpc_clnt *clnt = NULL;
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

        bufferevent_setcb (filtered_bev, generic_read_cb, NULL,
                           generic_event_cb, &clnt->closure);
        bufferevent_enable (filtered_bev, EV_READ|EV_WRITE);

        INIT_LIST_HEAD (&clnt->outstanding);
        clnt->bev = filtered_bev;
        clnt->closure.fn = process_response;
        return clnt;
err:
        if (bev)
                bufferevent_free (bev);

        if (base)
                event_base_free (base);

        return NULL;
}


int
pbrpc_clnt_mainloop (pbrpc_clnt *clnt)
{
        struct event_base *base = bufferevent_get_base (clnt->bev);

        if (!base)
                return -1;

        return event_base_dispatch(base);
}


int
pbrpc_clnt_call (pbrpc_clnt *clnt, const char *method,
                 ProtobufCBinaryData *msg, pbrpc_clnt_cbk cbk)
{
        Pbcodec__PbRpcRequest reqhdr = PBCODEC__PB_RPC_REQUEST__INIT;
        int       ret = -1;
        size_t    rlen = 0;
        char     *rbuf = NULL;
        char     *mth  = NULL;

        mth = strdup (method);
        if (!mth)
                return -1;

        reqhdr.id = next_id();
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


static Pbcodec__PbRpcResponse *
rpc_read_rsp (const char *msg, uint64_t *bytes_read)
{
        char *hdr;
        uint64_t proto_len = 0;

        memcpy (&proto_len, msg, sizeof(uint64_t));
        proto_len = be64toh (proto_len);
        *bytes_read = proto_len + sizeof (proto_len);

        return pbcodec__pb_rpc_response__unpack(NULL, proto_len, msg+sizeof(proto_len));
}


int
rpc_write_request (pbrpc_clnt *clnt, Pbcodec__PbRpcRequest *reqhdr, char **buf)
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
