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

#include "fftprocessor.h"
#include <QDebug>
#include <QtConcurrentRun>
#include <complex>
#include <fftw3.h>

using namespace std;


fftprocessor::fftprocessor(QObject* parent) : QObject() {
  uchar nullimg[] = { 0 };
  QImage img(nullimg, 1, 1, 1, QImage::Format_Indexed8);
  preparePlan(img);
}


fftprocessor::~fftprocessor() {
  deallocate();
}


void fftprocessor::deallocate() {
  fftw_destroy_plan(fftplan);
  fftw_free(fft2d_in);
  fftw_free(fft2d_out);
  delete[] spectrum_accum;
  delete[] spectrum_count;
}


void fftprocessor::preparePlan(QImage& image) {
//  qDebug() << "fftprocessor::preparePlan";
  alloc_width = image.width();
  alloc_height = image.height();

  size_t alloc_size = alloc_width*alloc_height*sizeof(fftw_complex);
  fft2d_in = reinterpret_cast<complex<double>*>(fftw_malloc(alloc_size));
  fft2d_out = reinterpret_cast<complex<double>*>(fftw_malloc(alloc_size));

  fftplan = fftw_plan_dft_2d(alloc_height, alloc_width,
                             reinterpret_cast<fftw_complex*>(fft2d_in),
                             reinterpret_cast<fftw_complex*>(fft2d_out),
                             FFTW_FORWARD, FFTW_ESTIMATE);

  halfx = alloc_width/2;
  halfy = alloc_height/2;
  maxidx = min(halfx, halfy);
  
  spectrum_accum = new double[maxidx + 1];
  spectrum_count = new int[maxidx + 1];
  spectrum.resize(maxidx + 1);
}


void fftprocessor::fromImage(QImage& image) {
  if ((image.width() != alloc_width) || (image.height() != alloc_height)) {
    deallocate();
    preparePlan(image);
  }

  if (image.format() == QImage::Format_Indexed8) {
    complex<double> *fft_ptr = fft2d_in;
    for (int row = 0; row < alloc_height; row++) {
      const uchar *image_ptr = image.constScanLine(row);
      double hann_y = 0.5*(1-cos(2*M_PI*row/(alloc_height-1)));
      for (int column = 0; column < alloc_width; column++) {
        double hann_x = 0.5*(1-cos(2*M_PI*column/(alloc_width-1)));
        *(fft_ptr++) = *(image_ptr++) * hann_x * hann_y;
      }
    }
    
    QtConcurrent::run(this, &fftprocessor::performFFT);
  }
}


void fftprocessor::performFFT() {
//  qDebug() << "   fftprocessor::performFFT";
  fftw_execute(fftplan);
  
  memset(spectrum_accum, 0, (maxidx + 1)*sizeof(*spectrum_accum));
  memset(spectrum_count, 0, (maxidx + 1)*sizeof(*spectrum_count));
  
  double minsize = min(alloc_width, alloc_height);
  double aspect_x = minsize/alloc_width;
  double aspect_y = minsize/alloc_height;
  
  int fx, fy, ridx;
  
  complex<double> *fft_ptr = fft2d_out;
  for (int y = 0; y < alloc_height; y++) {
    for (int x = 0; x < alloc_width; x++, fft_ptr++) {
      fx = (x > halfx ? alloc_width - x : x);
      fy = (y > halfy ? alloc_height - y : y);
      ridx = floor(sqrt(pow(fx*aspect_x, 2) + pow(fy*aspect_y, 2)));

      if (ridx > maxidx)
        continue;
      
      spectrum_accum[ridx] += abs(*fft_ptr);
      spectrum_count[ridx]++;
    }
  }
  
  double dc = log10(spectrum_accum[0]/spectrum_count[0]);
  for (int i = 0; i <= maxidx; i++)
    spectrum[i] = log10(spectrum_accum[i]/spectrum_count[i]) - dc;
  
//  qDebug() << "   fftprocessor::performFFT (about to emit fftDone)";
  emit(fftDone(spectrum));
//  qDebug() << "   fftprocessor::performFFT (exit)";
}
