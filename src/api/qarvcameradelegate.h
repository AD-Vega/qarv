/*
    qarv, a Qt interface to aravis.
    Copyright (C) 2012, 2013  Jure Varlec <jure.varlec@ad-vega.si>

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

#ifndef QARVCAMERADELEGATE_H
#define QARVCAMERADELEGATE_H

#include <QStyledItemDelegate>

//! QArvCameraDelegate provides editing widgets to go with the QArvCamera model.
/*!
 * Once a view is created for the data model provided by QArvCamera, use this
 * delegate to provide editing widgets for the view.
 */
class QArvCameraDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit QArvCameraDelegate(QObject* parent = 0);
  QWidget* createEditor(QWidget* parent,
                        const QStyleOptionViewItem& option,
                        const QModelIndex& index) const;
  void setEditorData(QWidget* editor, const QModelIndex& index) const;
  void setModelData(QWidget* editor,
                    QAbstractItemModel* model,
                    const QModelIndex & index) const;
  void updateEditorGeometry(QWidget* editor,
                            const QStyleOptionViewItem& option,
                            const QModelIndex & index) const;

private slots:
  void finishEditing();
};

//! QArvEditor is a QWidget that contains the actual editing widgets.
/*! It is used to translate whichever signal is emitted by the actual widgets
 * when editig is finished into the editingFinished() signal which can be
 * used by QArvCameraDelegate.
 */
struct QArvEditor : QWidget {
  Q_OBJECT

public:
  QArvEditor(QWidget* parent = 0) : QWidget(parent) {}

signals:
  void editingFinished();

private slots:
  void editingComplete() { emit editingFinished(); }

  friend class QArvEnumeration;
  friend class QArvString;
  friend class QArvFloat;
  friend class QArvInteger;
  friend class QArvBoolean;
  friend class QArvCommand;
  friend class QArvRegister;
};

//! \name Types that correspond to types of feature nodes
/**@{*/
/*!
 * These types are used by the QArvCamera model and delegate to edit feature
 * node values. Sometimes, a feature has several possible types (e.g. an
 * enumeration can be either an enumeration, a string or an integer; an integer
 * can be cast to a float etc.), but the delegate needs to be able to identify
 * the type exactly. Therefore, each type is given a distinct class.
 * When deciding what type to return, the model tries to match the
 * highest-level type. Each type also provides its own editing widget.
 */

struct QArvType {
  virtual operator QString() const = 0;
  virtual QArvEditor* createEditor(QWidget* parent = NULL) const = 0;
  virtual void populateEditor(QWidget* editor) const = 0;
  virtual void readFromEditor(QWidget* editor) = 0;
};
Q_DECLARE_METATYPE(QArvType*)

struct QArvEnumeration : QArvType {
  QList<QString> names;
  QList<QString> values;
  QList<bool> isAvailable;
  int currentValue;
  QArvEnumeration() : values(), isAvailable() {}
  operator QString()  const {
    return currentValue >= 0 ? names[currentValue] : QString();
  }
  QArvEditor* createEditor(QWidget* parent) const;
  void populateEditor(QWidget* editor) const;
  void readFromEditor(QWidget* editor);
};
Q_DECLARE_METATYPE(QArvEnumeration)

struct QArvString : QArvType {
  QString value;
  qint64 maxlength;
  QArvString() : value() {}
  operator QString() const { return value; }
  QArvEditor* createEditor(QWidget* parent) const;
  void populateEditor(QWidget* editor) const;
  void readFromEditor(QWidget* editor);
};
Q_DECLARE_METATYPE(QArvString)

struct QArvFloat : QArvType {
  double value, min, max;
  QString unit;
  QArvFloat() : unit() {}
  operator QString() const { return QString::number(value) + " " + unit; }
  QArvEditor* createEditor(QWidget* parent) const;
  void populateEditor(QWidget* editor) const;
  void readFromEditor(QWidget* editor);
};
Q_DECLARE_METATYPE(QArvFloat)

struct QArvInteger : QArvType {
  qint64 value, min, max, inc;
  operator QString() const { return QString::number(value); }
  QArvEditor* createEditor(QWidget* parent) const;
  void populateEditor(QWidget* editor) const;
  void readFromEditor(QWidget* editor);
};
Q_DECLARE_METATYPE(QArvInteger)

struct QArvBoolean : QArvType {
  bool value;
  operator QString() const {
    return value
           ? QObject::tr("on/true", "QArvCamera")
           : QObject::tr("off/false", "QArvCamera");
  }
  QArvEditor* createEditor(QWidget* parent) const;
  void populateEditor(QWidget* editor) const;
  void readFromEditor(QWidget* editor);
};
Q_DECLARE_METATYPE(QArvBoolean)

struct QArvCommand : QArvType {
  operator QString() const { return QObject::tr("<command>", "QArvCamera"); }
  QArvEditor* createEditor(QWidget* parent) const;
  void populateEditor(QWidget* editor) const;
  void readFromEditor(QWidget* editor);
};
Q_DECLARE_METATYPE(QArvCommand)

struct QArvRegister : QArvType {
  QByteArray value;
  qint64 length;
  QArvRegister() : value() {}
  operator QString() const { return QString("0x") + value.toHex(); }
  QArvEditor* createEditor(QWidget* parent) const;
  void populateEditor(QWidget* editor) const;
  void readFromEditor(QWidget* editor);
};
Q_DECLARE_METATYPE(QArvRegister)

/**@}*/

#endif
