/*
    qarv, a Qt interface to aravis.
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

#include "recorders/rawrecorders.h"
#include <QFile>
#include <QSettings>
#include <QRegExp>
#include <QFileInfo>
#include <QDebug>
extern "C" {
#include <libavutil/pixdesc.h>
}

using namespace QArv;

// Make sure settings format matches qarvrecordedvideo.cpp!

static const QString descExt(".qarv");

void initDescfile(QSettings& s, QSize size, int FPS) {
  s.beginGroup("qarv_raw_video_description");
  s.remove("");
  s.setValue("description_version", "0.1");
  QFileInfo finfo(s.fileName());
  QString fname(finfo.completeBaseName());
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
               bool appendToFile,
               bool writeInfo) :
    file(fileName), decoder(decoder_), bytesizeWritten(false) {
    file.open(appendToFile ? QIODevice::Append : QIODevice::WriteOnly);
    if (isOK() && !appendToFile && writeInfo) {
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
    if (isOK()) {
      file.write(raw);
      if (!bytesizeWritten) {
        bytesizeWritten = true;
        QSettings s(file.fileName() + descExt, QSettings::Format::IniFormat);
        s.beginGroup("qarv_raw_video_description");
        s.setValue("frame_bytes", raw.size());
      }
    }
  }

private:
  QFile file;
  QArvDecoder* decoder;
  bool bytesizeWritten;
};

class RawDecoded8: public Recorder {
public:
  RawDecoded8(QArvDecoder* decoder_,
              QString fileName,
              QSize size,
              int FPS,
              bool appendToFile,
              bool writeInfo) :
    file(fileName), decoder(decoder_), OK(true) {
    file.open(appendToFile ? QIODevice::Append : QIODevice::WriteOnly);
    if (isOK() && !appendToFile && writeInfo) {
      enum PixelFormat fmt;
      int frameBytes;
      switch (decoder->cvType()) {
      case CV_8UC1:
      case CV_16UC1:
        fmt = PIX_FMT_GRAY8;
        frameBytes = size.width()*size.height();
        break;
      case CV_8UC3:
      case CV_16UC3:
        fmt = PIX_FMT_BGR24;
        frameBytes = size.width()*size.height()*3;
        break;
      default:
        qDebug() << "Recorder: Invalid CV image format";
        return;
      }
      QSettings s(fileName + descExt, QSettings::Format::IniFormat);
      initDescfile(s, size, FPS);
      s.setValue("encoding_type", "libavutil");
      s.setValue("libavutil_pixel_format", fmt);
      s.setValue("libavutil_pixel_format_name", av_get_pix_fmt_name(fmt));
      s.setValue("frame_bytes", frameBytes);
    }
  }

  bool isOK() {
    return OK && file.isOpen() && (file.error() == QFile::NoError);
  }

  void recordFrame(QByteArray raw, cv::Mat decoded) {
    if (!isOK())
      return;
    if (decoded.depth() == CV_8U) {
      for (auto in = decoded.begin<uint8_t>(), end = decoded.end<uint8_t>();
           in != end; in++) {
        file.putChar(*reinterpret_cast<char*>(&(*in)));
      }
    } else {
      for (auto in = decoded.begin<uint16_t>(), end = decoded.end<uint16_t>();
           in != end; in++) {
        uint8_t tmp = (*in) >> 8;
        file.putChar(*reinterpret_cast<char*>(&tmp));
      }
    }
  }

private:
  QFile file;
  QArvDecoder* decoder;
  bool OK;
};

class RawDecoded16: public Recorder {
public:
  RawDecoded16(QArvDecoder* decoder_,
               QString fileName,
               QSize size,
               int FPS,
               bool appendToFile,
               bool writeInfo) :
    file(fileName), decoder(decoder_), OK(true) {
    file.open(appendToFile ? QIODevice::Append : QIODevice::WriteOnly);
    if (isOK() && !appendToFile && writeInfo) {
      enum PixelFormat fmt;
      int frameBytes;
      switch (decoder->cvType()) {
      case CV_8UC1:
      case CV_16UC1:
        fmt = PIX_FMT_GRAY16;
        frameBytes = size.width()*size.height()*2;
        break;
      case CV_8UC3:
      case CV_16UC3:
        fmt = PIX_FMT_BGR48;
        frameBytes = size.width()*size.height()*6;
        break;
      default:
        qDebug() << "Recorder: Invalid CV image format";
        return;
      }
      QSettings s(fileName + descExt, QSettings::Format::IniFormat);
      initDescfile(s, size, FPS);
      s.setValue("encoding_type", "libavutil");
      s.setValue("libavutil_pixel_format", fmt);
      s.setValue("libavutil_pixel_format_name", av_get_pix_fmt_name(fmt));
      s.setValue("frame_bytes", frameBytes);
    }
  }

  bool isOK() {
    return OK && file.isOpen() && (file.error() == QFile::NoError);
  }

  void recordFrame(QByteArray raw, cv::Mat decoded) {
    if (!isOK())
      return;
    if (decoded.depth() == CV_16U) {
      for (auto in = decoded.begin<uint16_t>(), end = decoded.end<uint16_t>();
           in != end; in++) {
        char* tmpp = reinterpret_cast<char*>(&(*in));
        file.putChar(tmpp[0]);
        file.putChar(tmpp[1]);
      }
    } else {
      for (auto in = decoded.begin<uint8_t>(), end = decoded.end<uint8_t>();
           in != end; in++) {
        uint16_t tmp = (*in) << 8;
        char* tmpp = reinterpret_cast<char*>(&tmp);
        file.putChar(tmpp[0]);
        file.putChar(tmpp[1]);
      }
    }
  }

private:
  QFile file;
  QArvDecoder* decoder;
  bool OK;
};

Recorder* RawUndecodedFormat::makeRecorder(QArvDecoder* decoder,
                                           QString fileName,
                                           QSize frameSize,
                                           int framesPerSecond,
                                           bool appendToFile,
                                           bool writeInfo) {
  return new RawUndecoded(decoder, fileName, frameSize, framesPerSecond, appendToFile, writeInfo);
}

Recorder* RawDecoded8Format::makeRecorder(QArvDecoder* decoder,
                                          QString fileName,
                                          QSize frameSize,
                                          int framesPerSecond,
                                          bool appendToFile,
                                          bool writeInfo) {
  return new RawDecoded8(decoder, fileName, frameSize, framesPerSecond, appendToFile, writeInfo);
}

Recorder* RawDecoded16Format::makeRecorder(QArvDecoder* decoder,
                                           QString fileName,
                                           QSize frameSize,
                                           int framesPerSecond,
                                           bool appendToFile,
                                           bool writeInfo) {
  return new RawDecoded16(decoder, fileName, frameSize, framesPerSecond, appendToFile, writeInfo);
}

Q_EXPORT_PLUGIN2(RawUndecoded, QArv::RawUndecodedFormat)
Q_IMPORT_PLUGIN(RawUndecoded)

Q_EXPORT_PLUGIN2(RawDecoded8, QArv::RawDecoded8Format)
Q_IMPORT_PLUGIN(RawDecoded8)

Q_EXPORT_PLUGIN2(RawDecoded16, QArv::RawDecoded16Format)
Q_IMPORT_PLUGIN(RawDecoded16)
