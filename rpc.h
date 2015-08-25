#ifndef _RPC_H
#define _RPC_H

#include <event2/bufferevent.h>
#include <event2/event.h>

#include "pbrpc.pb-c.h"
#include "rpcclnt.h"

/**
 * Rpcsvc method type
 */
typedef int (*rpcsvc_fn) (ProtobufCBinaryData *req,
                          ProtobufCBinaryData *rsp);
/**
 * Rpcsvc method entry
 */
struct rpcsvc_fn_obj {
        rpcsvc_fn fn;
        char *name;
};

typedef struct rpcsvc_fn_obj rpcsvc_fn_obj;

struct rpcsvc {
        struct evconnlistener *listener;
        void *ctx;
        /**
         * Callbacks for accepted connection
         * */
        bufferevent_data_cb reader;
        bufferevent_event_cb notifier;
        rpcsvc_fn_obj *methods;
};

typedef struct rpcsvc rpcsvc;


/**
 * Creates a new object en pbrpc based service listening at hostname on port
 *
 * @param hostname - a string representing the hostname or IP address of the
 *                 node
 *
 * @param port - the port on which the service needs to listening
 *
 * @return  - a pointer to a newly allocated rpcsvc object, or NULL if an error
 *            occurred
 *
 * */

rpcsvc*
rpcsvc_new (const char *hostname, int16_t port);


/**
 * Registers RPC methods with a rpcsvc object
 *
 * @param svc - the service object
 * @param methods - a list of functions that are exported via @rpcsvc
 * @return - 0 on success, -1 on failure
 *
 * */
int
rpcsvc_register_methods (rpcsvc *svc, rpcsvc_fn_obj *methods);


/**
 * Starts the event_base_loop (ref: libevent2) to listen for requests on @svc.
 * Blocks the current thread until rpcsvc_destroy is called
 *
 * @param svc - the service object
 * @return - 0 on success, -1 on failure
 *
 * */
int
rpcsvc_serve (rpcsvc *svc);


/**
 * Stops the @svc's event_base_loop and frees @svc.
 * @return - 0 on success, -1 on failure
 *
 * */
int
rpcsvc_destroy (rpcsvc* svc);


/* Rpcproto Method constants */
enum method_type {
        CALCULATE = 1
};

typedef int (*rpc_handler_func) (ProtobufCBinaryData *req,
                                 ProtobufCBinaryData *reply);

Pbcodec__PbRpcRequest *
rpc_read_req (rpcsvc *svc, const char* msg, size_t msg_len);

int
rpc_write_reply (rpcsvc *svc, Pbcodec__PbRpcResponse *rsphdr, char **buf);

int
rpc_invoke_call (rpcsvc *svc, Pbcodec__PbRpcRequest *reqhdr,
                 Pbcodec__PbRpcResponse *rsphdr);

#endif
