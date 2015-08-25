#include "calc.pb-c.h"
#include "../rpc.h"
#include "../rpcclnt.h"

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
        rpcclnt *clnt = arg;

        rpcclnt_mainloop (clnt);

        return NULL;
}


int
calc_cbk (rpcclnt *clnt, ProtobufCBinaryData *msg, int ret)
{
        if (ret)
                return ret;

        Calc__CalcRsp *crsp = calc__calc_rsp__unpack (NULL, msg->len,
                                                      msg->data);

        printf("rsp->sum = %d\n", crsp->ret);
        calc__calc_rsp__free_unpacked (crsp, NULL);
}

int main(int argc, char **argv)
{
        int ret;
        rpcclnt *clnt = NULL;

        clnt = rpcclnt_new ("localhost", 9876);
        if (!clnt) {
                fprintf (stderr, "Failed to create an rpcclnt object\n");
                return 1;
        }

        pthread_t tid;
        ret = pthread_create (&tid, NULL, mainloop, clnt);
        if (ret) {
                rpcclnt_destroy (clnt);
                return ret;
        }

        Calc__CalcReq calc = CALC__CALC_REQ__INIT;
        calc.op = 1; calc.a = 2; calc.b = 3;
        size_t clen = calc__calc_req__get_packed_size(&calc);
        char *cbuf = calloc (1, clen);
        calc__calc_req__pack(&calc, cbuf);

        ProtobufCBinaryData msg;
        msg.len = clen;
        msg.data = cbuf;

        ret = rpcclnt_call (clnt, "calculate", &msg, calc_cbk);
        if (ret) {
                fprintf (stderr, "RPC call failed\n");
        }

        pthread_join (tid, NULL);
        return 0;
}



