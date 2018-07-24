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

#ifndef RAWRECORDERS_H
#define RAWRECORDERS_H

#include "recorder.h"

namespace QArv
{

class RawUndecodedFormat : public QObject, public OutputFormat {
    Q_OBJECT
    Q_INTERFACES(QArv::OutputFormat)

public:
    QString name() { return "Raw undecoded"; }
    bool canAppend() { return true; }
    bool canWriteInfo() { return true; }
    Recorder* makeRecorder(QArvDecoder* decoder,
                           QString fileName,
                           QSize frameSize,
                           int framesPerSecond,
                           bool writeInfo);
};

class RawDecoded8Format : public QObject, public OutputFormat {
    Q_OBJECT
    Q_INTERFACES(QArv::OutputFormat)

public:
    QString name() { return "Raw decoded (8-bit)"; }
    bool canAppend() { return true; }
    bool canWriteInfo() { return true; }
    Recorder* makeRecorder(QArvDecoder* decoder,
                           QString fileName,
                           QSize frameSize,
                           int framesPerSecond,
                           bool writeInfo);
};

class RawDecoded16Format : public QObject, public OutputFormat {
    Q_OBJECT
    Q_INTERFACES(QArv::OutputFormat)

public:
    QString name() { return "Raw decoded (16-bit)"; }
    bool canWriteInfo() { return true; }
    Recorder* makeRecorder(QArvDecoder* decoder,
                           QString fileName,
                           QSize frameSize,
                           int framesPerSecond,
                           bool writeInfo);
};

}

#endif
