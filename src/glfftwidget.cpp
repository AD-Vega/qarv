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


GLFFTWidget::GLFFTWidget(QWidget* parent) :
  QGLWidget(), isReferenced(false) {

  ffter = new fftprocessor(this);
  this->connect(ffter, SIGNAL(fftDone(QVector<double>, bool, bool)), SLOT(spectrumComputed(QVector<double>, bool, bool)));
  this->connect(ffter, SIGNAL(fftQuality(double)), SIGNAL(fftQuality(double)));
}


GLFFTWidget::~GLFFTWidget() {
}


void GLFFTWidget::fromImage(QImage& image, bool wantReference, bool setReference) {
//  qDebug() << "GLFFTWidget::fromImage";
  ffter->fromImage(image, wantReference, setReference);
}


void GLFFTWidget::spectrumComputed(QVector<double> result, bool isReferenced, bool haveReference) {
//  qDebug() << "GLFFTWidget::spectrumComputed";
  spectrum = result;
  this->isReferenced = isReferenced;
  emit(fftStatus(isReferenced, haveReference));
  emit(fftIdle());
  update();
}


void GLFFTWidget::paintGL() {
  QStyleOption opt;
  opt.initFrom(this);
  QPainter painter(this);
  painter.setBackground(opt.palette.color(QPalette::Background));
  painter.fillRect(rect(), opt.palette.color(QPalette::Background));

  double scale_min, scale_max;
  if (isReferenced) {
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
