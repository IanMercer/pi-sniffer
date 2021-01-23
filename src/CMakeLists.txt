cmake_minimum_required (VERSION 3.16.3)
project (SCAN VERSION 1.0)
set(CMAKE_VERBOSE_MAKEFILE ON)

message(STATUS "LIB_DIRS ${LIB_DIRS}")
# specify the C++ standard
#set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_CXX_STANDARD_REQUIRED True)

#set(CMAKE_PREFIX_PATH, 
#rootfs/usr/lib/arm-linux-gnueabihf/

find_package(Threads)

#find_package(Glib REQUIRED)
#find_package(glib-2.0 REQUIRED)

#message(STATUS "$PKG_CONFIG_PATH: ${$PKG_CONFIG_PATH}")
#message(STATUS "$PKG_CONFIG_LIBDIR: ${$PKG_CONFIG_LIBDIR}")
#message(STATUS "PKG_CONFIG_PATH: ${PKG_CONFIG_PATH}")
#message(STATUS "PKG_CONFIG_LIBDIR: ${PKG_CONFIG_LIBDIR}")

#set(PKG_CONFIG_PATH, /home/ian/pi/pi4/rootfs/usr/lib/arm-linux-gnueabihf/pkgconfig)
#set($PKG_CONFIG_PATH, ~/pi/pi4/rootfs/usr/lib/arm-linux-gnueabihf/pkgconfig)

#??include(FindPkgConfig)
#FIND_PACKAGE(PkgConfig)
# PATHS ${SYSROOT_PATH})
#PKG_CHECK_MODULES(GLIB glib-2.0)
# Overwrite them, that didn't work ^
#set(GLIB_INCLUDE_DIRS, /home/ian/pi/pi4/rootfs/usr/include/)
#set(GLIB_LIBRARY_DIRS, /home/ian/pi/pi4/rootfs/usr/lib/arm-linux-gnueabihf/)
#set(GLIB_CFLAGS_OTHER, GLIB_CFLAGS_OTHER)

include_directories(BEFORE /home/ian/pi/pi4/rootfs/usr/lib/arm-linux-gnueabihf/glib-2.0/include)
include_directories(BEFORE /home/ian/pi/pi4/rootfs/usr/include/glib-2.0)
include_directories(AFTER model)
include_directories(AFTER core)
include_directories(AFTER bluetooth)
include_directories(AFTER dbus)

link_directories(/home/ian/pi/pi4/rootfs/usr/lib/arm-linux-gnueabihf)

set(GLib_LIBRARY
 -l/home/ian/pi/pi4/rootfs/lib/arm-linux-gnueabihf/lgio-2.0
 -l/home/ian/pi/pi4/rootfs/lib/arm-linux-gnueabihf/gobject-2.0
 -l/home/ian/pi/pi4/rootfs/lib/arm-linux-gnueabihf/glib-2.0)

#/home/ian/pi/pi4/rootfs/usr/lib/arm-linux-gnueabihf/libglib-2.0.so

set(CMAKE_C_STANDARD 99)

configure_file(version.h.in version.h)

# add_subdirectory needs it own CMakeLists.txt for this
#add_subdirectory(src)
#add_subdirectory(bluetooth)
#add_subdirectory(heuristics)

#message(STATUS "CMAKE_FIND_ROOT_PATH_MODE_LIBRARY: ${CMAKE_FIND_ROOT_PATH_MODE_LIBRARY}")
#message(STATUS "CMAKE_FIND_ROOT_PATH_MODE_INCLUDE: ${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE}")
#message(STATUS "CMAKE_FIND_ROOT_PATH_MODE_PACKAGE: ${CMAKE_FIND_ROOT_PATH_MODE_PACKAGE}")
message(STATUS "GLIB_INCLUDE_DIRS: ${GLIB_INCLUDE_DIRS}")
message(STATUS "GLIB_LIBRARY_DIRS: ${GLIB_LIBRARY_DIRS}")
message(STATUS "GLIB_CFLAGS: ${GLIB_CFLAGS_OTHER}")


add_executable(scan ${SOURCE_FILES} ${Glib_LIBRARY})
target_link_libraries(scan /home/ian/pi/pi4/rootfs/usr/lib/arm-linux-gnueabihf/libglib-2.0.so)

target_sources(scan PRIVATE scan.c core/cJSON.c core/closest.c
 core/http.c core/influx.c core/kalman.c core/knn.c core/udp.c core/utility.c core/webhook.c
 model/accesspoints.c model/device.c model/rooms.c  model/state.c)

#target_include_directories(scan PUBLIC "${PROJECT_BINARY_DIR}")
