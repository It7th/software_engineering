cmake_minimum_required(VERSION 3.20)
project(lms_rest_api LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Poco REQUIRED COMPONENTS Foundation Net Util JSON)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPQXX REQUIRED IMPORTED_TARGET libpqxx)

add_executable(lms_api
    src/main.cpp
    src/application.cpp
    src/api_handler.cpp
    src/lms_core.cpp
)

target_compile_options(lms_api PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(lms_api PRIVATE
    Poco::Foundation
    Poco::Net
    Poco::Util
    Poco::JSON
    PkgConfig::LIBPQXX
)
