package pbcodec

import (
	"errors"
	"io"
	"log"
	"net/rpc"
	"sync"

	"github.com/golang/protobuf/proto"
)

type serverCodec struct {
	rwc io.ReadWriteCloser
	m   sync.Mutex
	req *PbRpcRequest
}

func NewServerCodec(conn io.ReadWriteCloser) rpc.ServerCodec {
	return &serverCodec{rwc: conn}
}

func (c *serverCodec) ReadRequestHeader(r *rpc.Request) error {
	data, err := ReadRpc(c.rwc)
	if err != nil {
		return err
	}
	req := &PbRpcRequest{}
	err = proto.Unmarshal(data, req)
	if err != nil {
		return err
	}

	c.req = req

	r.Seq = req.GetId()
	r.ServiceMethod = req.GetMethod()

	return nil
}

func (c *serverCodec) ReadRequestBody(params interface{}) error {
	if params == nil {
		return nil
	}

	protoParams, ok := params.(proto.Message)
	if !ok {
		return errors.New("Invalid type given")
	}

	return proto.Unmarshal(c.req.GetParams(), protoParams)
}

func (c *serverCodec) WriteResponse(r *rpc.Response, result interface{}) error {
	log.Print("in write repsonse")
	rsp := &PbRpcResponse{Id: &r.Seq}

	if r.Error != "" {
		rsp.Error = &r.Error
	} else {
		protoResult, ok := result.(proto.Message)
		if !ok {
			return errors.New("Invalid type given")
		}
		data, err := proto.Marshal(protoResult)
		if err != nil {
			return err
		}
		rsp.Result = data
	}

	data, err := proto.Marshal(rsp)
	if err != nil {
		return err
	}
	_, err = WriteRpc(c.rwc, data)

	return err
}

func (c *serverCodec) Close() error {
	return c.rwc.Close()
}
