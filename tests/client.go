package main

import (
	"log"
	"net"
	"net/rpc"

	"github.com/kshlm/pbrpc/pbcodec"
	"github.com/kshlm/pbrpc/tests/calc"
)

func main() {
	args := &calc.CalcReq{Op: new(int32), A: new(int32), B: new(int32)}
	*args.Op = 100
	*args.A = 100
	*args.B = 3
	rsp := new(calc.CalcRsp)

	c, e := net.Dial("tcp", "localhost:9876")
	if e != nil {
		panic(e)
	}

	client := rpc.NewClientWithCodec(pbcodec.NewClientCodec(c))

	e = client.Call("Calculator.Calculate", args, rsp)
	if e == nil {
		log.Println("Result: ", *rsp.Ret)
	} else {
		log.Print("error calling Calculate ", e)
	}

	client.Close()
}
