cmake_minimum_required(VERSION 3.20)
project(antropy_farm_gateway VERSION 0.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ANTROPY_BUILD_TESTS "Build tests" OFF)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)
add_library(antropy_gateway_lib
  src/settings.cpp
  src/ring_buffer.cpp
  src/metrics.cpp
  src/optimizer.cpp
  src/lora_receiver.cpp
  src/grpc_client.cpp
  src/service.cpp
  src/sqlite_store.cpp
)

target_include_directories(antropy_gateway_lib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(antropy_gateway_lib
  PUBLIC
    Threads::Threads
    SQLite::SQLite3
)

add_executable(antropy_farm_gateway src/main.cpp)
target_link_libraries(antropy_farm_gateway PRIVATE antropy_gateway_lib)

install(TARGETS antropy_farm_gateway RUNTIME DESTINATION bin)
install(FILES config/settings.json DESTINATION share/antropy_farm_gateway/config)
