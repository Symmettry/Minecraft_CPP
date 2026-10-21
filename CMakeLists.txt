if(_WIN32)
    cmake_minimum_required(VERSION 3.25)
    project(MinecraftClient VERSION 0.1 LANGUAGES CXX)

    set(CMAKE_CXX_STANDARD 26)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    endif()
    file(GLOB_RECURSE SRC_CPP CONFIGURE_DEPENDS "src/*.cpp")
    add_executable(${PROJECT_NAME} ${SRC_CPP})
    target_include_directories(${PROJECT_NAME} PRIVATE src)

    find_package(OpenGL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)

    find_package(ZLIB REQUIRED)
    find_package(zstd CONFIG REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)

    target_link_libraries(${PROJECT_NAME} PRIVATE
            ZLIB::ZLIB
            Zstd::Zstd
            glfw
            glm::glm
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
else()
    cmake_minimum_required(VERSION 3.25)
    project(MinecraftClient VERSION 0.1 LANGUAGES CXX)

    set(CMAKE_CXX_STANDARD 26)
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

    target_include_directories(${PROJECT_NAME} PRIVATE /usr/include)
    target_link_libraries(${PROJECT_NAME} PRIVATE zstd)
    target_link_options(${PROJECT_NAME} PRIVATE -rdynamic)
endif()

find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::Crypto CURL::libcurl)

option(MINECRAFT_ENABLE_TRACY "Enable Tracy profiler instrumentation" ON)
target_sources(${PROJECT_NAME} PRIVATE third_party/tracy/TracyClient.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE third_party/tracy)
if(MINECRAFT_ENABLE_TRACY)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ENABLE TRACY_ON_DEMAND)
endif()
