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


#ifndef GLFFTWIDGET_H
#define GLFFTWIDGET_H

#include <QGLWidget>
#include <QMouseEvent>
#include <fftw3.h>

#include "fftprocessor.h"

class GLFFTWidget : public QGLWidget {
  Q_OBJECT

public:
  GLFFTWidget(QWidget* parent = 0);
  ~GLFFTWidget();

  void paintGL();
  void fromImage(QImage& image, bool setReference);

signals:
  void fftIdle();
  void fftQuality(double quality);

private:
  fftprocessor *ffter;
  QVector<double> spectrum;
  bool isReferenced;

private slots:
  void spectrumComputed(QVector<double> result, bool isReferenced);
};

#endif
