/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012, 2013 Jure Varlec <jure.varlec@ad-vega.si>
                             Andrej Lajovic <andrej.lajovic@ad-vega.si>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "api/qarvrecordedvideo.h"
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include "globals.h"
extern "C" {
#include <aravis-0.2/arvbuffer.h>
}

using namespace QArv;

// Make sure settings format matches rawrecorders.cpp!

QArvRecordedVideo::QArvRecordedVideo(const QString& filename):
 fps(0), uncompressed(true), arvPixfmt(0), swscalePixfmt(PIX_FMT_NONE),
 frameBytes(0) {
  QSettings s(filename, QSettings::Format::IniFormat);
  isOK = s.status() == QSettings::Status::NoError;
  if (!isOK) {
    return;
    logMessage() << "Invalid description file.";
  }
  s.beginGroup("qarv_raw_video_description");
  QVariant v = s.value("description_version");
  isOK = v.toString() == "0.1";
  if (!isOK) {
    return;
    logMessage() << "Invalid video description file version.";
  }

  v = s.value("file_name");
  QFileInfo finfo(filename);
  QString dirname = finfo.absoluteDir().path();
  videofile.setFileName(dirname + "/" + v.toString());
  isOK = videofile.open(QIODevice::ReadOnly);
  uncompressed = true;
  if (!isOK) {
    // TODO: try opening compressed versions
    logMessage() << "Unable to open video file" << v.toString();
    return;
  }

  v = s.value("frame_size");
  fsize = v.toSize();
  v = s.value("nominal_fps");
  fps = v.toInt();
  v = s.value("encoding_type");
  auto type = v.toString();

  if (type == "aravis") {
    v = s.value("arv_pixel_format");
    arvPixfmt = v.toString().toULongLong(NULL, 16);
  } else if (type == "libavutil") {
    v = s.value("libavutil_pixel_format");
    swscalePixfmt = (enum PixelFormat)v.toLongLong();
  } else {
    logMessage() << "Unable to determine decoder type.";
    isOK = false;
    return;
  }

  v = s.value("frame_bytes");
  frameBytes = v.toInt();
  if (!frameBytes) {
    isOK = false;
    logMessage() << "Unable to read frame bytesize.";
    return;
  }

  isOK = true;
}

bool QArvRecordedVideo::status() {
  return isOK && (videofile.error() == QFile::NoError);
}

QFile::FileError QArvRecordedVideo::error() {
  return videofile.error();
}

bool QArvRecordedVideo::isSeekable() {
  return uncompressed;
}

int QArvRecordedVideo::framerate() {
  return fps;
}

QSize QArvRecordedVideo::frameSize() {
  return fsize;
}

QArvDecoder* QArvRecordedVideo::makeDecoder() {
  if (!isOK) return NULL;
  if (arvPixfmt != 0) {
    return QArvPixelFormat::makeDecoder(arvPixfmt, fsize);
  } else if (swscalePixfmt != PIX_FMT_NONE) {
    return QArvPixelFormat::makeSwScaleDecoder(swscalePixfmt, fsize);
  } else {
    isOK = false;
    logMessage() << "Unknown decoder type.";
    return NULL;
  }
}

bool QArvRecordedVideo::seek(uint frame) {
  return videofile.seek(frame*frameBytes);
}

QByteArray QArvRecordedVideo::read() {
  return videofile.read(frameBytes);
}

uint QArvRecordedVideo::numberOfFrames() {
  return videofile.size() / frameBytes;
}
