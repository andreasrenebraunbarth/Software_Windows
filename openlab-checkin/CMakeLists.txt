cmake_minimum_required(VERSION 3.16)
project(OpenLabCheckIn VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(OpenLabCheckIn WIN32 src/main.cpp src/model.cpp src/app.rc src/resource.h)
target_compile_definitions(OpenLabCheckIn PRIVATE UNICODE _UNICODE NOMINMAX)
target_link_libraries(OpenLabCheckIn PRIVATE comctl32 comdlg32 bcrypt)

if(MSVC)
    target_compile_options(OpenLabCheckIn PRIVATE /W4 /utf-8)
    # Manifest kommt aus app.rc
    target_link_options(OpenLabCheckIn PRIVATE /MANIFEST:NO)
    set_property(TARGET OpenLabCheckIn PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_compile_options(OpenLabCheckIn PRIVATE -Wall -Wextra)
    target_link_options(OpenLabCheckIn PRIVATE -municode -static)
endif()
