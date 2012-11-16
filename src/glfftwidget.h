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
#include <QMap>
#include <QMouseEvent>
#include <fftw3.h>

#include "fftprocessor.h"

class GLFFTWidget : public QGLWidget {
  Q_OBJECT

public:
  GLFFTWidget(QWidget* parent = 0);
  ~GLFFTWidget();

  void paintGL();
  void fromImage(QImage& image, fft_options options);

signals:
  void fftInfo(fft_info info);
  void fftQuality(double quality);

public slots:
  void enableQDisplay(bool state);
  void setQscaleMid(double value);
  void setQscaleSpan(double value);
  void setTscaleSpan(int value);

private:
  void pruneQualityHistory(qint64 older_than);

  fftprocessor *ffter;
  QVector<double> spectrum;
  fft_info last_info;
  QMap<qint64, double> quality;

  bool display_quality;
  double qscale_mid;
  double qscale_span;
  double tscale_span;

private slots:
  void spectrumComputed(QVector<double> result, fft_info info);
};

#endif
