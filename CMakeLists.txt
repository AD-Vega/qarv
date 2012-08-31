project(qarv)
cmake_minimum_required(VERSION 2.8)

find_package(Qt4 REQUIRED QtCore QtGui QtOpenGL QtNetwork)
include(FindPkgConfig)
set( ENV{PKG_CONFIG_PATH} "/home/exzombie/src/aravis-prefix/lib/pkgconfig/")
pkg_check_modules(ARAVIS REQUIRED aravis-0.2)
pkg_check_modules(GIO REQUIRED gio-2.0)

include(${QT_USE_FILE})

include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR} ${ARAVIS_INCLUDE_DIRS} ${GIO_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "-std=c++0x -O3")

qt4_wrap_ui(qarv_UIS mainwindow.ui)
set(qarv_MOCS glvideowidget.h mainwindow.h arcam.h)
qt4_wrap_cpp(qarv_MOCD ${qarv_MOCS} ${qarv_UIS})
set(qarv_SRCS framedecoders.cpp arcam.cpp mainwindow.cpp main.cpp
              glvideowidget.cpp)
add_executable(qarv ${qarv_SRCS} ${qarv_MOCD})
target_link_libraries(qarv ${QT_LIBRARIES} ${ARAVIS_LDFLAGS} ${GIO_LDFLAGS})
