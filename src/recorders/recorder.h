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

#ifndef RECORDER_H
#define RECORDER_H

#include "api/qarvdecoder.h"
#include <QList>
#include <QString>
#include <QMetaType>
#include <QPair>

namespace QArv
{

class Recorder {
public:
    virtual ~Recorder() {}

    //! Check if the recorder was initialized successfully
    virtual bool isOK() = 0;

    //! Check whether the recorder takes raw input instead of processed.
    virtual bool recordsRaw() = 0;

    /*!
     * Write a single frame. Default implementations does nothing so
     * that it does not need to be overriden for recorders that can't
     * take raw video.
     */
    virtual void recordFrame(QByteArray raw) {};

    /*!
     * Write a single frame. Default implementations does nothing so
     * that it does not need to be overriden for recorders that can't
     * take processed video.
     */
    virtual void recordFrame(cv::Mat processed) {};

    /*!
     * Returns the size of recorded file in bytes and the number
     * of recorded frames.
     */
    virtual QPair<qint64, qint64> fileSize() = 0;
};

class OutputFormat {
public:
    //! Returns the name of the output format.
    virtual QString name() = 0;

    //! Returns true if metadata should be written to a .qarv file.
    virtual bool canWriteInfo() = 0;

    //! Instantiates a recorder using this plugin.
    virtual Recorder* makeRecorder(QArvDecoder* decoder,
                                   QString fileName,
                                   QSize frameSize,
                                   int framesPerSecond,
                                   bool writeInfo) = 0;

    //! Creates a recorder for the requested output format.
    static Recorder* makeRecorder(QArvDecoder* decoder,
                                  QString fileName,
                                  QString outputFormat,
                                  QSize frameSize,
                                  int framesPerSecond,
                                  bool writeInfo);
};

}

Q_DECLARE_INTERFACE(QArv::OutputFormat,
                    "si.ad-vega.qarv.QArvOutputFormat/0.1")
Q_DECLARE_METATYPE(QArv::OutputFormat*)

#endif
