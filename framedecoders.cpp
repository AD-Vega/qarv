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

#include "framedecoders.h"
#include <QHash>
#include <cstdlib>
#include <cassert>

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
    const char* dta = frame.constData();
    const int h = size.height(), w = size.width();
    for (int i=0; i<h; i++)
      memcpy(img.scanLine(i), dta+i*w, w);
    return img;
  }

private:
  QSize size;
};

class Mono12: public FrameDecoder {
public:
  Mono12(QSize size_): size(size_) {};
  ~Mono12() {}
  QImage decode(QByteArray frame) {
    QImage img(size, QImage::Format_Indexed8);
    img.setColorTable(graymap);
    const uchar* dta = reinterpret_cast<const uchar*>(frame.constData());
    const int h = size.height(), w = size.width(), W = img.bytesPerLine();
    auto I = img.bits();
    for (int i=0; i<h; i++)
      for (int j=0; j<w; j++)
        I[i*W+j] = dta[i*w+j] >> 4;
  }
private:
  QSize size;
};

class Mono12Packed: public FrameDecoder {
public:
  Mono12Packed(QSize size_): size(size_) {};
  ~Mono12Packed() {}
  QImage decode(QByteArray frame) {
    QImage img(size, QImage::Format_Indexed8);
    img.setColorTable(graymap);
    const uchar* dta = reinterpret_cast<const uchar*>(frame.constData());
    const int h = size.height(), w = size.width(), W = img.bytesPerLine();
    assert(frame.size() % 2 == 0);
    assert(h*w % 2 == 0);

    int line = 0;
    uchar* outptr = img.scanLine(0);
    uchar* outptr_base = outptr;
    const uchar* inptr = dta;
    uint16_t pixel;
    char * bytes = reinterpret_cast<char*>(&pixel);
    while (inptr < dta + frame.size()) {
      bytes[0] = inptr[1] << 4;
      bytes[1] = inptr[0];
      pixel >>= 8;
      *(outptr++) = pixel;

      if (outptr - outptr_base == w) {
        if (line++ == h) break;
        outptr = outptr_base = img.scanLine(line);
      }

      bytes[0] = inptr[1] << 4;
      bytes[1] = inptr[2];
      pixel >>= 8;
      *(outptr++) = pixel;

      if (outptr - outptr_base == w) {
        if (line++ == h) break;
        outptr = outptr_base = img.scanLine(line);
      }
    }
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
  list.insert("Mono12", makeDecoder<Mono12>);
  list.insert("Mono12Packed", makeDecoder<Mono12Packed>);
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
