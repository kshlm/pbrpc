#ifndef _RPCCLNT_H
#define _RPCCLNT_H

#include <event2/bufferevent.h>
#include <event2/event.h>
#include <sys/queue.h>

#include "list.h"
#include "pbrpc.pb-c.h"


struct rpcclnt {
        struct bufferevent *bev;
        struct list_head outstanding;
        void *ctx;
};

typedef struct rpcclnt rpcclnt;


/**
 * Rpcclnt callback
 */
typedef int (*rpcclnt_cbk) (rpcclnt *clnt, ProtobufCBinaryData *res, int ret);


/**
 * Rpc Pending call object
 */
struct saved_req {
        int32_t id;
        rpcclnt_cbk cbk;
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
 * @return - a pointer to a newly allocated rpcclnt object, or NULL if an error
 * occurred
 *
 */
rpcclnt * rpcclnt_new (const char *host, int16_t port);


/**
 * Stops the libevent mainloop, destroys the bufferevent associated with @clnt
 *
 * @param clnt - The RPC client endpoint
 *
 * @return 0 on success, -1 otherwise.
 */
int
rpcclnt_destroy (rpcclnt *clnt);


/**
 *
 * Starts the underlying libevent mainloop
 *
 * @param client object encapsulating the RPC client endpoint
 *
 * @return 0 on success, -1 otherwise
 */
int
rpcclnt_mainloop (rpcclnt *clnt);


/**
 *
 * Calls the remote service identified by @clnt
 *
 * @param clnt - rpcclnt object representing the remote server
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
rpcclnt_call (rpcclnt *clnt, const char *method, ProtobufCBinaryData *msg,
              rpcclnt_cbk cbk);


Pbcodec__PbRpcResponse *
rpc_read_rsp (const char* msg, size_t msg_len);


int
rpc_write_request (rpcclnt *clnt, Pbcodec__PbRpcRequest *reqhdr, char **buf);

#endif
