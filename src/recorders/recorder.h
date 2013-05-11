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

#ifndef RECORDER_H
#define RECORDER_H

#include "api/qarvdecoder.h"
#include <QList>
#include <QString>

namespace QArv {

class Recorder {
public:
  virtual ~Recorder() {}

  //! Check if the recorder was initialized successfully
  virtual bool isOK() = 0;

  /*!
   * Write a single frame. Both raw and decoded forms must be provided, it
   * is up to the recorder to decide whether the decoded form is useful.
   */
  virtual void recordFrame(QByteArray raw, cv::Mat decoded) = 0;
};

class OutputFormat {
public:
  //! Returns the name of the output format.
  virtual QString name() = 0;

  //! Instantiates a recorder using this plugin.
  virtual Recorder* makeRecorder(QArvDecoder* decoder,
                                 QString fileName,
                                 QSize frameSize,
                                 int framesPerSecond,
                                 bool appendToFile) = 0;

  //! Creates a recorder for the requested output format.
  static Recorder* makeRecorder(QArvDecoder* decoder,
                                QString fileName,
                                QString outputFormat,
                                QSize frameSize,
                                int framesPerSecond,
                                bool appendToFile);

  //! Returns a list of supported output formats.
  static QList<QString> outputFormats();
};

}

Q_DECLARE_INTERFACE(QArv::OutputFormat,
                    "si.ad-vega.qarv.QArvOutputFormat/0.1");

#endif
