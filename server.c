#include "rpc.h"
#include "calc.pb-c.h"
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <arpa/inet.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define DBUG(fp, x) do {                        \
        fprintf(fp, "%s was called\n", # x);    \
        fflush(fp);                             \
} while (0)

static void
read_cb(struct bufferevent *bev, void *ctx)
{
        rpcsvc *svc = ctx;
        /* This callback is invoked when there is data to read on bev. */
        struct evbuffer *input = bufferevent_get_input(bev);
        struct evbuffer *output = bufferevent_get_output(bev);

        /* Copy all the data from the input buffer to the output buffer. */
        char buf[4096] = {0};
        size_t read = bufferevent_read (bev, buf, sizeof (buf));
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

                bufferevent_write (bev, outbuf, ret);
                free(outbuf);
        }

}

static void
event_cb(struct bufferevent *bev, short events, void *ctx)
{
        if (events & BEV_EVENT_ERROR)
                perror("Error from bufferevent");
        if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
                bufferevent_free(bev);
        }
}

static int
calculate (ProtobufCBinaryData *req, ProtobufCBinaryData *reply)
{
        Calc__CalcReq *creq;
        Calc__CalcRsp crsp = CALC__CALC_RSP__INIT;
        size_t rsplen;
        int ret = 0;

        creq = calc__calc_req__unpack (NULL, req->len, req->data);

        /* method-specific logic */
        switch(creq->op) {
        default:
                crsp.ret = creq->a + creq->b;
        }

        rsplen = calc__calc_rsp__get_packed_size(&crsp);
        reply->data = calloc (1, rsplen);
        if (!reply->data) {
                ret = -1;
                goto out;
        }

        reply->len = rsplen;
        calc__calc_rsp__pack(&crsp, reply->data);
out:
        calc__calc_req__free_unpacked (creq, NULL);

        return 0;
}


int
main(int argc, char **argv)
{
        struct event_base *base;
        struct evconnlistener *listener;
        struct sockaddr_in sin;
        rpcsvc_fn_obj tbl[] = {
                {calculate, "calculate"},
                NULL
        };

        int ret = -1;
        int port = 9876;

        if (argc > 1) {
                port = atoi(argv[1]);
        }
        if (port<=0 || port>65535) {
                puts("Invalid port");
                return 1;
        }

        rpcsvc *svc = rpcsvc_new ("localhost", 9876, read_cb, event_cb);
        if (!svc) {
                fprintf (stderr, "Failed to create a new rpcsvc object");
                return 1;
        }

        ret = rpcsvc_register_methods (svc, tbl);
        if (ret) {
                fprintf (stderr, "Failed to register methods to rpcsvc.");
                return 1;
        }

        ret = rpcsvc_serve (svc);
        if (ret) {
                fprintf (stderr, "Failed to start libevent base loop for "
                         "rpcsvc listener.");
                return 1;
        }

        return 0;
}
