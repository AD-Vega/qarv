project(qarv)
cmake_minimum_required(VERSION 2.8)
set(qarv_VERSION 0.1.0)
set(qarv_ABI 0)
set(qarv_API 0)

macro(set_prefixed varname string)
  set(${varname})
  foreach(name ${ARGN})
    set( ${varname} ${${varname}} ${string}${name} )
  endforeach(name)
endmacro(set_prefixed)

find_package(Qt4 REQUIRED
  QtCore
  QtGui
  QtOpenGL
  QtNetwork
  QtSVG
)
include(FindPkgConfig)
pkg_check_modules(ARAVIS REQUIRED aravis-0.2)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(SWSCALE REQUIRED libswscale)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
find_package( OpenCV REQUIRED )

include(${QT_USE_FILE})

include_directories(
  ${QT_INCLUDES}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${ARAVIS_INCLUDE_DIRS}
  ${GIO_INCLUDE_DIRS}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -fvisibility-inlines-hidden -fvisibility=hidden")
string(TOLOWER "${CMAKE_BUILD_TYPE}" qarv_BTYPE)
if (NOT qarv_BTYPE MATCHES "debug")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -Wall -Werror")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Werror")
add_definitions(-DQARV_PREFIX=\"${CMAKE_INSTALL_PREFIX}\" -DQT_STATICPLUGIN)

set_prefixed(qarv_UIS_pre src/
  mainwindow.ui
)
qt4_wrap_ui(qarv_UIS ${qarv_UIS_pre})
set_prefixed(qarv_RCS_pre res/ icons/icons.qrc)
qt4_add_resources(qarv_RCS ${qarv_RCS_pre})
set_prefixed(qarv_MOCS src/
  glvideowidget.h
  glhistogramwidget.h
  qarvmainwindow.h
  roicombobox.h
  qarvtype.h
  api/qarvgui.h
  api/qarvcamera.h
  api/qarvcameradelegate.h
)
qt4_wrap_cpp(qarv_MOCD ${qarv_MOCS})
set_prefixed(qarv_SRCS src/
  qarvmainwindow.cpp
  main.cpp
  glvideowidget.cpp
  glhistogramwidget.cpp
  globals.cpp
  roicombobox.cpp
  qarvfeaturetree.cpp
  qarvtype.cpp
  recorders/recorder.cpp
  api/qarvgui.cpp
  api/qarvcamera.cpp
  api/qarvdecoder.cpp
  api/qarvcameradelegate.cpp
  api/qarvrecordedvideo.cpp
)
set_prefixed(qarv_decoders_MOCS src/decoders/
  mono12packed.h
  monounpackeddecoders.h
)
qt4_wrap_cpp(qarv_decoders_MOCD ${qarv_decoders_MOCS})
set_prefixed(qarv_decoders_SRC src/decoders/
  graymap.cpp
  mono12packed.cpp
  monounpackeddecoders.cpp
  swscaledecoder.cpp
)
set_prefixed(qarv_recorders_MOCS src/recorders/
  rawrecorders.h
)
qt4_wrap_cpp(qarv_recorders_MOCD ${qarv_recorders_MOCS})
set_prefixed(qarv_recorders_SRC src/recorders/
  rawrecorders.cpp
)
set_prefixed(qarv_TRANS_pre i18n/qarv_ sl.ts)
qt4_add_translation(qarv_TRANS ${qarv_TRANS_pre})
add_library(qarv SHARED
  ${qarv_recorders_MOCD}
  ${qarv_recorders_SRC}
  ${qarv_decoders_SRC}
  ${qarv_decoders_MOCD}
  ${qarv_SRCS}
  ${qarv_MOCD}
  ${qarv_UIS}
  ${qarv_RCS}
  ${qarv_TRANS}
)
target_link_libraries(qarv
  ${QT_LIBRARIES}
  ${ARAVIS_LDFLAGS}
  ${GIO_LDFLAGS}
  ${SWSCALE_LDFLAGS}
  ${AVCODEC_LDFLAGS}
  ${AVUTIL_LDFLAGS}
  ${OpenCV_LIBS}
)
set_target_properties(qarv PROPERTIES SOVERSION ${qarv_ABI})

add_executable(qarvexe src/main.cpp)
target_link_libraries(qarvexe ${QT_LIBRARIES} qarv)
set_target_properties(qarvexe PROPERTIES OUTPUT_NAME qarv)

qt4_wrap_ui(qarv_videoplayer_UI src/utils/qarv_videoplayer.ui)
qt4_wrap_cpp(qarv_videoplayer_MOC
  src/utils/qarv_videoplayer.h
  src/glvideowidget.h
)
add_executable(qarv_videoplayer
  src/utils/qarv_videoplayer.cpp
  src/glvideowidget.cpp
  src/globals.cpp
  ${qarv_videoplayer_MOC}
  ${qarv_videoplayer_UI}
)
target_link_libraries(qarv_videoplayer ${QT_LIBRARIES} qarv)

set_prefixed(qarv_ICONS res/icons/
  document-open.svgz
  document-save.svgz
  edit-clear.svgz
  media-playback-start.svgz
  media-record.svgz
  video-display.svgz
  view-refresh.svgz
  zoom-original.svgz
  office-chart-bar.svgz
  view-object-histogram-linear.svgz
  view-object-histogram-logarithmic.svgz
  edit-clear-locationbar-rtl.svgz
  edit-select.svgz
  media-playback-stop.svgz
  media-playback-pause.svgz
)

install(TARGETS qarv qarvexe qarv_videoplayer
        RUNTIME DESTINATION bin/
        LIBRARY DESTINATION lib/)
set_prefixed(qarv_IHDR src/api/
             qarvcamera.h
             qarvcameradelegate.h
             qarvgui.h
             qarvdecoder.h
             qarvrecordedvideo.h
)
install(FILES ${qarv_IHDR}
        DESTINATION include/qarv-${qarv_API}/)
install(FILES ${qarv_TRANS} ${qarv_ICONS}
        DESTINATION share/qarv/)
install(FILES res/icons/qarv.svgz
        RENAME application-x-qarv.svgz
        DESTINATION share/icons/)
install(FILES res/qarv.desktop res/qarv-player.desktop
        DESTINATION share/applications/)
install(FILES res/qarv.xml
        DESTINATION share/mime/packages/)
