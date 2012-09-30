project(qarv)
cmake_minimum_required(VERSION 2.8)

macro(set_prefixed varname string)
  set(${varname})
  foreach(name ${ARGN})
    set( ${varname} ${${varname}} ${string}${name} )
  endforeach(name)
endmacro(set_prefixed)

find_package(Qt4 REQUIRED QtCore QtGui QtOpenGL QtNetwork)
include(FindPkgConfig)
pkg_check_modules(ARAVIS REQUIRED aravis-0.2)
pkg_check_modules(GIO REQUIRED gio-2.0)

include(${QT_USE_FILE})

include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src ${ARAVIS_INCLUDE_DIRS} ${GIO_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "-std=c++0x -O3")

set_prefixed(qarv_UIS_pre src/ mainwindow.ui)
qt4_wrap_ui(qarv_UIS ${qarv_UIS_pre})
set_prefixed(qarv_RCS_pre res/ icons/icons.qrc)
qt4_add_resources(qarv_RCS ${qarv_RCS_pre})
set_prefixed(qarv_MOCS src/ glvideowidget.h mainwindow.h arcam.h)
qt4_wrap_cpp(qarv_MOCD ${qarv_MOCS})
set_prefixed(qarv_SRCS src/ framedecoders.cpp arcam.cpp mainwindow.cpp
  main.cpp glvideowidget.cpp)
add_executable(qarv ${qarv_SRCS} ${qarv_MOCD} ${qarv_UIS} ${qarv_RCS})
target_link_libraries(qarv ${QT_LIBRARIES} ${ARAVIS_LDFLAGS} ${GIO_LDFLAGS})

install(TARGETS qarv
        RUNTIME DESTINATION bin/)
