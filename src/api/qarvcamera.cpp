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

extern "C" {
  #include <arv.h>
  #include <gio/gio.h>
  #include <sys/socket.h>
}
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "api/qarvcamera.h"
#include <QDebug>
#include <QComboBox>
#include <QLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTextDocument>

class QArvCameraExtension {};

QList<QArvCameraId> QArvCamera::cameraList;

void QArvCamera::init() {
  g_type_init();
  arv_enable_interface("Fake");
}

QArvCameraId::QArvCameraId() : id(NULL), vendor(NULL), model(NULL) {}

QArvCameraId::QArvCameraId(const char* id_,
                           const char* vendor_,
                           const char* model_) {
  id = strdup(id_);
  vendor = strdup(vendor_);
  model = strdup(model_);
}

QArvCameraId::QArvCameraId(const QArvCameraId& camid) {
  id = strdup(camid.id);
  vendor = strdup(camid.vendor);
  model = strdup(camid.model);
}

QArvCameraId::~QArvCameraId() {
  if (id != NULL) free((void*)id);
  if (vendor != NULL) free((void*)vendor);
  if (model != NULL) free((void*)model);
}

QArvFeatureTree* createFeaturetree(ArvGc* cam);
void freeFeaturetree(QArvFeatureTree* tree);

/*!
 * Acquisition mode is set to CONTINUOUS when the camera is opened.
 */
QArvCamera::QArvCamera(QArvCameraId id, QObject* parent) :
  QAbstractItemModel(parent), acquiring(false) {
  ext = new QArvCameraExtension;
  setFrameQueueSize();
  camera = arv_camera_new(id.id);
  arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS);
  device = arv_camera_get_device(camera);
  genicam = arv_device_get_genicam(device);
  featuretree = createFeaturetree(genicam);
}

QArvCamera::~QArvCamera() {
  delete ext;
  freeFeaturetree(featuretree);
  stopAcquisition();
  g_object_unref(camera);
}

/*!
 * A list of camera IDs is created by opening each camera to obtain the vendor
 * and model names.
 *
 * TODO: see if this info can be obtained using a lower level API.
 */
QList<QArvCameraId> QArvCamera::listCameras() {
  cameraList.clear();
  arv_update_device_list();
  unsigned int N = arv_get_n_devices();
  for (unsigned int i = 0; i < N; i++) {
    const char* camid = arv_get_device_id(i);
    ArvCamera* camera = arv_camera_new(camid);
    QArvCameraId id(camid, arv_camera_get_vendor_name(camera),
                    arv_camera_get_model_name(camera));
    g_object_unref(camera);
    cameraList << id;
  }
  return QList<QArvCameraId>(cameraList);
}

QArvCameraId QArvCamera::getId() {
  const char* id, * vendor, * model;
  id = arv_camera_get_device_id(camera);
  vendor = arv_camera_get_vendor_name(camera);
  model = arv_camera_get_model_name(camera);
  return QArvCameraId(id, vendor, model);
}

QRect QArvCamera::getROI() {
  int x, y, width, height;
  arv_camera_get_region(camera, &x, &y, &width, &height);
  return QRect(x, y, width, height);
}

QRect QArvCamera::getROIMaxSize() {
  int xmin, xmax, ymin, ymax;
  arv_camera_get_width_bounds(camera, &xmin, &xmax);
  arv_camera_get_height_bounds(camera, &ymin, &ymax);
  QRect retval;
  retval.setCoords(xmin, ymin, xmax, ymax);
  return retval;
}

void QArvCamera::setROI(QRect roi) {
  int x, y, width, height;
  roi.getRect(&x, &y, &width, &height);
  arv_camera_set_region(camera, x, y, width, height);
  emit dataChanged(QModelIndex(), QModelIndex());
}

QSize QArvCamera::getBinning() {
  int x, y;
  arv_camera_get_binning(camera, &x, &y);
  return QSize(x, y);
}

void QArvCamera::setBinning(QSize bin) {
  arv_camera_set_binning(camera, bin.width(), bin.height());
  emit dataChanged(QModelIndex(), QModelIndex());
}

QList< QString > QArvCamera::getPixelFormats() {
  unsigned int numformats;
  const char** formats =
    arv_camera_get_available_pixel_formats_as_strings(camera, &numformats);
  QList<QString> list;
  for (int i = 0; i < numformats; i++)
    list << formats[i];
  free(formats);
  return list;
}

QList< QString > QArvCamera::getPixelFormatNames() {
  unsigned int numformats;
  const char** formats =
    arv_camera_get_available_pixel_formats_as_display_names(camera,
                                                            &numformats);
  QList<QString> list;
  for (int i = 0; i < numformats; i++) {
    list << formats[i];
  }
  free(formats);
  return list;
}

QString QArvCamera::getPixelFormat() {
  return QString(arv_camera_get_pixel_format_as_string(camera));
}

void QArvCamera::setPixelFormat(QString format) {
  auto tmp = format.toAscii();
  arv_camera_set_pixel_format_from_string(camera, tmp.constData());
  emit dataChanged(QModelIndex(), QModelIndex());
}

double QArvCamera::getFPS() {
  return arv_camera_get_frame_rate(camera);
}

void QArvCamera::setFPS(double fps) {
  arv_camera_set_frame_rate(camera, fps);
  emit dataChanged(QModelIndex(), QModelIndex());
}

int QArvCamera::getMTU() {
  return arv_device_get_integer_feature_value(device, "GevSCPSPacketSize");
}

void QArvCamera::setMTU(int mtu) {
  arv_device_set_integer_feature_value(device, "GevSCPSPacketSize", mtu);
  arv_device_set_integer_feature_value(device, "GevSCBWR", 10);
  emit dataChanged(QModelIndex(), QModelIndex());
}

double QArvCamera::getExposure() {
  return arv_camera_get_exposure_time(camera);
}

void QArvCamera::setExposure(double exposure) {
  arv_camera_set_exposure_time(camera, exposure);
  emit dataChanged(QModelIndex(), QModelIndex());
}

bool QArvCamera::hasAutoExposure() {
  return arv_camera_is_exposure_auto_available(camera);
}

void QArvCamera::setAutoExposure(bool enable) {
  if (enable)
    arv_camera_set_exposure_time_auto(camera, ARV_AUTO_CONTINUOUS);
  else
    arv_camera_set_exposure_time_auto(camera, ARV_AUTO_OFF);
  emit dataChanged(QModelIndex(), QModelIndex());
}

double QArvCamera::getGain() {
  return arv_camera_get_gain(camera);
}

void QArvCamera::setGain(double gain) {
  arv_camera_set_gain(camera, gain);
  emit dataChanged(QModelIndex(), QModelIndex());
}

QPair< double, double > QArvCamera::getExposureLimits() {
  double expomin, expomax;
  arv_camera_get_exposure_time_bounds(camera, &expomin, &expomax);
  return QPair<double, double>(expomin, expomax);
}

QPair< double, double > QArvCamera::getGainLimits() {
  double gainmin, gainmax;
  arv_camera_get_gain_bounds(camera, &gainmin, &gainmax);
  return QPair<double, double>(gainmin, gainmax);
}

bool QArvCamera::hasAutoGain() {
  return arv_camera_is_gain_auto_available(camera);
}

void QArvCamera::setAutoGain(bool enable) {
  if (enable)
    arv_camera_set_gain_auto(camera, ARV_AUTO_CONTINUOUS);
  else
    arv_camera_set_gain_auto(camera, ARV_AUTO_OFF);
  emit dataChanged(QModelIndex(), QModelIndex());
}

//! Store the pointer to the current frame.
void QArvCamera::swapBuffers() {
  arv_stream_push_buffer(stream, currentFrame);
  currentFrame = arv_stream_pop_buffer(stream);
  emit frameReady();
}

//! A wrapper that calls cam's private callback to accept frames.
void streamCallback(ArvStream* stream, QArvCamera* cam) {
  cam->swapBuffers();
}

/*!
 * This function not only starts acquisition, but also pushes a number of
 * frames onto the stream and sets up the callback which accepts frames.
 */
void QArvCamera::startAcquisition() {
  if (acquiring) return;
  unsigned int framesize = arv_camera_get_payload(camera);
  currentFrame = arv_buffer_new(framesize, NULL);
  stream = arv_camera_create_stream(camera, NULL, NULL);
  arv_stream_set_emit_signals(stream, TRUE);
  g_signal_connect(stream, "new-buffer", G_CALLBACK(streamCallback), this);
  for (uint i = 0; i < frameQueueSize; i++) {
    arv_stream_push_buffer(stream, arv_buffer_new(framesize, NULL));
  }
  arv_camera_start_acquisition(camera);
  acquiring = true;
  emit dataChanged(QModelIndex(), QModelIndex());
}

void QArvCamera::stopAcquisition() {
  if (!acquiring) return;
  arv_camera_stop_acquisition(camera);
  g_object_unref(currentFrame);
  g_object_unref(stream);
  acquiring = false;
  emit dataChanged(QModelIndex(), QModelIndex());
}

//! Set the number of frames on the stream. Takes effect on startAcquisition().
/*! An Aravis stream has a queue of frame buffers which is cycled as frames are
 * acquired. The getFrame() function returns the frame that is currently being
 * cycled. Several frames should be put on the queue for smooth operation. More
 * should be used when using the nocopy facility of getFrame(), as this
 * increases the grace period in which the returned frame is valid. For
 * example, with the queue size of 30 and framerate of 60 FPS, the grace period
 * is approximately one half second. Increasing the queue size increases the
 * memory usage, as all buffers are allocated when acquisition starts.
 */
void QArvCamera::setFrameQueueSize(uint size) {
  frameQueueSize = size;
}

QSize QArvCamera::getFrameSize() {
  return getROI().size();
}

/*!
 * \param dropInvalid If true, return an empty QByteArray on an invalid frame.
 * \param nocopy If true, the  returned QByteArray doesn't own the data. The
 * caller guarantees that the frame will be used "quickly", i.e. before it is
 * used to capture the next image. Only use this if the caller will decode or
 * otherwise copy the data immediately. Note that the array itself is never
 * invalidated after the grace period passes, it is merely overwritten.
 * \param rawbuffer If not NULL, the pointer to the Aravis' frame struct is
 * stored here. It can be used to get additional information about the frame,
 * such as its timestamp. The caller must provide similar guarantees as if the
 * nocopy parameter were set to true.
 * \return A QByteArray with raw frame data of size given by getFrameSize().
 */
QByteArray QArvCamera::getFrame(bool dropInvalid,
                                bool nocopy,
                                ArvBuffer** rawbuffer) {
  if (rawbuffer != NULL) *rawbuffer = currentFrame;
  if (currentFrame->status != ARV_BUFFER_STATUS_SUCCESS && dropInvalid)
    return QByteArray();
  else {
    if (nocopy)
      return QByteArray::fromRawData(static_cast<char*>(currentFrame->data),
                                     currentFrame->size);
    else
      return QByteArray(static_cast<char*>(currentFrame->data),
                        currentFrame->size);
  }
}

//! Translates betwen the glib and Qt address types via a native sockaddr.
QHostAddress GSocketAddress_to_QHostAddress(GSocketAddress* gaddr) {
  sockaddr addr;
  int success = g_socket_address_to_native(gaddr, &addr, sizeof(addr), NULL);
  if (!success) {
    qDebug() << "Unable to translate IP address.";
    return QHostAddress();
  }
  return QHostAddress(&addr);
}

QHostAddress QArvCamera::getIP() {
  if (ARV_IS_GV_DEVICE(device)) {
    auto gaddr = arv_gv_device_get_device_address(ARV_GV_DEVICE(device));
    return GSocketAddress_to_QHostAddress(gaddr);
  } else return QHostAddress();
}

QHostAddress QArvCamera::getHostIP() {
  if (ARV_IS_GV_DEVICE(device)) {
    auto gaddr = arv_gv_device_get_interface_address(ARV_GV_DEVICE(device));
    return GSocketAddress_to_QHostAddress(gaddr);
  } else return QHostAddress();
}

int QArvCamera::getEstimatedBW() {
  return arv_device_get_integer_feature_value(device, "GevSCDCT");
}

QTextStream& operator<<(QTextStream& out, QArvCamera* camera) {
  auto id = camera->getId();
  out << id.vendor << endl
      << id.model << endl
      << id.id << endl;
  recursiveSerialization(out, camera, camera->featuretree);
  return out;
}

/*!
 * This operator simply reads settings into the camera in the order they are
 * provided. This means that dependencies between features are not honored.
 * Because of this, loading may fail.
 */
QTextStream& operator>>(QTextStream& in, QArvCamera* camera) {
  auto ID = camera->getId();
  QString vendor, model, id;
  in >> vendor >> model >> id;
  if (!(vendor == ID.vendor && model == ID.model && id == ID.id)) {
    qWarning() << QObject::tr("Incompatible camera settings", "QArvCamera");
    return in;
  }

  while (!in.atEnd()) {
    QString name, type, v;
    in >> name;
    if (name == "Category") continue;
    ArvGcFeatureNode* node =
      ARV_GC_FEATURE_NODE(arv_gc_get_node(camera->genicam, name.toAscii()));

    in >> type;
    in >> v;
    if (type == "Register") {
      QString hex;
      in >> hex;
      auto b = QByteArray::fromHex(hex.toAscii());
      arv_gc_register_set(ARV_GC_REGISTER(node), v.data(), v.toLongLong(),
                          NULL);
    } else if (type == "Enumeration") {
      arv_gc_enumeration_set_string_value(ARV_GC_ENUMERATION(node),
                                          v.toAscii().data(), NULL);
    } else if (type == "String") {
      arv_gc_string_set_value(ARV_GC_STRING(node), v.toAscii().data(), NULL);
    } else if (type == "Float") {
      arv_gc_float_set_value(ARV_GC_FLOAT(node), v.toDouble(), NULL);
    } else if (type == "Boolean") {
      arv_gc_boolean_set_value(ARV_GC_BOOLEAN(node), v.toInt(), NULL);
    } else if (type == "Integer") {
      arv_gc_integer_set_value(ARV_GC_INTEGER(node), v.toLongLong(), NULL);
    }
  }

  return in;
}

/* QAbstractItemModel implementation ######################################## */

//! A class that stores the hirearchy of camera features.
/*! String identifiers are used to get feature nodes from Aravis. At first it
 * seems that a QAbstractItemModel can be implemented by only using Aravis
 * functions to walk the feature hirearchy, but it turns out there is no way
 * to find a feature's parent that way. Also, string identifiers returned by
 * Aravis are not persistent and need to be copied. Therefore, a tree to store
 * feature identifiers is used by the model. It is assumed that the hirearchy
 * is static.
 */
class QArvFeatureTree {
public:
  QArvFeatureTree(QArvFeatureTree* parent = NULL, const char* feature = NULL);
  ~QArvFeatureTree();
  QArvFeatureTree* parent();
  QList<QArvFeatureTree*> children();
  const char* feature();
  int row();

private:
  void addChild(QArvFeatureTree* child);
  void removeChild(QArvFeatureTree* child);
  QArvFeatureTree* parent_;
  QList<QArvFeatureTree*> children_;
  const char* feature_;
};

QArvFeatureTree::QArvFeatureTree(QArvFeatureTree* parent,
                                 const char* feature) :
  children_() {
  if (feature == NULL) feature_ = strdup("");
  else feature_ = strdup(feature);
  parent_ = parent;
  if (parent_ != NULL) parent_->addChild(this);
}

QArvFeatureTree::~QArvFeatureTree() {
  delete feature_;
  if (parent_ != NULL) parent_->removeChild(this);
  for (auto child = children_.begin(); child != children_.end(); child++)
    delete *child;
}

void QArvFeatureTree::addChild(QArvFeatureTree* child) {
  children_ << child;
}

QList< QArvFeatureTree* > QArvFeatureTree::children() {
  return children_;
}

const char* QArvFeatureTree::feature() {
  return feature_;
}

QArvFeatureTree* QArvFeatureTree::parent() {
  return parent_;
}

void QArvFeatureTree::removeChild(QArvFeatureTree* child) {
  children_.removeAll(child);
}

int QArvFeatureTree::row() {
  if (parent_ == NULL) return 0;
  auto litter = parent_->children();
  return litter.indexOf(this);
}

//! Walk the Aravis feature tree and copy it as an QArvFeatureTree.
/**@{*/
void recursiveMerge(ArvGc* cam, QArvFeatureTree* tree, ArvGcNode* node) {
  const GSList* child = arv_gc_category_get_features(ARV_GC_CATEGORY(node));
  for (; child != NULL; child = child->next) {
    ArvGcNode* newnode = arv_gc_get_node(cam, (const char*)(child->data));
    auto newtree = new QArvFeatureTree(tree, (const char*)(child->data));
    if (ARV_IS_GC_CATEGORY(newnode)) recursiveMerge(cam, newtree, newnode);
  }
}

QArvFeatureTree* createFeaturetree(ArvGc* cam) {
  QArvFeatureTree* tree = new QArvFeatureTree(NULL, "Root");
  ArvGcNode* node = arv_gc_get_node(cam, tree->feature());
  recursiveMerge(cam, tree, node);
  return tree;
}
/**@}*/

void freeFeaturetree(QArvFeatureTree* tree) {
  auto children = tree->children();
  for (auto child = children.begin(); child != children.end(); child++)
    freeFeaturetree(*child);
  delete tree;
}

//! Serialize the tree, used by QArvCamera stream operators.
void recursiveSerialization(QTextStream& out, QArvCamera* camera,
                            QArvFeatureTree* tree) {
  auto node = arv_gc_get_node(camera->genicam, tree->feature());
  ArvGcFeatureNode* fnode = ARV_GC_FEATURE_NODE(node);

  if (tree->children().count() != 0) {
    if (QString("Root") != tree->feature())
      out << "Category: " << tree->feature() << endl;
    foreach (auto child, tree->children()) {
      recursiveSerialization(out, camera, child);
    }
    return;
  }

  if (ARV_IS_GC_COMMAND(node)) return;

  out << "\t" << tree->feature() << "\t";
  if (ARV_IS_GC_REGISTER_NODE(node)
      && QString(arv_dom_node_get_node_name(ARV_DOM_NODE(node)))
      == "IntReg") {
    QArvRegister r;
    r.length = arv_gc_register_get_length(ARV_GC_REGISTER(node), NULL);
    r.value = QByteArray(r.length, 0);
    arv_gc_register_get(ARV_GC_REGISTER(node),
                        r.value.data(), r.length, NULL);
    out << "Register\t" << QString::number(r.length) << "\t"
        << QString("0x") + r.value.toHex() << endl;
  } else if (ARV_IS_GC_ENUMERATION(node)) {
    out << "Enumeration\t"
        << arv_gc_enumeration_get_string_value(ARV_GC_ENUMERATION(node), NULL)
        << endl;
  } else if (ARV_IS_GC_STRING(node)) {
    out << "String\t" << arv_gc_string_get_value(ARV_GC_STRING(node), NULL)
        << endl;
  } else if (ARV_IS_GC_FLOAT(node)) {
    out << "Float\t" << arv_gc_float_get_value(ARV_GC_FLOAT(node), NULL)
        << "\t" << arv_gc_float_get_unit(ARV_GC_FLOAT(node), NULL) << endl;
  } else if (ARV_IS_GC_BOOLEAN(node)) {
    out << "Boolean\t" << arv_gc_boolean_get_value(ARV_GC_BOOLEAN(node),
                                                   NULL) << endl;
  } else if (ARV_IS_GC_INTEGER(node)) {
    out << "Integer\t" << arv_gc_integer_get_value(ARV_GC_INTEGER(node),
                                                   NULL) << endl;
  }
}

QModelIndex QArvCamera::index(int row, int column,
                              const QModelIndex& parent) const {
  if (column > 1) return QModelIndex();
  QArvFeatureTree* treenode;
  if (!parent.isValid()) treenode = featuretree;
  else treenode = static_cast<QArvFeatureTree*>(parent.internalPointer());
  auto children = treenode->children();
  if (row < 0 || row >= children.size()) return QModelIndex();
  auto child = children.at(row);
  ArvGcNode* node = arv_gc_get_node(genicam, child->feature());
  if (ARV_IS_GC_CATEGORY(node) && column > 0) return QModelIndex();
  return createIndex(row, column, child);
}

QModelIndex QArvCamera::parent(const QModelIndex& index) const {
  QArvFeatureTree* treenode;
  if (!index.isValid()) treenode = featuretree;
  else treenode = static_cast<QArvFeatureTree*>(index.internalPointer());
  if (treenode->parent() == NULL) return QModelIndex();
  auto parent = treenode->parent();
  return createIndex(parent == NULL ? 0 : parent->row(), 0, parent);
}

int QArvCamera::columnCount(const QModelIndex& parent) const {
  return 2;
}

int QArvCamera::rowCount(const QModelIndex& parent) const {
  QArvFeatureTree* treenode;
  if (!parent.isValid()) treenode = featuretree;
  else treenode = static_cast<QArvFeatureTree*>(parent.internalPointer());
  return treenode->children().count();
}

QVariant QArvCamera::data(const QModelIndex& index, int role) const {
  QArvFeatureTree* treenode;
  if (!index.isValid()) treenode = featuretree;
  else treenode = static_cast<QArvFeatureTree*>(index.internalPointer());
  ArvGcNode* node = arv_gc_get_node(genicam, treenode->feature());

  if (!ARV_IS_GC_FEATURE_NODE(node)) {
    qDebug() << "data:" << "Node" << treenode->feature() << "is not valid!";
    return QVariant();
  }

  const char* string;
  ArvGcFeatureNode* fnode = ARV_GC_FEATURE_NODE(node);

  if (index.column() == 0) {
    switch (role) {
    case Qt::DisplayRole:
      string =
        arv_gc_feature_node_get_display_name(ARV_GC_FEATURE_NODE(node), NULL);
      if (string == NULL)
        string = arv_gc_feature_node_get_name(ARV_GC_FEATURE_NODE(node));
      if (string == NULL) {
        qDebug() << "Node has no name!?";
        return QVariant();
      }
      return QVariant(string);

    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
      string =
        arv_gc_feature_node_get_description(ARV_GC_FEATURE_NODE(node), NULL);
      if (string == NULL) return QVariant();
      return QVariant("<qt/>" + Qt::escape(string));

    default:
      return QVariant();
    }
  } else if (index.column() == 1) {
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
      if (ARV_IS_GC_REGISTER_NODE(node)
          && QString(arv_dom_node_get_node_name(ARV_DOM_NODE(node)))
          == "IntReg") {
        QArvRegister r;
        r.length = arv_gc_register_get_length(ARV_GC_REGISTER(node), NULL);
        r.value = QByteArray(r.length, 0);
        arv_gc_register_get(ARV_GC_REGISTER(node),
                            r.value.data(), r.length, NULL);
        if (role == Qt::DisplayRole)
          return QVariant::fromValue((QString)r);
        else
          return QVariant::fromValue(r);
      }
      if (ARV_IS_GC_ENUMERATION(node)) {
        QArvEnumeration e;
        const GSList* entry =
          arv_gc_enumeration_get_entries(ARV_GC_ENUMERATION(node));
        for (; entry != NULL; entry = entry->next) {
          bool isAvailable =
            arv_gc_feature_node_is_available(ARV_GC_FEATURE_NODE(entry->data),
                                             NULL);
          bool isImplemented =
            arv_gc_feature_node_is_implemented(ARV_GC_FEATURE_NODE(entry->data),
                                               NULL);
          e.values
            << arv_gc_feature_node_get_name(ARV_GC_FEATURE_NODE(entry->data));
          const char* name =
            arv_gc_feature_node_get_display_name(ARV_GC_FEATURE_NODE(entry->
                                                                     data),
                                                 NULL);
          if (name == NULL)
            e.names << e.values.last();
          else
            e.names << name;
          if (isAvailable && isImplemented)
            e.isAvailable << true;
          else
            e.isAvailable << false;
        }
        const char* current =
          arv_gc_enumeration_get_string_value(ARV_GC_ENUMERATION(node), NULL);
        e.currentValue = e.values.indexOf(current);
        if (role == Qt::DisplayRole)
          return QVariant::fromValue((QString)e);
        else
          return QVariant::fromValue(e);
      }
      if (ARV_IS_GC_COMMAND(node)) {
        QArvCommand c;
        if (role == Qt::DisplayRole)
          return QVariant::fromValue((QString)c);
        else
          return QVariant::fromValue(c);
      }
      if (ARV_IS_GC_STRING(node)) {
        QArvString s;
        s.value = arv_gc_string_get_value(ARV_GC_STRING(node), NULL);
        s.maxlength = arv_gc_string_get_max_length(ARV_GC_STRING(node), NULL);
        if (role == Qt::DisplayRole)
          return QVariant::fromValue((QString)s);
        else
          return QVariant::fromValue(s);
      }
      if (ARV_IS_GC_FLOAT(node)) {
        QArvFloat f;
        f.value = arv_gc_float_get_value(ARV_GC_FLOAT(node), NULL);
        f.min = arv_gc_float_get_min(ARV_GC_FLOAT(node), NULL);
        f.max = arv_gc_float_get_max(ARV_GC_FLOAT(node), NULL);
        f.unit = arv_gc_float_get_unit(ARV_GC_FLOAT(node), NULL);
        if (role == Qt::DisplayRole)
          return QVariant::fromValue((QString)f);
        else
          return QVariant::fromValue(f);
      }
      if (ARV_IS_GC_BOOLEAN(node)) {
        QArvBoolean b;
        b.value = arv_gc_boolean_get_value(ARV_GC_BOOLEAN(node), NULL);
        if (role == Qt::DisplayRole)
          return QVariant::fromValue((QString)b);
        else
          return QVariant::fromValue(b);
      }
      if (ARV_IS_GC_INTEGER(node)) {
        QArvInteger i;
        i.value = arv_gc_integer_get_value(ARV_GC_INTEGER(node), NULL);
        i.min = arv_gc_integer_get_min(ARV_GC_INTEGER(node), NULL);
        i.max = arv_gc_integer_get_max(ARV_GC_INTEGER(node), NULL);
        i.inc = arv_gc_integer_get_inc(ARV_GC_INTEGER(node), NULL);
        if (role == Qt::DisplayRole)
          return QVariant::fromValue((QString)i);
        else
          return QVariant::fromValue(i);
      }
      return QVariant();
    }
  } else return QVariant();
  return QVariant();
}

bool QArvCamera::setData(const QModelIndex& index, const QVariant& value,
                         int role) {
  QAbstractItemModel::setData(index, value, role);
  if (!(index.model()->flags(index) & Qt::ItemIsEnabled)
      || !(index.model()->flags(index) & Qt::ItemIsEditable))
    return false;

  QArvFeatureTree* treenode;
  if (!index.isValid()) treenode = featuretree;
  else treenode = static_cast<QArvFeatureTree*>(index.internalPointer());
  ArvGcFeatureNode* node =
    ARV_GC_FEATURE_NODE(arv_gc_get_node(genicam, treenode->feature()));

  if (value.canConvert<QArvRegister>()) {
    auto r = qvariant_cast<QArvRegister>(value);
    arv_gc_register_set(ARV_GC_REGISTER(node), r.value.data(), r.length, NULL);
  } else if (value.canConvert<QArvEnumeration>()) {
    auto e = qvariant_cast<QArvEnumeration>(value);
    if (e.isAvailable.at(e.currentValue)) {
      arv_gc_enumeration_set_string_value(ARV_GC_ENUMERATION(node),
                                          e.values.at(
                                            e.currentValue).toAscii().data(),
                                          NULL);
    } else return false;
  } else if (value.canConvert<QArvCommand>()) {
    arv_gc_command_execute(ARV_GC_COMMAND(node), NULL);
  } else if (value.canConvert<QArvString>()) {
    auto s = qvariant_cast<QArvString>(value);
    arv_gc_string_set_value(ARV_GC_STRING(node), s.value.toAscii().data(),
                            NULL);
  } else if (value.canConvert<QArvFloat>()) {
    auto f = qvariant_cast<QArvFloat>(value);
    arv_gc_float_set_value(ARV_GC_FLOAT(node), f.value, NULL);
  } else if (value.canConvert<QArvBoolean>()) {
    auto b = qvariant_cast<QArvBoolean>(value);
    arv_gc_boolean_set_value(ARV_GC_BOOLEAN(node), b.value, NULL);
  } else if (value.canConvert<QArvInteger>()) {
    auto i = qvariant_cast<QArvInteger>(value);
    arv_gc_integer_set_value(ARV_GC_INTEGER(node), i.value, NULL);
  } else
    return false;

  emit dataChanged(QModelIndex(), QModelIndex());
  return true;
}

Qt::ItemFlags QArvCamera::flags(const QModelIndex& index) const {
  auto f = QAbstractItemModel::flags(index);
  if (!index.isValid()) return f;
  QArvFeatureTree* treenode;
  treenode = static_cast<QArvFeatureTree*>(index.internalPointer());
  ArvGcFeatureNode* node =
    ARV_GC_FEATURE_NODE(arv_gc_get_node(genicam, treenode->feature()));
  if (index.column() != 1 && !ARV_IS_GC_CATEGORY(node)) {
    f = flags(sibling(index.row(), 1, index));
    f &= ~Qt::ItemIsEditable;
  } else {
    int enabled =
      arv_gc_feature_node_is_available(node, NULL)
      && arv_gc_feature_node_is_implemented(node, NULL)
      && !arv_gc_feature_node_is_locked(node, NULL);
    if (!enabled) {
      f &= ~Qt::ItemIsEnabled;
      f &= ~Qt::ItemIsEditable;
    } else
      f |= Qt::ItemIsEditable;
  }
  return f;
}

QVariant QArvCamera::headerData(int section, Qt::Orientation orientation,
                                int role) const {
  switch (section) {
  case 0:
    return QVariant::fromValue(QString("Feature"));

  case 1:
    return QVariant::fromValue(QString("Value"));
  }
  return QVariant();
}

QArvCameraDelegate::QArvCameraDelegate(QObject* parent) :
  QStyledItemDelegate(parent) {}

QWidget* QArvCameraDelegate::createEditor(QWidget* parent,
                                          const QStyleOptionViewItem& option,
                                          const QModelIndex& index) const {
  auto var = index.model()->data(index, Qt::EditRole);
  assert(var.isValid());
  auto val = static_cast<QArvType*>(var.data());
  auto editor = val->createEditor(parent);
  this->connect(editor, SIGNAL(editingFinished()), SLOT(finishEditing()));
  return editor;
}

void QArvCameraDelegate::setEditorData(QWidget* editor,
                                       const QModelIndex& index) const {
  auto var = index.model()->data(index, Qt::EditRole);
  assert(var.isValid());
  auto val = static_cast<QArvType*>(var.data());
  val->populateEditor(editor);
}

void QArvCameraDelegate::setModelData(QWidget* editor,
                                      QAbstractItemModel* model,
                                      const QModelIndex& index) const {
  auto var = model->data(index, Qt::EditRole);
  assert(var.isValid());
  auto val = static_cast<QArvType*>(var.data());
  val->readFromEditor(editor);
  model->setData(index, var);
}

void QArvCameraDelegate::updateEditorGeometry(QWidget* editor,
                                              const QStyleOptionViewItem& option,
                                              const QModelIndex& index) const {
  editor->layout()->setContentsMargins(0, 0, 0, 0);
  editor->setGeometry(option.rect);
}

void QArvCameraDelegate::finishEditing() {
  auto editor = qobject_cast<QWidget*>(sender());
  emit commitData(editor);
  emit closeEditor(editor);
}

QArvEditor* QArvEnumeration::createEditor(QWidget* parent) const {
  auto editor = new QArvEditor(parent);
  auto select = new QComboBox(editor);
  select->setObjectName("selectEnum");
  auto layout = new QHBoxLayout;
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
