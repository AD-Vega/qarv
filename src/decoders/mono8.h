/*
    qarv, a Qt interface to aravis.
    Copyright (C) 2012  Jure Varlec <jure.varlec@ad-vega.si>

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

#ifndef MONO8_H
#define MONO8_H

#include "api/qarvdecoder.h"

class Mono8Decoder : public QArvDecoder {
public:
  Mono8Decoder(QSize size_) : size(size_) {}
  QImage decode(QByteArray frame);
  QString pixelFormat() { return "Mono8"; }
  QString ffmpegPixelFormat() { return "gray"; }
  bool isGrayscale() { return true; }

private:
  QSize size;
};

class Mono8Format : public QObject, public QArvPixelFormat {
  Q_OBJECT
  Q_INTERFACES(QArvPixelFormat)

public:
  QString pixelFormat() { return "Mono8"; }
  QArvDecoder* makeDecoder(QSize size) { return new Mono8Decoder(size); }
};

#endif