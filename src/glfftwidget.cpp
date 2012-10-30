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
  this->connect(ffter, SIGNAL(fftDone(QVector<double>)), SLOT(spectrumComputed(QVector<double>)));
}


GLFFTWidget::~GLFFTWidget() {
}


void GLFFTWidget::fromImage(QImage& image) {
//  qDebug() << "GLFFTWidget::fromImage";
  ffter->fromImage(image);
}


void GLFFTWidget::spectrumComputed(QVector<double> result) {
//  qDebug() << "GLFFTWidget::spectrumComputed";
  spectrum = result;
  emit(fftIdle());
  update();
}


void GLFFTWidget::paintGL() {
  QStyleOption opt;
  opt.initFrom(this);
  QPainter painter(this);
  painter.setBackground(opt.palette.color(QPalette::Background));
  painter.fillRect(rect(), opt.palette.color(QPalette::Background));

  const double scale_range = 6;
  float hUnit = rect().height() / scale_range;
  float wUnit = (float)rect().width() / spectrum.size();
  QPointF origin = rect().bottomLeft();

  painter.setPen(opt.palette.color(QPalette::WindowText));
  painter.setBrush(QBrush(opt.palette.color(QPalette::WindowText)));
  
  for (int i = 0; i < spectrum.size(); i++) {
    float height = (spectrum[i] + scale_range);
    if (height < 0)
      height = 0;
    else if (height > scale_range)
      height = scale_range;
      
    QPointF topLeft(origin + QPointF(i*wUnit, -height*hUnit));
    QPointF bottomRight(origin + QPointF((i+1)*wUnit, 0));
    painter.drawRect(QRectF(topLeft, bottomRight));
  }
}
