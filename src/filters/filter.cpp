/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012-2014 Jure Varlec <jure.varlec@ad-vega.si>
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

#include "filters/filter.h"
#include <QPluginLoader>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QLayout>
#include <QDialogButtonBox>
#include <QPushButton>

using namespace QArv;

ImageFilterSettingsWidget::ImageFilterSettingsWidget(ImageFilter* filter,
                                                     QWidget* parent,
                                                     Qt::WindowFlags f) :
  QWidget(parent, f), imageFilter(filter) {}

ImageFilter::ImageFilter(ImageFilterPlugin* plugin):
  pluginPtr(plugin) {}

ImageFilterPlugin* ImageFilter::plugin() {
  return pluginPtr;
}

ImageFilter* ImageFilterPlugin::makeFilter(QString name) {
  auto plugins = QPluginLoader::staticInstances();
  foreach (auto plugin, plugins) {
    auto fmt = qobject_cast<ImageFilterPlugin*>(plugin);
    if (fmt != NULL && name == fmt->name())
      return fmt->makeFilter();
  }
  return NULL;
}

ImageFilterSettingsDialog::ImageFilterSettingsDialog(
  ImageFilterSettingsWidget* settings_,
  QWidget* parent,
  Qt::WindowFlags f) :
  QDockWidget(settings_->windowTitle(), parent, f), settings(settings_) {
  setWidget(new QWidget);
  widget()->setLayout(new QVBoxLayout);
  widget()->layout()->addWidget(settings);
  auto live = new QCheckBox;
  live->setText(tr("Live update"));
  widget()->layout()->addWidget(live);
  auto buttons = new QDialogButtonBox;
  widget()->layout()->addWidget(buttons);

  connect(live, SIGNAL(toggled(bool)), settings, SLOT(setLiveUpdate(bool)));
  buttons->setStandardButtons(QDialogButtonBox::Ok
                              | QDialogButtonBox::Apply
                              | QDialogButtonBox::Cancel);
  connect(buttons, SIGNAL(accepted()), SLOT(accept()));
  connect(buttons, SIGNAL(rejected()), SLOT(reject()));
  connect(buttons->button(QDialogButtonBox::Apply), SIGNAL(clicked(bool)), SLOT(apply()));
}

void ImageFilterSettingsDialog::accept() {
  apply();
  close();
}

void ImageFilterSettingsDialog::reject() {
  settings->imageFilter->restoreSettings();
  close();
}

void ImageFilterSettingsDialog::apply() {
  settings->applySettings();
  settings->imageFilter->saveSettings();
}
