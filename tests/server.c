#include "../rpc.h"
#include "calc.pb-c.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define DBUG(fp, x) do {                        \
        fprintf(fp, "%s was called\n", # x);    \
        fflush(fp);                             \
} while (0)

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

        rpcsvc *svc = rpcsvc_new ("localhost", 9876);
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
