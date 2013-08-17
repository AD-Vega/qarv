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

#include "recorders/gstrecorders.h"
#include <QProcess>
#include <QFileInfo>
#include <QStringList>
#include <QDebug>

using namespace QArv;

/*
 * This class is currently implemented using a QProcess brecause our
 * dependencies link to gstreamer-0.10, which is too old for us.
 */

static bool checkPluginAvailability(const QStringList& plugins) {
  static bool haveInspector = false, failed = false;
  if (failed) return false;
  if (!haveInspector) {
    QProcess I;
    I.start("gst-inspect-1.0 --version");
    if (!I.waitForFinished(1000) || I.exitCode() != 0) {
      qDebug() << "gst-inspector not available";
      failed = true;
      return false;
    }
    haveInspector = true;
  }
  QString cmd("gst-inspect-1.0 --exists ");
  bool ok = true;
  foreach (QString p, plugins) {
    QProcess I;
    I.start(cmd + p);
    I.waitForFinished();
    if (I.exitCode() != 0) {
      qDebug() << "gstreamer missing plugin" << p;
      ok = false;
    }
  }
  return ok;
}

static const bool gstOK = checkPluginAvailability({ "fdsrc",
                                                    "videoparse",
                                                    "queue",
                                                    "videoconvert",
                                                    "filesink" });

class GstRecorder: public Recorder {
private:
  QProcess gstprocess;

public:
  GstRecorder(QString outputFormat,
              QArvDecoder* decoder,
              QString fileName,
              QSize size,
              int FPS,
              bool appendToFile,
              bool writeInfo) {
    if (!gstOK) return;
    QString informat;
    switch (decoder->cvType()) {
    case CV_8UC1:
      informat = "gray8";
      break;

    case CV_16UC1:
#if G_BYTE_ORDER == G_BIG_ENDIAN
      informat = "gray16be";
#else
      informat = "gray16le";
#endif
      break;

    case CV_8UC3:
      informat = "bgr";
      break;

    case CV_16UC3:
      informat = "argb64";
      break;

    default:
      qDebug() << "Recorder: Invalid CV image format";
      return;
    }
    QString cmdline("gst-launch-1.0 -e "
                    "fdsrc fd=0 do-timestamp=true ! "
                    "videoparse "
                    "format=%1 "
                    "framerate=%2/1 "
                    "width=%3 "
                    "height=%4 ! "
                    "queue ! videoconvert ! queue ! "
                    "%5 ! "
                    "filesink location=%6");
    cmdline = cmdline.arg(informat,
                          QString::number(FPS),
                          QString::number(size.width()),
                          QString::number(size.height()),
                          outputFormat,
                          fileName);
    gstprocess.setProcessChannelMode(QProcess::ForwardedChannels);
    gstprocess.start(cmdline, QIODevice::ReadWrite);
    if (!gstprocess.waitForStarted(1000))
      qDebug() << "gstreamer failed to start";
  }

  virtual ~GstRecorder() {
    gstprocess.closeWriteChannel();
    gstprocess.terminate();
    if (!gstprocess.waitForFinished(1000))
      qDebug() << "gstreamer process did not finish";
  }

  bool isOK() {
    if (!gstOK) return false;
    if (gstprocess.state() == QProcess::Starting) {
      if (!gstprocess.waitForStarted(1000)) {
        qDebug() << "gstreamer failed to start";
	return false;
      }
    }
    return gstprocess.state() == QProcess::Running;
  }

  void recordFrame(QByteArray raw, cv::Mat decoded) {
    if (!isOK())
      return;
    if (decoded.type() == CV_16UC3) {
      for (uint i = 0; i < decoded.total(); i+=3) {
        auto p = reinterpret_cast<char*>(decoded.data + i);
        gstprocess.putChar(0);
        gstprocess.putChar(p[2]);
        gstprocess.putChar(p[1]);
        gstprocess.putChar(p[0]);
      }
    } else {
      auto p = reinterpret_cast<char*>(decoded.data);
      gstprocess.write(p, decoded.total()*decoded.elemSize());
    }
  }
};

Recorder* HuffyuvAviFormat::makeRecorder(QArvDecoder* decoder,
                                         QString fileName,
                                         QSize frameSize,
                                         int framesPerSecond,
                                         bool appendToFile,
                                         bool writeInfo) {
  static bool OK = checkPluginAvailability({ "avenc_huffyuv", "avimux" });
  if (OK)
    return new GstRecorder("avenc_huffyuv ! avimux",
                           decoder, fileName, frameSize,
                           framesPerSecond, appendToFile, writeInfo);
  else
    return NULL;
}

Q_EXPORT_PLUGIN2(HuffyuvAvi, QArv::HuffyuvAviFormat)
Q_IMPORT_PLUGIN(HuffyuvAvi)
