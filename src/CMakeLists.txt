set(SERVER_SRC server_main.cpp server.cpp)

add_executable(
  server
  ${SERVER_SRC}
)

target_link_libraries(
  server
  pthread
  hiredis
)

set(CLIENT_SRC client_main.cpp client.cpp)

add_executable(
  client
  ${CLIENT_SRC}
)
