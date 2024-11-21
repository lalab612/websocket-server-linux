
# date：2023-08-01

# 指定最低版本
cmake_minimum_required(VERSION 2.8)

# 项目路径
project(acqsystem)

set (gcc)

# 输出路径
set (EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin)


# 头文件路径
include_directories(${PROJECT_SOURCE_DIR}/websocket/include)

include_directories(${PROJECT_SOURCE_DIR}/)

# 非实时
# 连接动态库
LINK_LIBRARIES(-lpthread -lm -lrt -ldl)
# 连接实时线程动态库：-lrt

# 生成可执行文件
add_executable(
    # 可执行文件名
    ${PROJECT_NAME}
    # 全局性
    ${PROJECT_SOURCE_DIR}/main.c 
  
    # websocket
    ${PROJECT_SOURCE_DIR}/websocket/ws_server.c
    ${PROJECT_SOURCE_DIR}/websocket/ws_sha1.c
    ${PROJECT_SOURCE_DIR}/websocket/ws_string.c
    ${PROJECT_SOURCE_DIR}/websocket/ws_websocket.c

)
