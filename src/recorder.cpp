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

#include "recorder.h"
#include <QFile>
#include <QDebug>
#include <cassert>

using namespace QArv;

// Order of format classes must match order of strings.
// TODO Make these into proper plugins when there's too many.
// Enforce it below in definition of makeRecorder();

class RawUndecoded;
class RawDecoded8;
class RawDecoded16;

QList<QString> allFormats = {
  "Raw undecoded",
  "Raw decoded (8-bit)",
  "Raw decoded (16-bit)",
};

QList< QString > Recorder::outputFormats() {
  return allFormats;
}

class RawUndecoded: public Recorder {
public:
  RawUndecoded(QString fileName, QSize size, int FPS, bool appendToFile):
    file(fileName) {
    file.open(appendToFile ? QIODevice::Append : QIODevice::WriteOnly);
  }

  bool isOK() {
    return file.isOpen() && (file.error() == QFile::NoError);
  }

  void recordFrame(QByteArray raw, QArvDecoder* decoder) {
    file.write(raw);
  }

private:
  QFile file;
};

class RawDecoded8: public Recorder {
public:
  RawDecoded8(QString fileName, QSize size, int FPS, bool appendToFile):
    file(fileName) {
    file.open(appendToFile ? QIODevice::Append : QIODevice::WriteOnly);
  }

  bool isOK() {
    return file.isOpen() && (file.error() == QFile::NoError);
  }

  void recordFrame(QByteArray raw, QArvDecoder* decoder) {
    QImage img = decoder->getQImage();
    int bytesPerLine;
    if (img.format() == QImage::Format_Indexed8)
      bytesPerLine = img.width();
    else
      bytesPerLine = 3*img.width();
    for (int i = 0; i < img.height(); i++) {
      auto line = reinterpret_cast<const char*>(img.constScanLine(i));
      file.write(line, bytesPerLine);
    }
  }

private:
  QFile file;
};

class RawDecoded16: public Recorder {
public:
  RawDecoded16(QString fileName, QSize size, int FPS, bool appendToFile):
    file(fileName) {
    file.open(appendToFile ? QIODevice::Append : QIODevice::WriteOnly);
  }

  bool isOK() {
    return file.isOpen() && (file.error() == QFile::NoError);
  }

  void recordFrame(QByteArray raw, QArvDecoder* decoder) {
    cv::Mat M = decoder->getCvImage();
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
};

Recorder* Recorder::makeRecorder(QString fileName,
                                 QString outputFormat,
                                 QSize frameSize,
                                 int framesPerSecond,
                                 bool appendToFile) {
  auto idx = allFormats.indexOf(outputFormat);
  switch (idx) {
    case 0: return new RawUndecoded(fileName, frameSize, framesPerSecond, appendToFile);
    case 1: return new RawDecoded8(fileName, frameSize, framesPerSecond, appendToFile);
    case 2: return new RawDecoded16(fileName, frameSize, framesPerSecond, appendToFile);
    default: return NULL;
  }
}
