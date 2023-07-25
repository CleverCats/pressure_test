# 放在项目空间根目录下

# 设置最低版本要求
cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(client)

# 编译器选项
set(CMAKE_CXX_FLAGS, "${CMAKE_CXX_FLAGS} -g -02 -Wall")

# 设置头文件目录
include_directories(include)

# 设置源文件目录
aux_source_directory(app APP_SRC)

# 设置可执行文件输出目录
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR})

# 添加可执行文件
add_executable(client ${APP_SRC})

