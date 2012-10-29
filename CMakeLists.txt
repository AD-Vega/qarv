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

set(CMAKE_CXX_FLAGS "-std=c++0x")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3")
add_definitions(-DQARV_PREFIX=\"${CMAKE_INSTALL_PREFIX}\")

set_prefixed(qarv_UIS_pre src/ mainwindow.ui)
qt4_wrap_ui(qarv_UIS ${qarv_UIS_pre})
set_prefixed(qarv_RCS_pre res/ icons/icons.qrc)
qt4_add_resources(qarv_RCS ${qarv_RCS_pre})
set_prefixed(qarv_MOCS src/ glvideowidget.h glhistogramwidget.h mainwindow.h arcam.h)
qt4_wrap_cpp(qarv_MOCD ${qarv_MOCS})
set_prefixed(qarv_SRCS src/ framedecoders.cpp arcam.cpp mainwindow.cpp
  main.cpp glvideowidget.cpp glhistogramwidget.cpp globals.cpp)
set_prefixed(qarv_TRANS_pre i18n/qarv_ sl.ts)
qt4_add_translation(qarv_TRANS ${qarv_TRANS_pre})
add_executable(qarv ${qarv_SRCS} ${qarv_MOCD} ${qarv_UIS} ${qarv_RCS} ${qarv_TRANS})
target_link_libraries(qarv ${QT_LIBRARIES} ${ARAVIS_LDFLAGS} ${GIO_LDFLAGS})

set_prefixed(qarv_ICONS res/icons/ document-open.svgz document-save.svgz
  edit-clear.svgz media-playback-start.svgz media-record.svgz video-display.svgz
  view-refresh.svgz zoom-original.svgz office-chart-bar.svgz
  view-object-histogram-linear.svgz view-object-histogram-logarithmic.svgz
  edit-clear-locationbar-rtl.svgz edit-select.svgz)

install(TARGETS qarv
        RUNTIME DESTINATION bin/)
install(FILES ${qarv_TRANS} ${qarv_ICONS}
        DESTINATION share/qarv/)
install(FILES res/icons/qarv.svgz
        DESTINATION share/icons/)
install(FILES res/qarv.desktop
        DESTINATION share/applications/)
