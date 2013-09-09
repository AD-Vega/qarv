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

#include "recorders/gstrecorder_implementation.h"
#include <QProcess>
#include <QFileInfo>
#include <QStringList>
#include "globals.h"

using namespace QArv;

const int processTimeout = 10000;

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
    if (!I.waitForFinished(processTimeout) || I.exitCode() != 0) {
      logMessage() << "gst-inspect-1.0 not available";
      failed = true;
      return false;
    }
    I.start("gst-launch-1.0 --version");
    if (!I.waitForFinished(processTimeout) || I.exitCode() != 0) {
      logMessage() << "gst-launch-1.0 not available";
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
      logMessage() << "gstreamer missing plugin" << p;
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
  cv::Mat tmpMat;

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
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
      informat = "gray16-be";
#else
      informat = "gray16-le";
#endif
      break;

    case CV_8UC3:
      informat = "bgr";
      break;

    case CV_16UC3:
      tmpMat = cv::Mat(size.height(), size.width(), CV_16UC4, cv::Scalar(65535, 0, 0, 0));
      informat = "argb64";
      break;

    default:
      logMessage() << "Recorder: Invalid CV image format";
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
    if (!gstprocess.waitForStarted(processTimeout))
      logMessage() << "gstreamer failed to start";
  }

  virtual ~GstRecorder() {
    gstprocess.closeWriteChannel();
    if (!gstprocess.waitForFinished(processTimeout)) {
      logMessage() << "gstreamer process did not finish";
      gstprocess.kill();
    }
  }

  bool isOK() {
    if (!gstOK) return false;
    if (gstprocess.state() == QProcess::Starting) {
      if (!gstprocess.waitForStarted(processTimeout)) {
        logMessage() << "gstreamer failed to start";
	return false;
      }
    }
    return gstprocess.state() == QProcess::Running;
  }

  void recordFrame(QByteArray raw, cv::Mat decoded) {
    if (!isOK())
      return;
    char* p;
    int bytes;
    if (decoded.type() == CV_16UC3) {
      const int mix[] = { 2, 1, 1, 2, 0, 3 };
      cv::mixChannels(&decoded, 1, &tmpMat, 1, mix, 3);
      p = reinterpret_cast<char*>(tmpMat.data);
      bytes = tmpMat.total()*tmpMat.elemSize();
    } else {
      p = reinterpret_cast<char*>(decoded.data);
      bytes = decoded.total()*decoded.elemSize();
    }
    while (gstprocess.bytesToWrite() > 200*bytes)
      gstprocess.waitForBytesWritten(-1);
    gstprocess.write(p, bytes);
  }
};

Recorder* QArv::makeGstRecorder(QStringList plugins,
                          QString pipelineFragment,
                          QArvDecoder* decoder,
                          QString fileName,
                          QSize size,
                          int FPS,
                          bool appendToFile,
                          bool writeInfo) {
  static bool OK = checkPluginAvailability(plugins);
  if (OK)
    return new GstRecorder(pipelineFragment,
                           decoder, fileName, size,
                           FPS, appendToFile, writeInfo);
  else
    return NULL;
}
