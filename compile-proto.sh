#! /bin/bash

protoc-c --c_out=. pbrpc.proto
protoc --go_out=pbcodec pbrpc.proto
