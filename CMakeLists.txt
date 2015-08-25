cmake_minimum_required(VERSION 3.0)
project (MINIRPC)

find_package(PkgConfig)

pkg_check_modules(PROTOC REQUIRED libprotobuf-c)
pkg_check_modules(LIBEVENT REQUIRED libevent)

#set(MINIPROTO ${CMAKE_SOURCE_DIR}/pbrpc.proto)
set(MINIPROTOSRC ${CMAKE_SOURCE_DIR}/pbrpc.pb-c.c ${CMAKE_SOURCE_DIR}/pbrpc.pb-c.h)
#add_custom_command (
  #OUTPUT ${MINIPROTOSRC}
  #COMMAND protoc-c --proto_path=${CMAKE_SOURCE_DIR} --c_out=${CMAKE_SOURCE_DIR} ${MINIPROTO}
  #)


  set(MINIRPCSRC ${MINIPROTOSRC} ${TESTPROTOSRC} ${CMAKE_SOURCE_DIR}/pbrpc.c
         ${CMAKE_SOURCE_DIR}/pbrpc.h
         ${CMAKE_SOURCE_DIR}/pbrpc-clnt.h
         ${CMAKE_SOURCE_DIR}/list.h)
add_library(pbminirpc SHARED ${MINIRPCSRC})
target_link_libraries(pbminirpc ${PROTOC_LIBRARIES} ${LIBEVENT_LIBRARIES})
add_subdirectory(tests)

