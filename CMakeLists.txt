project(freedesktop-thumbnailer-gdbus-tst C)
cmake_minimum_required(VERSION 2.6)
set(CMAKE_C_FLAGS "-std=c99 ${CMAKE_C_FLAGS}")

find_package(PkgConfig REQUIRED)

#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR})
#find_package(GLIB REQUIRED 2)
#MESSAGE( STATUS "GLIB_GIO_LIBRARIES: " ${GLIB_GIO_LIBRARIES} )
#include_directories(${GLIB_INCLUDE_DIRS})

pkg_check_modules(PC_GIO gio-2.0)
include_directories(${PC_GIO_INCLUDE_DIRS})
SET(GLIB_GIO_LIBRARIES ${PC_GIO_LIBRARIES})

add_executable(freedesktop-thumbnailer-gdbus-tst main.c)
target_link_libraries(freedesktop-thumbnailer-gdbus-tst ${GLIB_GIO_LIBRARIES})
