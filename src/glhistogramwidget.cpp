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

#include "glhistogramwidget.h"
#include "globals.h"
#include <QStyleOption>
#include <QDebug>

using namespace QArv;

GLHistogramWidget::GLHistogramWidget(QWidget* parent) :
  QGLWidget(), unusedHists(&histograms2), histRed(histograms1.red),
  histGreen(histograms1.green), histBlue(histograms1.blue) {
  indexed = true;
  logarithmic = false;
  QFile iconfile(QString(qarv_datafiles)
                 + "/view-object-histogram-linear.svgz");
  if (iconfile.exists())
    idleImageIcon = QIcon(iconfile.fileName());
  else
    idleImageIcon = QIcon::fromTheme("view-object-histogram-linear");
  setIdle();
}

void GLHistogramWidget::setLogarithmic(bool logarithmic_) {
  logarithmic = logarithmic_;
}



void GLHistogramWidget::setIdle() {
    idle = true;
    update();
    return;
}

Histograms* GLHistogramWidget::unusedHistograms() {
  return unusedHists;
}

void GLHistogramWidget::swapHistograms(bool grayscale) {
  idle = false;
  indexed = grayscale;
  histRed = unusedHists->red;
  histGreen = unusedHists->green;
  histBlue = unusedHists->blue;
  if (unusedHists == &histograms1)
    unusedHists = &histograms2;
  else
    unusedHists = &histograms1;
  update();
}

void GLHistogramWidget::paintGL() {
  if (idle) {
    QPainter painter(this);
    painter.drawPixmap(rect(), idleImageIcon.pixmap(rect().size()));
    return;
  }

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
