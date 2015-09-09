#ifndef _PBRPC_CLNT_H
#define _PBRPC_CLNT_H

#include <event2/bufferevent.h>
#include <event2/event.h>
#include <sys/queue.h>

#include "list.h"
#include "pbrpc.pb-c.h"
#include "pbrpc.h"


struct pbrpc_clnt {
        struct bufferevent *bev;
        struct list_head outstanding;
        struct cb_closure closure;
        void *ctx;
};

typedef struct pbrpc_clnt pbrpc_clnt;


/**
 * Rpcclnt callback
 */
typedef int (*pbrpc_clnt_cbk) (pbrpc_clnt *clnt, ProtobufCBinaryData *res, int ret);


/**
 * Rpc Pending call object
 */
struct saved_req {
        int32_t id;
        pbrpc_clnt_cbk cbk;
        struct list_head list;
};


/**
 *
 * Creates a new RPC client endpoint and connects to the service identified by
 * @host:@port
 *
 * @param host - a string representing the hostname or IP address of the RPC
 * server
 *
 * @param port - The port of the RPC service
 *
 * @return - a pointer to a newly allocated pbrpc_clnt object, or NULL if an error
 * occurred
 *
 */
pbrpc_clnt * pbrpc_clnt_new (const char *host, int16_t port);


/**
 * Stops the libevent mainloop, destroys the bufferevent associated with @clnt
 *
 * @param clnt - The RPC client endpoint
 *
 * @return 0 on success, -1 otherwise.
 */
int
pbrpc_clnt_destroy (pbrpc_clnt *clnt);


/**
 *
 * Starts the underlying libevent mainloop
 *
 * @param client object encapsulating the RPC client endpoint
 *
 * @return 0 on success, -1 otherwise
 */
int
pbrpc_clnt_mainloop (pbrpc_clnt *clnt);


/**
 *
 * Calls the remote service identified by @clnt
 *
 * @param clnt - pbrpc_clnt object representing the remote server
 *
 * @param method - string representation of the RPC method to be called.
 *
 * @param msg - protobuf-c encoded message containing RPC-specific payload
 *
 * @param cbk - callback called when the RPC call is responded to by RPC server
 *
 * @return - 0 on success and -1 otherwise
 */
int
pbrpc_clnt_call (pbrpc_clnt *clnt, const char *method, ProtobufCBinaryData *msg,
              pbrpc_clnt_cbk cbk);



int
rpc_write_request (pbrpc_clnt *clnt, Pbcodec__PbRpcRequest *reqhdr, char **buf);

#endif
