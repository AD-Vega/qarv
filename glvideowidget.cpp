/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2012  Jure Varlec <jure.varlec@gmail.com>

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
#include <QDebug>

GLVideoWidget::GLVideoWidget(QWidget* parent):
  QGLWidget(QGLFormat(QGL::NoDepthBuffer | QGL::NoSampleBuffers), parent),
  image() {}

void GLVideoWidget::setImage(const QImage& image_) {
  image = image_;
  update();
}

QImage GLVideoWidget::getImage() {
  return image;
}

GLVideoWidget::~GLVideoWidget() {}

void GLVideoWidget::paintGL() {
  QPainter painter(this);
  auto view = rect();
  auto in = image.rect();
  QRect out(view);

  if (in.size() != view.size()) {
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    float aspect = in.width() / (float)in.height();
    float vaspect = view.width() / (float)view.height();
    int x, y, w, h;
    if(vaspect > aspect) {
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

  painter.drawImage(out, image);
}
