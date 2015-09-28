cmake_minimum_required(VERSION 3.0)

set(TESTPROTO ${CMAKE_CURRENT_SOURCE_DIR}/calc.proto)
set(TESTPROTOSRC ${CMAKE_CURRENT_SOURCE_DIR}/calc.pb-c.c ${CMAKE_CURRENT_SOURCE_DIR}/calc.pb-c.h)
add_custom_command (
  OUTPUT ${TESTPROTOSRC}
  COMMAND protoc-c --proto_path=${CMAKE_CURRENT_SOURCE_DIR} --c_out=${CMAKE_CURRENT_SOURCE_DIR} ${TESTPROTO}
  )

set(CSERVERSRC ${TESTPROTOSRC} ${CMAKE_CURRENT_SOURCE_DIR}/server.c)
set(CCLIENTSRC ${TESTPROTOSRC} ${CMAKE_CURRENT_SOURCE_DIR}/client.c)

add_executable(c-server ${CSERVERSRC})
target_link_libraries(c-server pbminirpc)
add_executable(c-client ${CCLIENTSRC})
target_link_libraries(c-client pbminirpc pthread)

configure_file(build-go-tests.sh.in build-go-tests.sh @ONLY)

set(GOTESTS ${CMAKE_CURRENT_SOURCE_DIR}/server.go ${CMAKE_CURRENT_SOURCE_DIR}/client.go)
set(GOTESTSBINS ${CMAKE_CURRENT_BINARY_DIR}/go-server ${CMAKE_CURRENT_BINARY_DIR}/go-client)
add_custom_command (
  OUTPUT ${GOTESTSBINS}
  COMMAND bash -x ${CMAKE_CURRENT_BINARY_DIR}/build-go-tests.sh
  )
add_custom_target (go-tests ALL
  DEPENDS ${GOTESTSBINS}
  SOURCES ${GOTESTS}
  )
