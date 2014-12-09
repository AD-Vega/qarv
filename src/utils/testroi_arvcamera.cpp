#include "api/qarvcamera.h"
#include <QDebug>

extern "C" {
#include <arvcamera.h>
}

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

  qDebug() << "Testing ArvCamera API.";
  ArvCamera* ac = cam->aravisCamera();
  double exmin, exmax;
  int x, y, w, h, min, max;
  qDebug() << "exposure_bounds";
  arv_camera_get_exposure_time_bounds(ac, &exmin, &exmax);
  qDebug() << ">> min max = " << exmin << exmax;
  qDebug() << "height_bounds";
  arv_camera_get_height_bounds(ac, &min, &max);
  qDebug() << ">> min max = " << min << max;
  qDebug() << "width_bounds";
  arv_camera_get_width_bounds(ac, &min, &max);
  qDebug() << ">> min max = " << min << max;
  qDebug() << "region";
  arv_camera_get_region(ac, &x, &y, &w, &h);
  qDebug() << ">> x y w h =" << x << y << w << h;

  w = w / 3 / 2 * 2;
  h = h / 3 / 2 * 2;
  qDebug() << "set_region w h" << w << h;
  arv_camera_set_region(ac, x, y, w, h);
  qDebug() << "region";
  arv_camera_get_region(ac, &x, &y, &w, &h);
  qDebug() << ">> x y w h =" << x << y << w << h;

  x = w;
  y = h;
  qDebug() << "set_region x y" << x << y;
  arv_camera_set_region(ac, x, y, w, h);
  qDebug() << "region";
  arv_camera_get_region(ac, &x, &y, &w, &h);
  qDebug() << ">> x y w h =" << x << y << w << h;

  return 0;
}
