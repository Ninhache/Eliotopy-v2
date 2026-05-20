cmake_minimum_required(VERSION 3.29)
project(Eliotopy)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_subdirectory(libs/glm)


add_executable(Eliotopy src/main.cpp)
target_include_directories(Eliotopy PRIVATE
        libs/unity6
        libs/unity6/include

        src/memory
        src/types
        src/shared
)
target_link_libraries(Eliotopy user32 gdi32 glm::glm)
