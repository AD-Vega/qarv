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

#include "decoders/uyvy422.h"
#include <QDebug>

UYVY422Decoder::UYVY422Decoder(QSize size_) :
  size(size_), channel_pointers { 0, 0, 0 }, channel_strides { 0, 0, 0 },
  image_pointers { 0, 0, 0 }, image_strides { 0, 0, 0 } {
    channel_strides[0] = 2 * size.width();
    ctx = sws_getContext(size.width(), size.height(), PIX_FMT_UYVY422,
                         size.width(), size.height(), PIX_FMT_RGB24,
                         SWS_BICUBIC, 0, 0, 0);
}

UYVY422Decoder::~UYVY422Decoder() {
  sws_freeContext(ctx);
}

QImage UYVY422Decoder::decode(QByteArray frame) {
  QImage out(size, QImage::Format_RGB888);
  channel_pointers[0] = reinterpret_cast<const uint8_t*>(frame.constData());
  image_pointers[0] = reinterpret_cast<uint8_t*>(out.scanLine(0));
  image_strides[0] = out.bytesPerLine();
  int outheight = sws_scale(ctx, channel_pointers, channel_strides,
                            0, size.height(),
                            image_pointers, image_strides);
  if(outheight != size.height()) {
    qDebug() << "swscale error! outheight =" << outheight;
    return QImage();
  }
  return out;
}

Q_EXPORT_PLUGIN2(UYVY422, UYVY422Format)