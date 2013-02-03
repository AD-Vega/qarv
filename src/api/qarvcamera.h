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

#ifndef QARVCAMERA_H
#define QARVCAMERA_H

#include <QList>
#include <QString>
#include <QRect>
#include <QPair>
#include <QMetaType>
#include <QImage>
#include <QHostAddress>
#include <QAbstractItemModel>

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

  class QArvCameraExtension;
  class QArvFeatureTree;

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

  //! \name Control acquisition
  /**@{*/
  void startAcquisition();
  void stopAcquisition();
  void setFrameQueueSize(uint size = 30);
  /**@}*/

  //! \name Get a captured frame
  /**@{*/
  QSize getFrameSize();
  QByteArray getFrame(bool dropInvalid = false, bool nocopy = false,
                      ArvBuffer** rawbuffer = NULL);
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
  uint frameQueueSize;

  friend void streamCallback(ArvStream* stream, QArvCamera* cam);
  friend QTextStream& operator<<(QTextStream& out, QArvCamera* camera);
  friend QTextStream& operator>>(QTextStream& in, QArvCamera* camera);

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

#endif // ARCAM_H
