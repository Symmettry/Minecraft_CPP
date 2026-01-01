cmake_minimum_required(VERSION 3.16)
project(MinecraftClient VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SRC_CPP CONFIGURE_DEPENDS "src/*.cpp")
add_executable(${PROJECT_NAME} ${SRC_CPP})

target_include_directories(${PROJECT_NAME} PRIVATE src)

find_package(OpenGL REQUIRED)
if(OPENGL_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)
endif()

find_package(ZLIB REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
if(GLFW_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${GLFW_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${GLFW_LIBRARIES})
endif()

# change this for windows dhdfg
target_include_directories(${PROJECT_NAME} PRIVATE /usr/include)
target_link_libraries(${PROJECT_NAME} PRIVATE zstd)
