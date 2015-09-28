package main

import (
	"log"
	"net"
	"net/rpc"

	"github.com/kshlm/pbrpc/pbcodec"
	"github.com/kshlm/pbrpc/tests/calc"
)

type Calculator int

func (c *Calculator) Calculate(args *calc.CalcReq, reply *calc.CalcRsp) error {
	i := *args.A + *args.B
	log.Println("A:", *args.A)
	log.Println("B:", *args.B)
	log.Println("Sum:", i)
	reply.Ret = &i

	return nil
}

func main() {
	calc := new(Calculator)

	server := rpc.NewServer()
	server.Register(calc)

	l, e := net.Listen("tcp", ":9876")
	if e != nil {
		panic(e)
	}

	for {
		c, e := l.Accept()
		if e != nil {
			continue
		}
		log.Print("New incoming connection:", c.RemoteAddr())
		go server.ServeCodec(pbcodec.NewServerCodec(c))
	}
}
