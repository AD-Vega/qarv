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

#include "decoders/mono12.h"
#include "decoders/graymap.h"

QImage Mono12Decoder::decode(QByteArray frame) {
  QImage img(size, QImage::Format_Indexed8);
  img.setColorTable(graymap);
  const uint16_t* dta = reinterpret_cast<const uint16_t*>(frame.constData());
  const int h = size.height(), w = size.width();
  for (int i = 0; i < h; i++) {
    auto I = img.scanLine(i);
    for (int j = 0; j < w; j++)
      I[j] = dta[i * w + j] >> 4;
  }
  return img;
}

Q_EXPORT_PLUGIN2(Mono12, Mono12Format)
