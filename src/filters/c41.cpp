/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012-2015 Jure Varlec <jure.varlec@ad-vega.si>
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

#include "filters/c41.h"
#include "globals.h"
#include <QtPlugin>
#include <array>
#include <cmath>

using namespace QArv;

static const int junk = [] () {
    qRegisterMetaType<QArv::C41Filter::Values>("QArv::C41Filter::Values");
    return 0;
}();

QString C41Plugin::name() {
    return tr("C41");
}

ImageFilter* C41Plugin::makeFilter() {
    return new C41Filter(this);
}

/* Faster pow() approximation; borrowed from http://www.dctsystems.co.uk/Software/power.html
 * Tests on real-world data showed a max error of 4% and avg. error or .1 to .5%,
 * while accelerating rendering by a factor of 4.
 */
static float myLog2(float i) {
    float x;
    float y;
    float LogBodge = 0.346607f;
    //x = *(int*)&i;
    char* type_punning = (char*)&i;
    int* tmp_x = (int*)type_punning;
    x = *tmp_x;
    x *= 1.0 / (1 << 23);       // 1/pow(2,23);
    x = x - 127;

    y = x - floorf(x);
    y = (y - y * y) * LogBodge;
    return x + y;
}

static float myPow2(float i) {
    float PowBodge = 0.33971f;
    float x;
    float y = i - floorf(i);
    y = (y - y * y) * PowBodge;

    x = i + 127 - y;
    x *= (1 << 23);
    //*(int*) &x = (int)x;
    char* type_punning = (char*)&x;
    int* tmp_x = (int*)type_punning;
    *tmp_x = (int)x;
    return x;
}

/*
  static float myPow(float a, float b) {
  return myPow2(b * myLog2(a));
  }
*/

inline static float myPow(float a, float b) {
    return powf(a, b);
}

/*
 * This funciton is ported from the CInelerra implementation of the C41
 * filter, wrriten in 2011 by Florent Delannoy <florent@plui.es> and
 * licensed as GPL2+.
 */
void C41Filter::filterImage(cv::Mat& image) {
    if (image.channels() != 3)
        return;

    int frame_w = image.cols;
    int frame_h = image.rows;
    image.convertTo(image, CV_32F);

    if ((bool)analyze && (C41SettingsWidget*)settingsWidget) {
        // Box blur!
        cv::Mat tmp_frame(image.size(), image.type());
        cv::Mat blurry_frame = image.clone();

        std::vector<float*> rows(frame_h);
        std::vector<float*> tmp_rows(frame_h);
        std::vector<float*> blurry_rows(frame_h);
        for (int i = 0; i < frame_h; ++i) {
            rows[i] = image.ptr<float>(i);
            tmp_rows[i] = tmp_frame.ptr<float>(i);
            blurry_rows[i] = blurry_frame.ptr<float>(i);
        }

        int boxw = 5, boxh = 5;
        // 10 passes of Box blur should be good
        int pass;
        int x;
        int y;
        int y_up;
        int y_down;
        int x_right;
        int x_left;
        float component;
        for (pass = 0; pass < 10; pass++) {
            for (y = 0; y < frame_h; y++) {
                for (x = 0; x < (3 * frame_w); x++) {
                    tmp_rows[y][x] = blurry_rows[y][x];
                }
            }
            for (y = 0; y < frame_h; y++) {
                y_up = (y - boxh < 0) ? 0 : y - boxh;
                y_down = (y + boxh >= frame_h) ? frame_h - 1 : y + boxh;
                for (x = 0; x < (3*frame_w); x++) {
                    x_left = (x-(3*boxw) < 0) ? 0 : x-(3*boxw);
                    x_right = (x+(3*boxw) >= (3*frame_w)) ? (3*frame_w)-1 : x+(3*boxw);
                    component = (tmp_rows[y_down][x_right]
                                 +tmp_rows[y_up][x_left]
                                 +tmp_rows[y_up][x_right]
                                 +tmp_rows[y_down][x_right])/4;
                    blurry_rows[y][x] = component;
                }
            }
        }

        // Compute magic negfix values
        float minima_r = 50.;
        float minima_g = 50.;
        float minima_b = 50.;
        float maxima_r = 0.;
        float maxima_g = 0.;
        float maxima_b = 0.;

        // Shave the image in order to avoid black borders
        // Tolerance default: 5%, i.e. 0.05
#define TOLERANCE 0.20
#define SKIP_ROW if (i < (TOLERANCE * frame_h)                  \
                     || i > ((1-TOLERANCE)*frame_h)) continue
#define SKIP_COL if (j < (TOLERANCE * frame_w)                  \
                     || j > ((1-TOLERANCE)*frame_w)) continue

        for (int i = 0; i < frame_h; i++) {
            SKIP_ROW;
            float* row = blurry_frame.ptr<float>(i);
            for (int j = 0; j < frame_w; j++, row += 3) {
                SKIP_COL;
                if (row[0] < minima_r) minima_r = row[0];
                if (row[0] > maxima_r) maxima_r = row[0];

                if (row[1] < minima_g) minima_g = row[1];
                if (row[1] > maxima_g) maxima_g = row[1];

                if (row[2] < minima_b) minima_b = row[2];
                if (row[2] > maxima_b) maxima_b = row[2];
            }
        }

        Values values;
        values.min_r = minima_r;
        values.min_g = minima_g;
        values.min_b = minima_b;
        values.light = (minima_r / maxima_r) * 0.95;
        values.gamma_g = logf(maxima_r / minima_r) / logf(maxima_g / minima_g);
        values.gamma_b = logf(maxima_r / minima_r) / logf(maxima_b / minima_b);

        // Update GUI
        C41SettingsWidget* wgt = settingsWidget;
        if (wgt)
            QMetaObject::invokeMethod(wgt, "updateAnalysis", Qt::QueuedConnection,
                                      Q_ARG(QArv::C41Filter::Values, values));
    }

    // Apply the transformation
    if ((bool)fix) {
        double min, max;
        cv::minMaxLoc(image, &min, &max);
        // Get the values from the config instead of the computed ones
        float min_r = this->min_r.load(std::memory_order_relaxed),
            min_g = this->min_g.load(std::memory_order_relaxed),
            min_b = this->min_b.load(std::memory_order_relaxed),
            light = this->light.load(std::memory_order_relaxed),
            gamma_g = this->gamma_g.load(std::memory_order_relaxed),
            gamma_b = this->gamma_b.load(std::memory_order_relaxed);
        for (int i = 0; i < frame_h; i++) {
            float* row = image.ptr<float>(i);
            for (int j = 0; j < frame_w; j++, row += 3) {
                row[0] = (min_r / row[0]) - light;
                row[1] = myPow((min_g / row[1]), gamma_g) - light;
                row[2] = myPow((min_b / row[2]), gamma_b) - light;
            }
        }
        cv::normalize(image, image, max, min, cv::NORM_MINMAX);
    }
}

C41Filter::C41Filter(ImageFilterPlugin* plugin) :
    ImageFilter(plugin) {
    settingsWidget.store(NULL);
    min_r.store(0);
    min_g.store(0);
    min_b.store(0);
    light.store(0);
    gamma_g.store(0);
    gamma_b.store(0);
    analyze.store(false);
    fix.store(false);
}

void C41Filter::restoreSettings() {
    QSettings settings;
    float mr = settings.value("qarv_filters_c41/min_r", 0.0).toFloat();
    float mg = settings.value("qarv_filters_c41/min_g", 0.0).toFloat();
    float mb = settings.value("qarv_filters_c41/min_b", 0.0).toFloat();
    float l = settings.value("qarv_filters_c41/light", 0.0).toFloat();
    float gg = settings.value("qarv_filters_c41/gamma_g", 0.0).toFloat();
    float gb = settings.value("qarv_filters_c41/gamma_b", 0.0).toFloat();
    min_r.store(mr, std::memory_order_relaxed);
    min_g.store(mg, std::memory_order_relaxed);
    min_b.store(mb, std::memory_order_relaxed);
    light.store(l, std::memory_order_relaxed);
    gamma_g.store(gg, std::memory_order_relaxed);
    gamma_b.store(gb, std::memory_order_relaxed);
}

void C41Filter::saveSettings() {
    QSettings settings;
    float mr = min_r.load(std::memory_order_relaxed),
        mg = min_g.load(std::memory_order_relaxed),
        mb = min_b.load(std::memory_order_relaxed),
        l = light.load(std::memory_order_relaxed),
        gg = gamma_g.load(std::memory_order_relaxed),
        gb = gamma_b.load(std::memory_order_relaxed);
    settings.setValue("qarv_filters_c41/min_r", mr);
    settings.setValue("qarv_filters_c41/min_g", mg);
    settings.setValue("qarv_filters_c41/min_b", mb);
    settings.setValue("qarv_filters_c41/light", l);
    settings.setValue("qarv_filters_c41/gamma_g", gg);
    settings.setValue("qarv_filters_c41/gamma_b", gb);
}

ImageFilterSettingsWidget* C41Filter::createSettingsWidget() {
    return new C41SettingsWidget(this);
}

C41SettingsWidget::C41SettingsWidget(ImageFilter* filter_,
                                     QWidget* parent,
                                     Qt::WindowFlags f) :
    ImageFilterSettingsWidget(filter_, parent, f) {
    setupUi(this);
    QMetaObject::connectSlotsByName(this);
    minRBox->setValue(filter()->min_r.load(std::memory_order_relaxed));
    minGBox->setValue(filter()->min_g.load(std::memory_order_relaxed));
    minBBox->setValue(filter()->min_b.load(std::memory_order_relaxed));
    lightBox->setValue(filter()->light.load(std::memory_order_relaxed));
    gammaGBox->setValue(filter()->gamma_g.load(std::memory_order_relaxed));
    gammaBBox->setValue(filter()->gamma_b.load(std::memory_order_relaxed));
    filter()->settingsWidget.store(this);
}

C41SettingsWidget::~C41SettingsWidget() {
    filter()->settingsWidget.store(NULL);
}

void C41SettingsWidget::applySettings() {
    filter()->min_r.store(minRBox->value(), std::memory_order_relaxed);
    filter()->min_g.store(minGBox->value(), std::memory_order_relaxed);
    filter()->min_b.store(minBBox->value(), std::memory_order_relaxed);
    filter()->light.store(lightBox->value(), std::memory_order_relaxed);
    filter()->gamma_g.store(gammaGBox->value(), std::memory_order_relaxed);
    filter()->gamma_b.store(gammaBBox->value(), std::memory_order_relaxed);
    filter()->analyze.store(analyzeGroup->isChecked(), std::memory_order_relaxed);
    filter()->fix.store(fixGroup->isChecked(), std::memory_order_relaxed);
}

void C41SettingsWidget::setLiveUpdate(bool enabled) {
    if (enabled) {
        connect(analyzeGroup, SIGNAL(toggled(bool)), this, SLOT(applySettings()));
        connect(fixGroup, SIGNAL(toggled(bool)), this, SLOT(applySettings()));
        connect(minRBox, SIGNAL(valueChanged(double)), this, SLOT(applySettings()));
        connect(minGBox, SIGNAL(valueChanged(double)), this, SLOT(applySettings()));
        connect(minBBox, SIGNAL(valueChanged(double)), this, SLOT(applySettings()));
        connect(lightBox, SIGNAL(valueChanged(double)), this, SLOT(applySettings()));
        connect(gammaGBox, SIGNAL(valueChanged(double)), this, SLOT(applySettings()));
        connect(gammaBBox, SIGNAL(valueChanged(double)), this, SLOT(applySettings()));
    } else {
        disconnect(analyzeGroup, SIGNAL(toggled(bool)), this, SLOT(applySettings()));
        disconnect(fixGroup, SIGNAL(toggled(bool)), this, SLOT(applySettings()));
        disconnect(minRBox, SIGNAL(valueChanged(double)), this, SLOT(applySettings()));
        disconnect(minGBox, SIGNAL(valueChanged(double)), this, SLOT(applySettings()));
        disconnect(minBBox, SIGNAL(valueChanged(double)), this, SLOT(applySettings()));
        disconnect(lightBox, SIGNAL(valueChanged(double)), this, SLOT(applySettings()));
        disconnect(gammaGBox, SIGNAL(valueChanged(double)), this, SLOT(applySettings()));
        disconnect(gammaBBox, SIGNAL(valueChanged(double)), this, SLOT(applySettings()));
    }
}

void C41SettingsWidget::updateAnalysis(C41Filter::Values values) {
    minRBoxAnalyze->setValue(values.min_r);
    minGBoxAnalyze->setValue(values.min_g);
    minBBoxAnalyze->setValue(values.min_b);
    lightBoxAnalyze->setValue(values.light);
    gammaGBoxAnalyze->setValue(values.gamma_g);
    gammaBBoxAnalyze->setValue(values.gamma_b);
}

C41Filter* C41SettingsWidget::filter() {
    return static_cast<C41Filter*>(imageFilter);
}

void C41SettingsWidget::on_useValuesButton_clicked(bool checked) {
    minRBox->setValue(minRBoxAnalyze->value());
    minGBox->setValue(minGBoxAnalyze->value());
    minBBox->setValue(minBBoxAnalyze->value());
    lightBox->setValue(lightBoxAnalyze->value());
    gammaGBox->setValue(gammaGBoxAnalyze->value());
    gammaBBox->setValue(gammaBBoxAnalyze->value());
}

Q_IMPORT_PLUGIN(C41Plugin)
