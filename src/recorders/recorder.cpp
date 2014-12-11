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

#include "recorder.h"
#include <QPluginLoader>

using namespace QArv;

static int init = [] () {
  qRegisterMetaType<Recorder*>("QArv::Recorder*");
  return 0;
}();

Recorder* OutputFormat::makeRecorder(QArvDecoder* decoder,
                                     QString fileName,
                                     QString outputFormat,
                                     QSize frameSize,
                                     int framesPerSecond,
                                     bool writeInfo) {
  auto plugins = QPluginLoader::staticInstances();
  foreach (auto plugin, plugins) {
    auto fmt = qobject_cast<OutputFormat*>(plugin);
    if (fmt != NULL && outputFormat == fmt->name())
      return fmt->makeRecorder(decoder, fileName, frameSize,
                               framesPerSecond, writeInfo);
  }
  return NULL;
}
