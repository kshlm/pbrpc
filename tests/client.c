#include "calc.pb-c.h"
#include "../pbrpc.h"
#include "../pbrpc-clnt.h"

#include <inttypes.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>


#define DBUG(x) fprintf (stdout, "%s was called\n", # x)

static void*
mainloop (void *arg)
{
        pbrpc_clnt *clnt = arg;

        pbrpc_clnt_mainloop (clnt);

        return NULL;
}


int
calc_cbk (pbrpc_clnt *clnt, ProtobufCBinaryData *msg, int ret)
{
        if (ret)
                return ret;

        Calc__CalcRsp *crsp = calc__calc_rsp__unpack (NULL, msg->len,
                                                      msg->data);

        printf("rsp->sum = %d\n", crsp->ret);
        calc__calc_rsp__free_unpacked (crsp, NULL);
}

static ProtobufCBinaryData
build_call_args (Calc__CalcReq *calc, int op, int a, int b)
{
        size_t clen;
        char *cbuf;
        ProtobufCBinaryData ret;

        calc->op = 1; calc->a = 2; calc->b = 3;
        clen = calc__calc_req__get_packed_size(calc);
        cbuf = calloc (1, clen);
        calc__calc_req__pack(calc, cbuf);
        ret.data = cbuf; ret.len = clen;
        return ret;
}


int main(int argc, char **argv)
{
        int ret;
        pbrpc_clnt *clnt = NULL;

        clnt = pbrpc_clnt_new ("localhost", 9876);
        if (!clnt) {
                fprintf (stderr, "Failed to create an pbrpc_clnt object\n");
                return 1;
        }

        Calc__CalcReq calc = CALC__CALC_REQ__INIT;
        int i = 1;
        do {
                fprintf (stdout, "call no %d\n", i);
                ProtobufCBinaryData msg  = build_call_args (&calc, 1, i, i+1);
                ret = pbrpc_clnt_call (clnt, "calculate", &msg, calc_cbk);
                if (ret) {
                        fprintf (stderr, "RPC call failed\n");
                }
                i++;

        } while (i <= 10);
        event_base_dispatch (bufferevent_get_base (clnt->bev));

        return 0;
}



