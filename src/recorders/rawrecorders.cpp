/*
    qarv, a Qt interface to aravis.
    Copyright (C) 2013 Jure Varlec <jure.varlec@ad-vega.si>

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

#include "recorders/rawrecorders.h"
#include <QFile>
#include <QSettings>
#include <QRegExp>
#include <QFileInfo>
extern "C" {
#include <libavutil/pixdesc.h>
}

using namespace QArv;

static const QString descExt(".desc");
static const QRegExp descExtRegexp("\\.desc$");

void initDescfile(QSettings& s, QSize size, int FPS) {
  s.beginGroup("qarv_raw_video_description");
  s.remove("");
  s.setValue("description_version", "0.1");
  QFileInfo finfo(s.fileName());
  QString fname(finfo.baseName().remove(descExtRegexp));
  s.setValue("file_name", fname);
  s.setValue("frame_size", size);
  s.setValue("nominal_fps", FPS);
}

class RawUndecoded: public Recorder {
public:
  RawUndecoded(QArvDecoder* decoder_,
               QString fileName,
               QSize size,
               int FPS,
               bool appendToFile) :
    file(fileName), decoder(decoder_) {
    file.open(appendToFile ? QIODevice::Append : QIODevice::WriteOnly);
    if (isOK() && !appendToFile) {
      QSettings s(fileName + descExt, QSettings::Format::IniFormat);
      initDescfile(s, size, FPS);
      s.setValue("encoding_type", "aravis");
      auto pxfmt = QString::number(decoder->pixelFormat(), 16);
      s.setValue("arv_pixel_format", QString("0x") + pxfmt);
    }
  }

  bool isOK() {
    return file.isOpen() && (file.error() == QFile::NoError);
  }

  void recordFrame(QByteArray raw, cv::Mat decoded) {
    file.write(raw);
  }

private:
  QFile file;
  QArvDecoder* decoder;
};

class RawDecoded8: public Recorder {
public:
  RawDecoded8(QArvDecoder* decoder_,
              QString fileName,
              QSize size,
              int FPS,
              bool appendToFile) :
    file(fileName), decoder(decoder_) {
    file.open(appendToFile ? QIODevice::Append : QIODevice::WriteOnly);
    if (isOK() && !appendToFile) {
      QSettings s(fileName + descExt, QSettings::Format::IniFormat);
      initDescfile(s, size, FPS);
      s.setValue("encoding_type", "libavutil");
      s.setValue("libavutil_pixel_format", PIX_FMT_GRAY8);
      s.setValue("libavutil_pixel_format_name", av_get_pix_fmt_name(PIX_FMT_GRAY8));
    }
  }

  bool isOK() {
    return file.isOpen() && (file.error() == QFile::NoError);
  }

  void recordFrame(QByteArray raw, cv::Mat decoded) {
    QArvDecoder::CV2QImage_RGB24(decoded, temporary);
    int bytesPerLine;
    if (temporary.format() == QImage::Format_Indexed8)
      bytesPerLine = temporary.width();
    else
      bytesPerLine = 3*temporary.width();
    for (int i = 0; i < temporary.height(); i++) {
      auto line = reinterpret_cast<const char*>(temporary.constScanLine(i));
      file.write(line, bytesPerLine);
    }
  }

private:
  QFile file;
  QArvDecoder* decoder;
  QImage temporary;
};

class RawDecoded16: public Recorder {
public:
  RawDecoded16(QArvDecoder* decoder_,
               QString fileName,
               QSize size,
               int FPS,
               bool appendToFile) :
    file(fileName), decoder(decoder_) {
    file.open(appendToFile ? QIODevice::Append : QIODevice::WriteOnly);
    if (isOK() && !appendToFile) {
      QSettings s(fileName + descExt, QSettings::Format::IniFormat);
      initDescfile(s, size, FPS);
      s.setValue("encoding_type", "libavutil");
      s.setValue("libavutil_pixel_format", PIX_FMT_GRAY16);
      s.setValue("libavutil_pixel_format_name", av_get_pix_fmt_name(PIX_FMT_GRAY16));
    }
  }

  bool isOK() {
    return file.isOpen() && (file.error() == QFile::NoError);
  }

  void recordFrame(QByteArray raw, cv::Mat decoded) {
    auto& M = decoded;
    // The image is assumed contiguous, as cv::Mat is by default.
    assert(M.isContinuous());
    if (M.channels() == 1) {
      file.write(M.ptr<const char>(), M.total() * M.elemSize());
    } else {
      for(int i = 0; i < M.rows; i++) {
        auto Mr = M.ptr<cv::Vec<uint16_t, 3> >(i);
        for (int j = 0; j < M.cols; j++) {
          const auto& bgr = Mr[j];
          for (int px = 0; px < 3; px++) {
            auto bytes = reinterpret_cast<const char*>(&(bgr[2 - px]));
            file.write(bytes, 2);
          }
        }
      }
    }
  }

private:
  QFile file;
  QArvDecoder* decoder;
};

Recorder* RawUndecodedFormat::makeRecorder(QArvDecoder* decoder,
                                           QString fileName,
                                           QSize frameSize,
                                           int framesPerSecond,
                                           bool appendToFile) {
  return new RawUndecoded(decoder, fileName, frameSize, framesPerSecond, appendToFile);
}

Recorder* RawDecoded8Format::makeRecorder(QArvDecoder* decoder,
                                          QString fileName,
                                          QSize frameSize,
                                          int framesPerSecond,
                                          bool appendToFile) {
  return new RawDecoded8(decoder, fileName, frameSize, framesPerSecond, appendToFile);
}

Recorder* RawDecoded16Format::makeRecorder(QArvDecoder* decoder,
                                           QString fileName,
                                           QSize frameSize,
                                           int framesPerSecond,
                                           bool appendToFile) {
  return new RawDecoded16(decoder, fileName, frameSize, framesPerSecond, appendToFile);
}

Q_EXPORT_PLUGIN2(RawUndecoded, QArv::RawUndecodedFormat)
Q_IMPORT_PLUGIN(RawUndecoded)

Q_EXPORT_PLUGIN2(RawDecoded8, QArv::RawDecoded8Format)
Q_IMPORT_PLUGIN(RawDecoded8)

Q_EXPORT_PLUGIN2(RawDecoded16, QArv::RawDecoded16Format)
Q_IMPORT_PLUGIN(RawDecoded16)
