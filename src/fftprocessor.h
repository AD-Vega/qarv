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


#ifndef FFTER_H
#define FFTER_H

#include <QObject>
#include <QImage>
#include <complex>
#include <fftw3.h>

class fftprocessor : public QObject {
  Q_OBJECT

public:
  fftprocessor(QObject* parent = 0);
  ~fftprocessor();

  void fromImage(QImage& image);
  
signals:
  void fftDone(QVector<double> spectrum);

private:
  void deallocate();
  void preparePlan(QImage& image);
  void performFFT();
  
  size_t alloc_width;
  size_t alloc_height;
  fftw_plan fftplan;
  std::complex<double> *fft2d_in;
  std::complex<double> *fft2d_out;
  int halfx;
  int halfy;
  int maxidx;
  double *spectrum_accum;
  int *spectrum_count;
  QVector<double> spectrum;
};

#endif
