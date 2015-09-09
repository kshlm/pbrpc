#ifndef _RPC_H
#define _RPC_H

#include <event2/bufferevent.h>
#include <event2/event.h>

#include "pbrpc.pb-c.h"
#include "pbrpc-clnt.h"

/**
 * Rpcsvc method type
 */
typedef int (*pbrpc_svc_fn) (ProtobufCBinaryData *req,
                          ProtobufCBinaryData *rsp);
/**
 * Rpcsvc method entry
 */
struct pbrpc_svc_fn_obj {
        pbrpc_svc_fn fn;
        char *name;
};

typedef struct pbrpc_svc_fn_obj pbrpc_svc_fn_obj;

struct pbrpc_svc {
        struct evconnlistener *listener;
        void *ctx;
        /**
         * Callbacks for accepted connection
         * */
        bufferevent_data_cb reader;
        bufferevent_event_cb notifier;
        pbrpc_svc_fn_obj *methods;
};

typedef struct pbrpc_svc pbrpc_svc;


/**
 * Creates a new object en pbrpc based service listening at hostname on port
 *
 * @param hostname - a string representing the hostname or IP address of the
 *                 node
 *
 * @param port - the port on which the service needs to listening
 *
 * @return  - a pointer to a newly allocated pbrpc_svc object, or NULL if an error
 *            occurred
 *
 * */

pbrpc_svc*
pbrpc_svc_new (const char *hostname, int16_t port);


/**
 * Registers RPC methods with a pbrpc_svc object
 *
 * @param svc - the service object
 * @param methods - a list of functions that are exported via @pbrpc_svc
 * @return - 0 on success, -1 on failure
 *
 * */
int
pbrpc_svc_register_methods (pbrpc_svc *svc, pbrpc_svc_fn_obj *methods);


/**
 * Starts the event_base_loop (ref: libevent2) to listen for requests on @svc.
 * Blocks the current thread until pbrpc_svc_destroy is called
 *
 * @param svc - the service object
 * @return - 0 on success, -1 on failure
 *
 * */
int
pbrpc_svc_serve (pbrpc_svc *svc);


/**
 * Stops the @svc's event_base_loop and frees @svc.
 * @return - 0 on success, -1 on failure
 *
 * */
int
pbrpc_svc_destroy (pbrpc_svc* svc);


/* Rpcproto Method constants */
enum method_type {
        CALCULATE = 1
};

typedef int (*rpc_handler_func) (ProtobufCBinaryData *req,
                                 ProtobufCBinaryData *reply);


int
rpc_write_reply (pbrpc_svc *svc, Pbcodec__PbRpcResponse *rsphdr, char **buf);

int
rpc_invoke_call (pbrpc_svc *svc, Pbcodec__PbRpcRequest *reqhdr,
                 Pbcodec__PbRpcResponse *rsphdr);

#endif
