#include "api/qarvcamera.h"
#include <QDebug>

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

  qDebug() << "Testing QArv API.";
  QRect roi;
  qDebug() << "getROIWidthBounds";
  qDebug() << ">>" << cam->getROIWidthBounds();
  qDebug() << "getROIHeightBounds";
  qDebug() << ">>" << cam->getROIHeightBounds();
  qDebug() << "getROI";
  qDebug() << ">>" << (roi = cam->getROI());

  roi.setWidth(roi.width() / 3 / 2 * 2);
  roi.setHeight(roi.height() / 3 / 2 * 2);
  qDebug() << "setROI" << roi;
  cam->setROI(roi);
  qDebug() << "getROI";
  qDebug() << ">>" << (roi = cam->getROI());

  roi.translate(roi.width(), roi.height());
    qDebug() << "setROI" << roi;
  cam->setROI(roi);
  qDebug() << "getROI";
  qDebug() << ">>" << (roi = cam->getROI());

  return 0;
}
