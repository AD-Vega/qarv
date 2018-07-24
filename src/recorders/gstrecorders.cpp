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

#include "recorders/gstrecorders.h"
#include "recorders/gstrecorder_implementation.h"

using namespace QArv;

Recorder* HuffyuvAviFormat::makeRecorder(QArvDecoder* decoder,
                                         QString fileName,
                                         QSize frameSize,
                                         int framesPerSecond,
                                         bool writeInfo) {
    return makeGstRecorder({ "avenc_huffyuv", "avimux" },
                           "avenc_huffyuv ! avimux",
                           decoder, fileName, frameSize,
                           framesPerSecond, writeInfo);
}

Q_IMPORT_PLUGIN(HuffyuvAviFormat)
