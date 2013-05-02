/*
    qarv, a Qt interface to aravis.
    Copyright (C) 2012  Jure Varlec <jure.varlec@ad-vega.si>

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


#include "glvideowidget.h"
#include "globals.h"
#include "api/qarvdecoder.h"
#include <QDebug>

using namespace QArv;

GLVideoWidget::GLVideoWidget(QWidget* parent) :
  QGLWidget(QGLFormat(QGL::NoDepthBuffer | QGL::NoSampleBuffers), parent),
  idleImageIcon(), selecting(false), drawRectangle(false),
  fixedSelection(false), corner1(), corner2(), rectangle(),
  whitepen(Qt::white), blackpen(Qt::black), markClipped(false) {
  QFile iconfile(QString(qarv_datafiles) + "/video-display.svgz");
  if (iconfile.exists())
    idleImageIcon = QIcon(iconfile.fileName());
  else
    idleImageIcon = QIcon::fromTheme("video-display");
  whitepen.setWidth(0);
  whitepen.setStyle(Qt::DotLine);
  blackpen.setWidth(0);
}

GLVideoWidget::~GLVideoWidget() {}

QImage GLVideoWidget::getImage() {
  return image;
}

void GLVideoWidget::setImage(const cv::Mat& image_) {
  if (image_.empty()) {
    idling = true;
    image = idleImageIcon.pixmap(size()).toImage();
  } else {
    idling = false;
    {
      const int h = image_.rows, w = image_.cols;
      QSize s = image.size();
      if (s.height() != h
          || s.width() != w
          || image.format() != QImage::Format_ARGB32_Premultiplied)
        image = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
      if (image_.channels() == 3) {
        for (int i = 0; i < h; i++) {
          auto imgLine = image.scanLine(i);
          auto imageLine = image_.ptr<cv::Vec<uint16_t, 3> >(i);
          for (int j = 0; j < w; j++) {
            auto& bgr = imageLine[j];
            bool clipped;
            for (int px = 0; px < 3; px++) {
              uint8_t tmp = bgr[2-px] >> 8;
              clipped = tmp == 255;
              imgLine[4*j + px] = tmp;
            }
            imgLine[4*j + 3] = 255;
            if (clipped && markClipped) {
              imgLine[4*j + 0] = 255;
              imgLine[4*j + 1] = 0;
              imgLine[4*j + 2] = 200;
            }
          }
        }
      } else {
        for (int i = 0; i < h; i++) {
          auto imgLine = image.scanLine(i);
          auto imageLine = image_.ptr<uint16_t>(i);
          for (int j = 0; j < w; j++) {
            uint8_t gray = imageLine[j] >> 8;
            if (gray == 255 && markClipped) {
              imgLine[4*j + 0] = 255;
              imgLine[4*j + 1] = 0;
              imgLine[4*j + 2] = 200;
            } else {
              for (int px = 0; px < 3; px++) {
                imgLine[4*j + px] = gray;
              }
            }
            imgLine[4*j + 3] = 255;
          }
        }
      }
    }
  }
  if (in.size() != image.size()) {
    in = image.rect();
    QResizeEvent nochange(size(), size());
    resizeEvent(&nochange);
  }
  update();
}

void GLVideoWidget::resizeEvent(QResizeEvent* event) {
  QGLWidget::resizeEvent(event);
  if (idling) setImage();
  auto view = rect();
  out = view;
  if (in.size() != view.size()) {
    float aspect = in.width() / (float)in.height();
    float vaspect = view.width() / (float)view.height();
    int x, y, w, h;
    if (vaspect > aspect) {
      h = view.height();
      w = aspect * h;
      y = view.y();
      x = view.x() + (view.width() - w) / 2.;
    } else {
      w = view.width();
      h = w / aspect;
      x = view.x();
      y = view.y() + (view.height() - h) / 2.;
    }
    out.setRect(x, y, w, h);
  }
}

void GLVideoWidget::paintGL() {
  QPainter painter(this);
  if (in.size() != out.size())
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
  painter.drawImage(out, image);

  if (drawRectangle) {
    painter.setPen(blackpen);
    painter.drawRect(drawnRectangle);
    painter.setPen(whitepen);
    painter.drawRect(drawnRectangle);
  }
}

void GLVideoWidget::enableSelection(bool enable) {
  if (enable) {
    selecting = true;
    setCursor(Qt::CrossCursor);

    if (fixedSelection)
      setMouseTracking(true);
  } else {
    selecting = false;
    drawRectangle = false;
    rectangle = QRect();
    setCursor(Qt::ArrowCursor);
    setMouseTracking(false);
  }
}

void GLVideoWidget::setSelectionSize(QSize size) {
  if (size.width() == 0 || size.height() == 0)
    fixedSelection = false;
  else {
    fixedSelection = true;
    fixedSize = size;
  }
}

void GLVideoWidget::mousePressEvent(QMouseEvent* event) {
  QWidget::mousePressEvent(event);
  if (fixedSelection) return;
  if (selecting) corner1 = event->pos();
}

void GLVideoWidget::mouseMoveEvent(QMouseEvent* event) {
  QWidget::mouseMoveEvent(event);

  if (!selecting)
    return;

  drawRectangle = true;
  float scale = out.width() / (float)in.width();

  if (fixedSelection) {
    if ((fixedSize.width() > in.width())
        || (fixedSize.height() > in.height())) {
      rectangle = in;
      drawnRectangle = out;
      return;
    }

    rectangle.setSize(fixedSize);
    rectangle.moveCenter((event->pos() - out.topLeft())/scale);

    if (rectangle.x() < 0)
      rectangle.translate(-rectangle.x(), 0);

    if (rectangle.y() < 0)
      rectangle.translate(0, -rectangle.y());

    int hmargin = (rectangle.x() + rectangle.width()) - in.width();
    if (hmargin > 0)
      rectangle.translate(-hmargin, 0);

    int vmargin = (rectangle.y() + rectangle.height()) - in.height();
    if (vmargin > 0)
      rectangle.translate(0, -vmargin);

    drawnRectangle.moveTopLeft(out.topLeft() + rectangle.topLeft()*scale);
    drawnRectangle.setSize(rectangle.size()*scale);
  } else {
    corner2 = event->pos();
    QRect rec(corner1, corner2);
    rec &= out;
    rec = rec.normalized();
    QPoint corner = rec.topLeft(), outcorner = out.topLeft();
    corner = (corner - outcorner) / scale;
    int width = rec.width() / scale, height = rec.height() / scale;
    rectangle.setRect(corner.x(), corner.y(), width, height);
    drawnRectangle = rec;
  }
}

void GLVideoWidget::mouseReleaseEvent(QMouseEvent* event) {
  QWidget::mouseReleaseEvent(event);
  if (selecting) {
    selecting = false;
    emit selectionComplete(rectangle);
  }
}

void GLVideoWidget::setMarkClipped(bool enable) {
  markClipped = enable;
}
