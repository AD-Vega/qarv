/*
    qarv, a Qt interface to aravis.
    Copyright (C) 2012  Andrej Lajovic <andrej.lajovic@ad-vega.si>

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

#include "glfftwidget.h"
#include <QStyleOption>
#include <QDebug>

using namespace std;


GLFFTWidget::GLFFTWidget(QWidget* parent) : QGLWidget() {

  ffter = new fftprocessor(this);
  this->connect(ffter, SIGNAL(fftDone(QVector<double>, fft_info)), SLOT(spectrumComputed(QVector<double>, fft_info)));
}


GLFFTWidget::~GLFFTWidget() {
}


void GLFFTWidget::fromImage(QImage& image, fft_options options) {
  ffter->fromImage(image, options);
}


void GLFFTWidget::spectrumComputed(QVector<double> result, fft_info info) {
  spectrum = result;
  last_info = info;
  emit(fftInfo(last_info));
  emit(fftQuality(last_info.quality));
  update();
}


void GLFFTWidget::paintGL() {
  QStyleOption opt;
  opt.initFrom(this);
  QPainter painter(this);
  painter.setBackground(opt.palette.color(QPalette::Background));
  painter.fillRect(rect(), opt.palette.color(QPalette::Background));

  double scale_min, scale_max;
  if (last_info.isReferenced) {
    scale_min = -3;
    scale_max = 3;
  } else {
    scale_min = -6;
    scale_max = 0;
  }

  float hUnit = rect().height() / (scale_max - scale_min);
  float wUnit = (float)rect().width() / spectrum.size();
  QPointF origin = rect().bottomLeft();

  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(91, 23, 173));

  for (int i = 0; i < spectrum.size(); i++) {
    float height = spectrum[i];

    if (height < scale_min)
      height = scale_min;
    else if (height > scale_max)
      height = scale_max;

    height -= scale_min;

    QPointF topLeft(origin + QPointF(i*wUnit, -height*hUnit));
    QPointF bottomRight(origin + QPointF((i+1)*wUnit, 0));
    painter.drawRect(QRectF(topLeft, bottomRight));
  }
}
