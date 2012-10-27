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

#include "glhistogramwidget.h"
#include <QStyleOption>
#include <QDebug>

GLHistogramWidget::GLHistogramWidget(QWidget* parent) : QGLWidget() {
  indexed = true;
  logarithmic = false;
  histRed = new float[256];
  histGreen = new float[256];
  histBlue = new float[256];
  uchar nullimg[] = { 0 };
  QImage img(nullimg, 1, 1, 1, QImage::Format_Indexed8);
  fromImage(img);
}

GLHistogramWidget::~GLHistogramWidget() {
  delete[] histRed;
  delete[] histGreen;
  delete[] histBlue;
}

void GLHistogramWidget::setLogarithmic(bool logarithmic_) {
  logarithmic = logarithmic_;
}

void GLHistogramWidget::fromImage(QImage& image) {
  if (image.format() == QImage::Format_Indexed8) {
    indexed = true;
    for (int i = 0; i < 256; i++)
      histRed[i] = 0;
    for (int i = 0; i < image.height(); i++) {
      const uchar* line = image.scanLine(i);
      for (int j = 0; j < image.width(); j++) {
	histRed[line[j]] += 1;
      }
    }
    if (logarithmic)
      for (int i = 0; i < 256; i++)
	histRed[i] = log2(histRed[i] + 1);
  } else {
    indexed = false;
    for (int i = 0; i < 256; i++)
      histRed[i] = histGreen[i] = histBlue[i] = 0;
    auto img = image.convertToFormat(QImage::Format_RGB888);
    for (int i = 0; i < img.height(); i++) {
      const uchar* line = img.scanLine(i);
      for (int j = 0; j < 3*img.width(); j += 3) {
	histRed[line[j]] += 1;
	histGreen[line[j+1]] += 1;
	histBlue[line[j+2]] += 1;
      }
    }
    if (logarithmic) {
      float* histograms[] = { histRed, histGreen, histBlue };
      for (int c = 0; c < 3; c++)
	for (int i = 0; i < 256; i++) {
	  float* h = histograms[c] + i;
	  *h = log2(*h + 1);
	}
    }
  }
  update();
}

void GLHistogramWidget::paintGL() {
  QStyleOption opt;
  opt.initFrom(this);
  QPainter painter(this);
  painter.setBackground(opt.palette.color(QPalette::Background));
  painter.fillRect(rect(), opt.palette.color(QPalette::Background));

  float wUnit = rect().width() / 256.;
  QPointF origin = rect().bottomLeft();

  if (indexed) {
    painter.setPen(opt.palette.color(QPalette::WindowText));
    painter.setBrush(QBrush(opt.palette.color(QPalette::WindowText)));

    float max = 0;
    for (int i = 0; i < 256; i++)
      if (histRed[i] > max) max = histRed[i];
    float hUnit = rect().height() / max;
    for (int i = 0; i < 256; i++) {
      float height = histRed[i]*hUnit;
      QPointF topLeft(origin + QPointF(i*wUnit, -height));
      QPointF bottomRight(origin + QPointF((i+1)*wUnit, 0));
      painter.drawRect(QRectF(topLeft, bottomRight));
    }
  } else {
    QColor colors[] = {
      QColor::fromRgba(qRgba(255, 0, 0, 128)),
      QColor::fromRgba(qRgba(0, 255, 0, 128)),
      QColor::fromRgba(qRgba(0, 0, 255, 128))
    };
    float* histograms[] = { histRed, histGreen, histBlue };
    for (int c = 0; c < 3; c++) {
      painter.setPen(colors[c]);
      painter.setBrush(colors[c]);

      float max = 0;
      for (int i = 0; i < 256; i++)
        if (histograms[c][i] > max) max = histograms[c][i];
      float hUnit = rect().height() / max;
      for (int i = 0; i < 256; i++) {
        float height = histograms[c][i]*hUnit;
        QPointF topLeft(origin + QPointF(i*wUnit, -height));
        QPointF bottomRight(origin + QPointF((i+1)*wUnit, 0));
        painter.drawRect(QRectF(topLeft, bottomRight));
      }
    }
  }
}
