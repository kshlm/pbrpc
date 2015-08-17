package pbcodec

import (
	"errors"
	"io"
	"log"
	"net/rpc"
	"sync"

	"github.com/golang/protobuf/proto"
)

type clientCodec struct {
	rwc io.ReadWriteCloser
	m   sync.Mutex
	rsp *PbRpcResponse
}

func NewClientCodec(conn io.ReadWriteCloser) rpc.ClientCodec {
	return &clientCodec{rwc: conn}
}

func (c *clientCodec) WriteRequest(r *rpc.Request, params interface{}) error {
	c.m.Lock()
	defer c.m.Unlock()

	req := &PbRpcRequest{Id: &r.Seq, Method: &r.ServiceMethod}

	protoParams, ok := params.(proto.Message)
	if !ok {
		e := errors.New("Invalid type given")
		log.Print(e)
		return e
	}

	data, err := proto.Marshal(protoParams)
	if err != nil {
		return err
	}
	req.Params = data

	data, err = proto.Marshal(req)
	if err != nil {
		return err
	}

	_, err = WriteRpc(c.rwc, data)

	return err
}

func (c *clientCodec) ReadResponseHeader(r *rpc.Response) error {
	data, err := ReadRpc(c.rwc)
	if err != nil {
		return err
	}

	rsp := &PbRpcResponse{}
	err = proto.Unmarshal(data, rsp)
	if err != nil {
		return err
	}

	c.rsp = rsp

	r.Seq = rsp.GetId()
	if rsp.GetResult() == nil {
		if rsp.GetError() == "" {
			r.Error = "Unknown error occurred"
		} else {
			r.Error = rsp.GetError()
		}
	}

	return nil
}

func (c *clientCodec) ReadResponseBody(result interface{}) error {
	if result == nil {
		return nil
	}

	protoResult, ok := result.(proto.Message)
	if !ok {
		return errors.New("Invalid type given")
	}

	return proto.Unmarshal(c.rsp.GetResult(), protoResult)
}

func (c *clientCodec) Close() error {
	return c.rwc.Close()
}
