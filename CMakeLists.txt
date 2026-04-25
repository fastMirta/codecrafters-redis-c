cmake_minimum_required(VERSION 3.13)

project(codecrafters-redis)

include_directories(include)

set(SOURCE_FILES 
    src/main.c 
    src/parser.c 
    src/handler.c
    src/utils.c
    src/transaction_handler.c
    src/utility_handler.c
    src/rdb_handler.c
    src/channels_handler.c
    src/set_handler.c
    src/auth_handler.c
    src/list_handler.c
    
)

set(CMAKE_C_STANDARD 23) # Enable the C23 standard

add_executable(redis ${SOURCE_FILES})
find_package(OpenSSL REQUIRED)
target_link_libraries(redis m)
target_link_libraries(redis ssl crypto)