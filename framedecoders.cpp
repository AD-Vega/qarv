/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2012  Jure Varlec <jure.varlec@gmail.com>

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

#include "framedecoders.h"
#include <QHash>
#include <cstdlib>

static QVector<QRgb> initGraymap() {
  QVector<QRgb> map(256);
  for (int i=0; i<256; i++) map[i] = qRgb(i, i, i);
  return map;
}

static QVector<QRgb> graymap = initGraymap();

class Mono8: public FrameDecoder {
public:
  Mono8(QSize size_): size(size_) {}
  ~Mono8() {}
  QImage decode(QByteArray frame) {
    QImage img(size, QImage::Format_Indexed8);
    img.setColorTable(graymap);
    char* dta = frame.data();
    const int h = size.height(), w = size.width();
    for (int i=0; i<h; i++)
      memcpy(img.scanLine(i), dta+i*w, w);
    return img;
  }

private:
  QSize size;
};

typedef FrameDecoder* (*makeDecoderFcn) (QSize);

template <class T>
FrameDecoder* makeDecoder(QSize size) {
  return new T(size);
}

static QHash<QString, makeDecoderFcn> listalldecoders() {
  QHash<QString, makeDecoderFcn> list;
  list.insert("Mono8", makeDecoder<Mono8>);
  return list;
}

static QHash<QString, makeDecoderFcn> alldecoders = listalldecoders();

FrameDecoder* makeFrameDecoder(QString format, QSize size) {
  makeDecoderFcn func = alldecoders.value(format);
  if (func == NULL)
    return NULL;
  else
    return func(size);
}
