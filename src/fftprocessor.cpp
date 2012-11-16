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
#include <fftw3.h>

using namespace std;

static const int fft_info_meta_id =
   qRegisterMetaType<fft_info>("fft_info");

static const int qvector_double_meta_id =
   qRegisterMetaType<QVector<double>>("QVector<double>");


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
  delete[] spectrum_reference;
}


void fftprocessor::preparePlan(QImage& image) {
  alloc_width = image.width();
  alloc_height = image.height();

  size_t alloc_size = alloc_width*alloc_height*sizeof(double);
  fft2d_in = (double *)fftw_malloc(alloc_size);
  fft2d_out = (double *)fftw_malloc(alloc_size);

  fftplan = fftw_plan_r2r_2d(alloc_height, alloc_width,
                             fft2d_in, fft2d_out,
                             FFTW_REDFT10, FFTW_REDFT10, FFTW_ESTIMATE);

  minsize = min(alloc_width, alloc_height);

  spectrum_accum = new double[minsize];
  spectrum_count = new int[minsize];
  spectrum_reference = new double[minsize];
  spectrum.resize(minsize);

  memset(spectrum_reference, 0, minsize*sizeof(*spectrum_reference));
  info.haveReference = false;
}


void fftprocessor::fromImage(QImage& image, fft_options options) {
  if ((image.width() != alloc_width) || (image.height() != alloc_height)) {
    deallocate();
    preparePlan(image);
  }

  this->options = options;

  if (image.format() == QImage::Format_Indexed8) {
    double *fft_ptr = fft2d_in;
    for (int row = 0; row < alloc_height; row++) {
      const uchar *image_ptr = image.constScanLine(row);
      for (int column = 0; column < alloc_width; column++)
        *(fft_ptr++) = *(image_ptr++);
    }

    QtConcurrent::run(this, &fftprocessor::performFFT);
  }
}


void fftprocessor::performFFT() {
  fftw_execute(fftplan);

  memset(spectrum_accum, 0, minsize*sizeof(*spectrum_accum));
  memset(spectrum_count, 0, minsize*sizeof(*spectrum_count));

  double aspect_x = minsize/alloc_width;
  double aspect_y = minsize/alloc_height;

  int ridx;
  int maxidx = minsize - 1;

  double *fft_ptr = fft2d_out;
  for (int y = 0; y < alloc_height; y++) {
    for (int x = 0; x < alloc_width; x++, fft_ptr++) {
      ridx = floor(sqrt(pow((double)x*aspect_x, 2) + pow((double)y*aspect_y, 2)));

      if (ridx > maxidx)
        continue;

      spectrum_accum[ridx] += abs(*fft_ptr);
      spectrum_count[ridx]++;
    }
  }

  double dc = log10(spectrum_accum[0]/spectrum_count[0]);

  info.haveReference |= options.setReference;
  info.isReferenced = info.haveReference & options.wantReference;

  double quality = 0;
  for (int i = 0; i <= maxidx; i++) {
    spectrum[i] = log10(spectrum_accum[i]/spectrum_count[i]) - dc;

    if (options.setReference)
      spectrum_reference[i] = spectrum[i];
    if (info.isReferenced)
      spectrum[i] -= spectrum_reference[i];

    quality += spectrum[i];
  }

  info.quality = quality;
  emit(fftDone(spectrum, info));
}
