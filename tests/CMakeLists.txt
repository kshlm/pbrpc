cmake_minimum_required(VERSION 3.0)

set(TESTPROTO ${CMAKE_CURRENT_SOURCE_DIR}/calc.proto)
set(TESTPROTOSRC ${CMAKE_CURRENT_SOURCE_DIR}/calc.pb-c.c ${CMAKE_CURRENT_SOURCE_DIR}/calc.pb-c.h)
add_custom_command (
  OUTPUT ${TESTPROTOSRC}
  COMMAND protoc-c --proto_path=${CMAKE_CURRENT_SOURCE_DIR} --c_out=${CMAKE_CURRENT_SOURCE_DIR} ${TESTPROTO}
  )

set(SERVERSRC ${TESTPROTOSRC} ${CMAKE_CURRENT_SOURCE_DIR}/server.c)
set(CLIENTSRC ${TESTPROTOSRC} ${CMAKE_CURRENT_SOURCE_DIR}/client.c)

add_executable(server ${SERVERSRC})
target_link_libraries(server pbminirpc)
add_executable(client ${CLIENTSRC})
target_link_libraries(client pbminirpc pthread)

