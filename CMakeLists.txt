cmake_minimum_required(VERSION 3.10)
project(execdir C)

include(GNUInstallDirs)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LMDB REQUIRED lmdb)

add_executable(execdir execdir.c)

target_compile_options(execdir PRIVATE -pedantic -O2)
target_include_directories(execdir PRIVATE ${LMDB_INCLUDE_DIRS})
target_link_libraries(execdir PRIVATE ${LMDB_LIBRARIES})

# Install binary (755 handled automatically)
install(TARGETS execdir
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install man page (644 by default)
install(FILES execdir.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
