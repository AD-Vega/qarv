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

#include <QDebug>
#include <QStringList>
#include "recorders/gstrecorders.h"
extern "C" {
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/gstcaps.h>
#include <gst/gstbuffer.h>
}

using namespace QArv;

class GstRecorder {
private:
  bool pluginsAvailable;

protected:
  GstPipeline* pipeline;
  GstAppSrc* src;

  GstRecorder(): pipeline(NULL), src(NULL) {
    QStringList gplugins = {
      "appsrc",
      "videoparse",
      "queue",
      "autoconvert",
      "filesink"
    };
    pluginsAvailable = checkPluginAvailability(gplugins);
  }

  bool init(const QString& inputFormatStr,
            int framesPerSecond,
            QSize frameSize,
            const QString& outputFormatStr,
            const QString& destFile) {
    if (!pluginsAvailable)
      return false;

    QString fmt("format=%1 "
                "framerate=%2/1 "
                "width=%3 "
                "height=%4");
    fmt = fmt.arg(inputFormatStr,
                  QString::number(framesPerSecond),
                  QString::number(frameSize.width()),
                  QString::number(frameSize.height()));
    QString pipelineStr("appsrc name=thesource ! "
                        "videoparse %1 ! "
                        "queue ! autoconvert ! queue ! "
                        "%2 ! "
                        "filesink location=%3");

    pipelineStr = pipelineStr.arg(fmt, outputFormatStr, destFile);
    GError* error = NULL;
    pipeline = GST_PIPELINE(gst_parse_launch(pipelineStr.toAscii().constData(), &error));
    if (error) {
      qDebug() << "gstreamer error:" << error->message;
      g_clear_error(&error);
      return false;
    }
    src = GST_APP_SRC(gst_bin_get_by_name(GST_BIN(pipeline), "thesource"));
    if (!GST_IS_APP_SRC(src)) {
      qDebug() << "gstreamer error: could not get appsrc";
      return false;
    }
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
    return true;
  }

  virtual ~GstRecorder() {
    if(src)
      gst_app_src_end_of_stream(src);
    if (pipeline)
      gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
    if (src)
      gst_object_unref(src);
    if (pipeline)
      gst_object_unref(pipeline);
  }

  static bool checkPluginAvailability(const QStringList& plugins) {
    bool ok = true;
    foreach (QString p, plugins) {
      auto f = gst_element_factory_find(p.toAscii().constData());
      if (f) {
        gst_object_unref(f);
      } else {
        ok = false;
        qDebug() << "gstreamer missing plugin for" << p;
      }
    }
    return ok;
  }
};

class HuffAviRecorder: public Recorder, protected GstRecorder {
public:
  HuffAviRecorder(QArvDecoder* decoder,
                  QString fileName,
                  QSize frameSize,
                  int framesPerSecond,
                  bool appendToFile,
                  bool writeInfo): GstRecorder() {
    QStringList gplugins = {
      "ffenc_huffyuv",
      "avimux"
    };
    if (!checkPluginAvailability(gplugins)) {
      OK = false;
      return;
    }

    QString outformat("ffenc_huffyuv ! avimux");
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
    case CV_16UC3:
      // gstreamer can't handle BGR48
      informat = "bgr";
      break;

    default:
      qDebug() << "Recorder: Invalid CV image format";
      return;
    }

    OK = init(informat,
              framesPerSecond,
              frameSize,
              outformat,
              fileName);
  }

  bool isOK() { return OK; }

  void recordFrame(QByteArray raw, cv::Mat decoded) {
    if (!OK)
      return;
    GstBuffer* bfr = gst_buffer_new();
    GST_BUFFER_MALLOCDATA(bfr) = NULL;
    if (decoded.type() == CV_16UC3) {
      decoded.convertTo(out, CV_8UC3);
      gst_buffer_set_data(bfr, out.data, out.total()*out.elemSize());
    } else {
      gst_buffer_set_data(bfr, decoded.data, decoded.total()*decoded.elemSize());
    }
    gst_app_src_push_buffer(src, bfr);
  }

private:
  bool OK;
  cv::Mat out;
};

Recorder* HuffAviFormat::makeRecorder(QArvDecoder* decoder,
                                      QString fileName,
                                      QSize frameSize,
                                      int framesPerSecond,
                                      bool appendToFile,
                                      bool writeInfo) {
  return new HuffAviRecorder(decoder, fileName, frameSize, framesPerSecond, appendToFile, writeInfo);
}

Q_EXPORT_PLUGIN2(HuffAvi, QArv::HuffAviFormat)
Q_IMPORT_PLUGIN(HuffAvi)
