/*
    qarv, a Qt interface to aravis.
    Copyright (C) 2012  Jure Varlec <jure.varlec@ad-vega.si>

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


#ifndef ARCAM_H
#define ARCAM_H

#include <QList>
#include <QString>
#include <QRect>
#include <QPair>
#include <QMetaType>
#include <QImage>
#include <QHostAddress>
#include <QAbstractItemModel>
#include <QStyledItemDelegate>

//! \name Forward declarations to avoid exposing arv.h
/**@{*/
struct _ArvCamera;
typedef _ArvCamera ArvCamera;
struct _ArvDevice;
typedef _ArvDevice ArvDevice;
struct _ArvBuffer;
typedef _ArvBuffer ArvBuffer;
struct _ArvStream;
typedef _ArvStream ArvStream;
struct _ArvGc;
typedef _ArvGc ArvGc;
class QArvFeatureTree;
/**@}*/

//! Objects of this class are used to identify cameras.
/*!
 * This class exposes three public strings containing the internal
 * Aravis id of the camera and the name of the camera vendor and model.
 * These strings are owned by the instance, not by Aravis.
 */
class QArvCameraId {
public:
  QArvCameraId();
  QArvCameraId(const char* id, const char* vendor, const char* model);
  QArvCameraId(const QArvCameraId& camid);
  ~QArvCameraId();
  const char* id, * vendor, * model;
};

Q_DECLARE_METATYPE(QArvCameraId)

// Allow ABI-compatible extensions.
class QArvCameraExtension;

//! QArvCamera provides an interface to an Aravis camera.
/*!
 * This class is mostly a thin wrapper around the arv_camera interface.
 * Only the parts that differ significantly from that interface are documented.
 * The QArvCamera::init() function must be called once in the main program
 * before this class is used.
 *
 * This class implements the QAbstractItemModel interface. This means that it
 * can be used as a data source for widgets such as QTreeView. An QArvCameraDelegate
 * is also provided to facilitate direct access to all camera features. The
 * model has two columns, the first being the name of the feature and the
 * second being the (editable) feature value.
 *
 * When the QAbstractItemModel::dataChanged() signal is emitted, it currently
 * fails to specify which data is affected. This may change in the future.
 */
class QArvCamera : public QAbstractItemModel {
  Q_OBJECT

public:
  //! Initialize glib and aravis. Call this once in the main program.
  static void init();

  //! A camera with the given ID is opened.
  QArvCamera(QArvCameraId id, QObject* parent = NULL);
  ~QArvCamera();

  static QList<QArvCameraId> listCameras(); //!< Returns a list of all cameras found.

  QArvCameraId getId(); //!< Returns the ID of the camera.

  //! \name Manipulate region of interest
  //@{
  QRect getROI();
  void setROI(QRect roi);
  QRect getROIMaxSize();
  //@}

  //! \name Manipulate pixel binning
  /**@{*/
  QSize getBinning();
  void setBinning(QSize bin);
  /**@}*/

  //! \name Choose pixel format
  /**@{*/
  /*!
   * The lists returned by getPixelFormat() and getPixelFormatNames()
   * are congruent.
   */
  QList<QString> getPixelFormats();
  QList<QString> getPixelFormatNames();
  QString getPixelFormat();
  void setPixelFormat(QString format);
  /**@}*/

  //! \name Manipulate frames-per-second.
  /**@{*/
  double getFPS();
  void setFPS(double fps);
  /**@}*/

  //! \name Manipulate exposure time (in microseconds)
  /**@{*/
  double getExposure();
  void setExposure(double exposure);
  QPair<double, double> getExposureLimits();
  bool hasAutoExposure();
  void setAutoExposure(bool enable);
  /**@}*/

  //! \name Manipulate sensor gain
  /**@{*/
  double getGain();
  void setGain(double gain);
  QPair<double, double> getGainLimits();
  bool hasAutoGain();
  void setAutoGain(bool enable);
  /**@}*/

  //! \name Start or stop acquisition
  /**@{*/
  void startAcquisition();
  void stopAcquisition();
  /**@}*/

  //! \name Get a captured frame
  /**@{*/
  QSize getFrameSize();
  QByteArray getFrame(bool dropInvalid = false, bool nocopy = false);
  /**@}*/

  //! \name Manipulate network parameters of an ethernet camera
  /**@{*/
  /*!
   * MTU corresponds to "GevSCPSPacketSize", which should be set to the
   * MTU of the network interface. getHostIP() can be used to detect the
   * interface's address.
   */
  void setMTU(int mtu);
  int getMTU();
  QHostAddress getIP();
  QHostAddress getHostIP();
  int getEstimatedBW();
  /**@}*/

signals:
  //! Emitted when a new frame is ready.
  void frameReady();

private:
  QArvCameraExtension* ext;
  void swapBuffers();
  static QList<QArvCameraId> cameraList;
  ArvCamera* camera;
  ArvDevice* device;
  ArvStream* stream;
  ArvBuffer* currentFrame;
  bool acquiring;

  friend void streamCallback(ArvStream* stream, QArvCamera* cam);
  friend QTextStream& operator<<(QTextStream& out, QArvCamera* camera);
  friend QTextStream& operator>>(QTextStream& in, QArvCamera* camera);
  friend void recursiveSerialization(QTextStream& out,
                                     QArvCamera* camera,
                                     QArvFeatureTree* tree);

public:
  //! \name QAbstractItemModel implementation
  /**@{*/
  QModelIndex index(int row, int column,
                    const QModelIndex& parent = QModelIndex()) const;
  QModelIndex parent(const QModelIndex& index) const;
  int rowCount(const QModelIndex& parent = QModelIndex()) const;
  int columnCount(const QModelIndex& parent = QModelIndex()) const;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
  bool setData(const QModelIndex& index,
               const QVariant& value,
               int role = Qt::EditRole);
  Qt::ItemFlags flags(const QModelIndex& index) const;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const;
  /**@}*/

private:
  ArvGc* genicam;
  QArvFeatureTree* featuretree;
};

//! Serializes camera settings in text form.
QTextStream& operator<<(QTextStream& out, QArvCamera* camera);

//! Reads the textual representation of cammera settings. May not succeed.
QTextStream& operator>>(QTextStream& in, QArvCamera* camera);

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

#endif // ARCAM_H
