#include "api/qarvcamera.h"
#include "api/qarvtype.h"
#include <QDebug>
#include <QAbstractItemModel>

int main(int argc, char** argv) {
  QArvCamera::init();

  if (argc < 2) {
    for (auto& camid: QArvCamera::listCameras()) {
      qDebug() << camid.id;
    }
    return 0;
  }

  QArvCamera* cam = nullptr;
  QString arg(argv[1]);
  for (auto& camid: QArvCamera::listCameras()) {
    if (camid.id == arg) {
      cam = new QArvCamera(camid);
      break;
    }
  }
  if (!cam) {
    qDebug() << "Cannot find camera" << arg;
    return 1;
  }

  qDebug() << "Testing features API.";
  QArvInteger x, y, w, h;
  auto qi = [&cam] (const QString& feature) {
    auto index = cam->featureIndex(feature);
    auto var = index.data(Qt::EditRole);
    if (!var.canConvert<QArvInteger>())
      abort();
    return var.value<QArvInteger>();
  };
  auto si = [&cam] (const QString &feature, QArvInteger val) mutable {
    auto index = cam->featureIndex(feature);
    auto var = index.data(Qt::EditRole);
    if (!var.canConvert<QArvInteger>())
      abort();
    var.setValue<QArvInteger>(val);
    cam->setData(index, var);
  };

  qDebug() << "get X";
  x = qi("OffsetX");
  qDebug() << ">> val min max = " << x.value << x.min << x.max;
  qDebug() << "get Y";
  y = qi("OffsetY");
  qDebug() << ">> val min max = " << y.value << y.min << y.max;
  qDebug() << "get width";
  w = qi("Width");
  qDebug() << ">> val min max = " << w.value << w.min << w.max;
  qDebug() << "get height";
  h = qi("Height");
  qDebug() << ">> val min max = " << h.value << h.min << h.max;

  w.value = w.value / 3 / 2 * 2;
  h.value = h.value / 3 / 2 * 2;
  qDebug() << "set width height =" << w.value << h.value;
  si("Width", w);
  si("Height", h);
  qDebug() << "get X";
  x = qi("OffsetX");
  qDebug() << ">> val min max = " << x.value << x.min << x.max;
  qDebug() << "get Y";
  y = qi("OffsetY");
  qDebug() << ">> val min max = " << y.value << y.min << y.max;
  qDebug() << "get width";
  w = qi("Width");
  qDebug() << ">> val min max = " << w.value << w.min << w.max;
  qDebug() << "get height";
  h = qi("Height");
  qDebug() << ">> val min max = " << h.value << h.min << h.max;

  x.value = w.value;
  y.value = h.value;
  qDebug() << "set X Y =" << x.value << y.value;
  si("OffsetX", x);
  si("OffsetY", y);
  qDebug() << "get X";
  x = qi("OffsetX");
  qDebug() << ">> val min max = " << x.value << x.min << x.max;
  qDebug() << "get Y";
  y = qi("OffsetY");
  qDebug() << ">> val min max = " << y.value << y.min << y.max;
  qDebug() << "get width";
  w = qi("Width");
  qDebug() << ">> val min max = " << w.value << w.min << w.max;
  qDebug() << "get height";
  h = qi("Height");
  qDebug() << ">> val min max = " << h.value << h.min << h.max;

  return 0;
}
