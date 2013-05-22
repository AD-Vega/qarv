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

#include "qarvtype.h"
#include <QComboBox>
#include <QLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <cassert>

using namespace QArv;

QArvEditor* QArvEnumeration::createEditor(QWidget* parent) const {
  auto editor = new QArvEditor(parent);
  auto select = new QComboBox(editor);
  select->setObjectName("selectEnum");
  auto layout = new QHBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  editor->setLayout(layout);
  layout->addWidget(select);
  editor->connect(select, SIGNAL(activated(int)), SLOT(editingComplete()));
  return editor;
}

void QArvEnumeration::populateEditor(QWidget* editor) const {
  auto select = editor->findChild<QComboBox*>("selectEnum");
  assert(select);
  select->clear();
  int choose = 0;
  for (int i = 0; i < names.size(); i++) {
    if (isAvailable.at(i)) {
      select->addItem(names.at(i), QVariant::fromValue(values.at(i)));
      if (i < currentValue) choose++;
    }
  }
  select->setCurrentIndex(choose);
}

void QArvEnumeration::readFromEditor(QWidget* editor) {
  auto select = editor->findChild<QComboBox*>("selectEnum");
  assert(select);
  auto val = select->itemData(select->currentIndex());
  currentValue = values.indexOf(val.toString());
}

QArvEditor* QArvString::createEditor(QWidget* parent) const {
  auto editor = new QArvEditor(parent);
  auto edline = new QLineEdit(editor);
  edline->setObjectName("editString");
  auto layout = new QHBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  editor->setLayout(layout);
  layout->addWidget(edline);
  editor->connect(edline, SIGNAL(editingFinished()), SLOT(editingComplete()));
  return editor;
}

void QArvString::populateEditor(QWidget* editor) const {
  auto edline = editor->findChild<QLineEdit*>("editString");
  assert(edline);
  edline->setMaxLength(maxlength);
  edline->setText(value);
}

void QArvString::readFromEditor(QWidget* editor) {
  auto edline = editor->findChild<QLineEdit*>("editString");
  assert(edline);
  value = edline->text();
}

QArvEditor* QArvFloat::createEditor(QWidget* parent) const {
  auto editor = new QArvEditor(parent);
  auto edbox = new QDoubleSpinBox(editor);
  edbox->setObjectName("editFloat");
  auto layout = new QHBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  editor->setLayout(layout);
  layout->addWidget(edbox);
  editor->connect(edbox, SIGNAL(editingFinished()), SLOT(editingComplete()));
  return editor;
}

void QArvFloat::populateEditor(QWidget* editor) const {
  auto edbox = editor->findChild<QDoubleSpinBox*>("editFloat");
  assert(edbox);
  edbox->setMaximum(max);
  edbox->setMinimum(min);
  edbox->setValue(value);
  edbox->setSuffix(QString(" ") + unit);
}

void QArvFloat::readFromEditor(QWidget* editor) {
  auto edbox = editor->findChild<QDoubleSpinBox*>("editFloat");
  assert(edbox);
  value = edbox->value();
}

QArvEditor* QArvInteger::createEditor(QWidget* parent) const {
  auto editor = new QArvEditor(parent);
  auto edbox = new QSpinBox(editor);
  edbox->setObjectName("editInteger");
  auto layout = new QHBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  editor->setLayout(layout);
  layout->addWidget(edbox);
  editor->connect(edbox, SIGNAL(editingFinished()), SLOT(editingComplete()));
  return editor;
}

void QArvInteger::populateEditor(QWidget* editor) const {
  auto edbox = editor->findChild<QSpinBox*>("editInteger");
  assert(edbox);
  edbox->setMaximum(max < INT_MAX ? max : INT_MAX);
  edbox->setMinimum(min > INT_MIN ? min : INT_MIN);
  edbox->setValue(value);
}

void QArvInteger::readFromEditor(QWidget* editor) {
  auto edbox = editor->findChild<QSpinBox*>("editInteger");
  assert(edbox);
  value = edbox->value();
}

QArvEditor* QArvBoolean::createEditor(QWidget* parent) const {
  auto editor = new QArvEditor(parent);
  auto check = new QCheckBox(editor);
  check->setObjectName("editBool");
  auto layout = new QHBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  editor->setLayout(layout);
  layout->addWidget(check);
  editor->connect(check, SIGNAL(clicked(bool)), SLOT(editingComplete()));
  return editor;
}

void QArvBoolean::populateEditor(QWidget* editor) const {
  auto check = editor->findChild<QCheckBox*>("editBool");
  assert(check);
  check->setChecked(value);
}

void QArvBoolean::readFromEditor(QWidget* editor) {
  auto check = editor->findChild<QCheckBox*>("editBool");
  assert(check);
  value = check->isChecked();
}

QArvEditor* QArvCommand::createEditor(QWidget* parent) const {
  auto editor = new QArvEditor(parent);
  auto button = new QPushButton(editor);
  button->setObjectName("execCommand");
  button->setText(QObject::tr("Execute", "QArvCamera"));
  auto layout = new QHBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  editor->setLayout(layout);
  layout->addWidget(button);
  editor->connect(button, SIGNAL(clicked(bool)), SLOT(editingComplete()));
  return editor;
}

void QArvCommand::populateEditor(QWidget* editor) const {}

void QArvCommand::readFromEditor(QWidget* editor) {}

QArvEditor* QArvRegister::createEditor(QWidget* parent) const {
  auto editor = new QArvEditor(parent);
  auto edline = new QLineEdit(editor);
  edline->setObjectName("editRegister");
  auto layout = new QHBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  editor->setLayout(layout);
  layout->addWidget(edline);
  editor->connect(edline, SIGNAL(editingFinished()), SLOT(editingComplete()));
  return editor;
}

void QArvRegister::populateEditor(QWidget* editor) const {
  auto edline = editor->findChild<QLineEdit*>("editRegister");
  assert(edline);
  auto hexval = value.toHex();
  QString imask("");
  for (int i = 0; i < hexval.length(); i++) imask += "H";
  edline->setInputMask(imask);
  edline->setText(hexval);
}

void QArvRegister::readFromEditor(QWidget* editor) {
  auto edline = editor->findChild<QLineEdit*>("editRegister");
  assert(edline);
  value.fromHex(edline->text().toAscii());
}
