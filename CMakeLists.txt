cmake_minimum_required(VERSION 3.10)
project(execdir C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LMDB REQUIRED lmdb)

add_executable(execdir execdir.c)

target_compile_options(execdir PRIVATE -pedantic -O2)
target_include_directories(execdir PRIVATE ${LMDB_INCLUDE_DIRS})
target_link_libraries(execdir PRIVATE ${LMDB_LIBRARIES})
