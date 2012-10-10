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

#include "arcam.h"
#include <QDebug>

QList<ArCamId> ArCam::cameraList;

void arcamInit() {
  g_type_init();
  arv_enable_interface("Fake");
}

ArCamId::ArCamId(): id(NULL), vendor(NULL), model(NULL) {}

ArCamId::ArCamId(const char* id_, const char* vendor_, const char* model_) {
  id = strdup(id_);
  vendor = strdup(vendor_);
  model = strdup(model_);
}

ArCamId::ArCamId(const ArCamId& camid) {
  id = strdup(camid.id);
  vendor = strdup(camid.vendor);
  model = strdup(camid.model);
}

ArCamId::~ArCamId() {
  if(id != NULL) free((void*)id);
  if(vendor != NULL) free((void*)vendor);
  if(model != NULL) free((void*)model);
}

ArFeatureTree* createFeaturetree(ArvGc* cam);
void freeFeaturetree(ArFeatureTree* tree);

/*!
 * Acquisition mode is set to CONTINUOUS when the camera is opened.
 */
ArCam::ArCam(ArCamId id, QObject* parent):
  QAbstractItemModel(parent), acquiring(false) {
  camera = arv_camera_new(id.id);
  arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS);
  device = arv_camera_get_device(camera);
  genicam = arv_device_get_genicam(device);
  featuretree = createFeaturetree(genicam);
}

ArCam::~ArCam() {
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
QList<ArCamId> ArCam::listCameras() {
  cameraList.clear();
  arv_update_device_list();
  unsigned int N = arv_get_n_devices();
  for (unsigned int i = 0; i < N; i++) {
    const char* camid = arv_get_device_id(i);
    ArvCamera* camera = arv_camera_new(camid);
    ArCamId id(camid, arv_camera_get_vendor_name(camera),
               arv_camera_get_model_name(camera));
    g_object_unref(camera);
    cameraList << id;
  }
  return QList<ArCamId>(cameraList);
}

ArCamId ArCam::getId() {
  const char *id, *vendor, *model;
  id = arv_camera_get_device_id(camera);
  vendor = arv_camera_get_vendor_name(camera);
  model = arv_camera_get_model_name(camera);
  return ArCamId(id, vendor, model);
}

QRect ArCam::getROI() {
  int x, y, width, height;
  arv_camera_get_region(camera, &x, &y, &width, &height);
  return QRect(x, y, width, height);
}

QRect ArCam::getROIMaxSize() {
  int xmin, xmax, ymin, ymax;
  arv_camera_get_width_bounds(camera, &xmin, &xmax);
  arv_camera_get_height_bounds(camera, &ymin, &ymax);
  QRect retval;
  retval.setCoords(xmin, ymin, xmax, ymax);
  return retval;
}

void ArCam::setROI(QRect roi) {
  int x, y, width, height;
  roi.getRect(&x,&y, &width, &height);
  arv_camera_set_region(camera, x, y, width, height);
}

QSize ArCam::getBinning() {
  int x, y;
  arv_camera_get_binning(camera, &x, &y);
  return QSize(x, y);
}

void ArCam::setBinning(QSize bin) {
  arv_camera_set_binning(camera, bin.width(), bin.height());
}

QList< QString > ArCam::getPixelFormats() {
  unsigned int numformats;
  const char ** formats =
    arv_camera_get_available_pixel_formats_as_strings(camera, &numformats);
  QList<QString> list;
  for (int i=0; i<numformats; i++)
    list << formats[i];
  free(formats);
  return list;
}

QList< QString > ArCam::getPixelFormatNames() {
  unsigned int numformats;
  const char ** formats =
    arv_camera_get_available_pixel_formats_as_display_names(camera, &numformats);
  QList<QString> list;
  for (int i=0; i<numformats; i++) {
    list << formats[i];
  }
  free(formats);
  return list;
}

QString ArCam::getPixelFormat() {
  return QString(arv_camera_get_pixel_format_as_string(camera));
}

void ArCam::setPixelFormat(QString format) {
  auto tmp = format.toAscii();
  arv_camera_set_pixel_format_from_string(camera, tmp.constData());
}

double ArCam::getFPS() {
  return arv_camera_get_frame_rate(camera);
}

void ArCam::setFPS(double fps) {
  arv_camera_set_frame_rate(camera, fps);
}

int ArCam::getMTU() {
  return arv_device_get_integer_feature_value(device, "GevSCPSPacketSize");
}

void ArCam::setMTU(int mtu) {
  arv_device_set_integer_feature_value(device, "GevSCPSPacketSize", mtu);
  arv_device_set_integer_feature_value(device, "GevSCBWR", 10);
}

double ArCam::getExposure() {
  return arv_camera_get_exposure_time(camera);
}

void ArCam::setExposure(double exposure) {
  arv_camera_set_exposure_time(camera, exposure);
}

bool ArCam::hasAutoExposure() {
  return arv_camera_is_exposure_auto_available(camera);
}

void ArCam::setAutoExposure(bool enable) {
  if (enable)
    arv_camera_set_exposure_time_auto(camera, ARV_AUTO_CONTINUOUS);
  else
    arv_camera_set_exposure_time_auto(camera, ARV_AUTO_OFF);
}

double ArCam::getGain() {
  return arv_camera_get_gain(camera);
}

void ArCam::setGain(double gain) {
  arv_camera_set_gain(camera, gain);
}

QPair< double, double > ArCam::getExposureLimits() {
  double expomin, expomax;
  arv_camera_get_exposure_time_bounds(camera, &expomin, &expomax);
  return QPair<double, double>(expomin, expomax);
}

QPair< double, double > ArCam::getGainLimits() {
  double gainmin, gainmax;
  arv_camera_get_gain_bounds(camera, &gainmin, &gainmax);
  return QPair<double, double>(gainmin, gainmax);
}

bool ArCam::hasAutoGain() {
  return arv_camera_is_gain_auto_available(camera);
}

void ArCam::setAutoGain(bool enable) {
  if(enable)
    arv_camera_set_gain_auto(camera, ARV_AUTO_CONTINUOUS);
  else
    arv_camera_set_gain_auto(camera, ARV_AUTO_OFF);
}

//! Store the pointer to the current frame.
void ArCam::swapBuffers() {
  arv_stream_push_buffer(stream, currentFrame);
  currentFrame = arv_stream_pop_buffer(stream);
  emit frameReady();
}

//! A wrapper that calls cam's private callback to accept frames.
void streamCallback(ArvStream* stream, ArCam* cam) {
  cam->swapBuffers();
}

/*!
 * This function not only starts acquisition, but also pushes a number of
 * frames onto the stream and sets up the callback which accepts frames.
 */
void ArCam::startAcquisition() {
  if (acquiring) return;
  unsigned int framesize = arv_camera_get_payload(camera);
  currentFrame = arv_buffer_new(framesize, NULL);
  stream = arv_camera_create_stream(camera, NULL, NULL);
  arv_stream_set_emit_signals(stream, TRUE);
  g_signal_connect(stream, "new-buffer", G_CALLBACK(streamCallback), this);
  for (int i=0; i<100; i++) {
    arv_stream_push_buffer(stream, arv_buffer_new(framesize, NULL));
  }
  arv_camera_start_acquisition(camera);
  acquiring = true;
}

void ArCam::stopAcquisition() {
  if (!acquiring) return;
  arv_camera_stop_acquisition(camera);
  g_object_unref(currentFrame);
  g_object_unref(stream);
  acquiring = false;
}

QSize ArCam::getFrameSize() {
  return getROI().size();
}

/*!
 * \param dropInvalid If true, return an empty QByteArray on an invalid frame.
 * \return A QByteArray with raw frame data of size given by getFrameSize().
 */
QByteArray ArCam::getFrame(bool dropInvalid) {
  if(currentFrame->status != ARV_BUFFER_STATUS_SUCCESS && dropInvalid)
    return QByteArray();
  else
    return QByteArray(static_cast<char*>(currentFrame->data), currentFrame->size);
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

QHostAddress ArCam::getIP() {
  if (ARV_IS_GV_DEVICE(device)) {
    auto gaddr = arv_gv_device_get_device_address(ARV_GV_DEVICE(device));
    return GSocketAddress_to_QHostAddress(gaddr);
  } else return QHostAddress();
}

QHostAddress ArCam::getHostIP() {
  if (ARV_IS_GV_DEVICE(device)) {
    auto gaddr = arv_gv_device_get_interface_address(ARV_GV_DEVICE(device));
    return GSocketAddress_to_QHostAddress(gaddr);
  } else return QHostAddress();
}

int ArCam::getEstimatedBW() {
  return arv_device_get_integer_feature_value(device, "GevSCDCT");
}

/* QAbstractItemModel implementation ######################################## */

class ArFeatureTree {
public:
  ArFeatureTree(ArFeatureTree* parent = NULL, const char* feature = NULL);
  ~ArFeatureTree();
  ArFeatureTree* parent();
  QList<ArFeatureTree*> children();
  const char* feature();
  int row();

private:
  void addChild(ArFeatureTree* child);
  void removeChild(ArFeatureTree* child);
  ArFeatureTree* parent_;
  QList<ArFeatureTree*> children_;
  const char* feature_;
};

ArFeatureTree::ArFeatureTree(ArFeatureTree* parent, const char* feature):
  children_() {
  if (feature == NULL) feature_ = strdup("");
  else feature_ = strdup(feature);
  parent_ = parent;
  if (parent_ != NULL) parent_->addChild(this);
}

ArFeatureTree::~ArFeatureTree() {
  delete feature_;
  if (parent_ != NULL) parent_->removeChild(this);
  for (auto child = children_.begin(); child != children_.end(); child++)
    delete *child;
}

void ArFeatureTree::addChild(ArFeatureTree* child) {
  children_ << child;
}

QList< ArFeatureTree* > ArFeatureTree::children() {
  return children_;
}

const char* ArFeatureTree::feature() {
  return feature_;
}

ArFeatureTree* ArFeatureTree::parent() {
  return parent_;
}

void ArFeatureTree::removeChild(ArFeatureTree* child) {
  children_.removeAll(child);
}

int ArFeatureTree::row() {
  if (parent_ == NULL) return 0;
  auto litter = parent_->children();
  return litter.indexOf(this);
}

void recursiveMerge(ArvGc* cam, ArFeatureTree* tree, ArvGcNode* node) {
  const GSList* child = arv_gc_category_get_features(ARV_GC_CATEGORY(node));
  for (; child != NULL; child = child->next) {
    ArvGcNode* newnode = arv_gc_get_node(cam, (const char*)(child->data));
    auto newtree = new ArFeatureTree(tree, (const char*)(child->data));
    if (ARV_IS_GC_CATEGORY(newnode)) recursiveMerge(cam, newtree, newnode);
  }
}

ArFeatureTree* createFeaturetree(ArvGc* cam) {
  ArFeatureTree* tree = new ArFeatureTree(NULL, "Root");
  ArvGcNode* node = arv_gc_get_node(cam, tree->feature());
  recursiveMerge(cam, tree, node);
  return tree;
}

void freeFeaturetree(ArFeatureTree* tree) {
  auto children = tree->children();
  for (auto child = children.begin(); child != children.end(); child++)
    freeFeaturetree(*child);
  delete tree;
}

QModelIndex ArCam::index(int row, int column, const QModelIndex& parent) const {
  if(column > 1) return QModelIndex();
  ArFeatureTree* treenode;
  if (!parent.isValid()) treenode = featuretree;
  else treenode = static_cast<ArFeatureTree*>(parent.internalPointer());
  auto children = treenode->children();
  if (row < 0 || row >= children.size()) return QModelIndex();
  auto child = children.at(row);
  ArvGcNode* node = arv_gc_get_node(genicam, child->feature());
  if (ARV_IS_GC_CATEGORY(node) && column > 0) return QModelIndex();
  return createIndex(row, column, child);
}

QModelIndex ArCam::parent(const QModelIndex& index) const {
  ArFeatureTree* treenode;
  if (!index.isValid()) treenode = featuretree;
  else treenode = static_cast<ArFeatureTree*>(index.internalPointer());
  if (treenode->parent() == NULL) return QModelIndex();
  auto parent = treenode->parent();
  return createIndex(parent == NULL ? 0 : parent->row(), 0, parent);
}

int ArCam::columnCount(const QModelIndex& parent) const {
  return 2;
}

int ArCam::rowCount(const QModelIndex& parent) const {
  ArFeatureTree* treenode;
  if (!parent.isValid()) treenode = featuretree;
  else treenode = static_cast<ArFeatureTree*>(parent.internalPointer());
  return treenode->children().count();
}

QVariant ArCam::data(const QModelIndex& index, int role) const {
  ArFeatureTree* treenode;
  if (!index.isValid()) treenode = featuretree;
  else treenode = static_cast<ArFeatureTree*>(index.internalPointer());
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
      return QVariant(string);
    case Qt::UserRole:
      if (ARV_IS_GC_INTEGER(node)) return QVariant("int");
      if (ARV_IS_GC_FLOAT(node)) return QVariant("float");
      if (ARV_IS_GC_STRING(node)) return QVariant("string");
      if (ARV_IS_GC_ENUMERATION(node)) return QVariant("enum");
      if (ARV_IS_GC_COMMAND(node)) return QVariant("command");
      if (ARV_IS_GC_BOOLEAN(node)) return QVariant("bool");
      if (ARV_IS_GC_REGISTER(node)) return QVariant("register");
      if (ARV_IS_GC_CATEGORY(node)) return QVariant("category");
      if (ARV_IS_GC_PORT(node)) return QVariant("port");
    default:
      return QVariant();
    }
  } else if (index.column() == 1) {
    switch (role) {
      case Qt::DisplayRole:
      case Qt::EditRole:
      if (ARV_IS_GC_ENUMERATION(node))
        return QVariant(arv_gc_feature_node_get_value_as_string(fnode, NULL));
      if (ARV_IS_GC_STRING(node))
        return arv_gc_string_get_value(ARV_GC_STRING(node), NULL);
      if (ARV_IS_GC_FLOAT(node))
        return arv_gc_float_get_value(ARV_GC_FLOAT(node), NULL);
      if (ARV_IS_GC_INTEGER(node))
        return (quint64)(arv_gc_integer_get_value(ARV_GC_INTEGER(node), NULL));
      if (ARV_IS_GC_BOOLEAN(node))
        return (bool)(arv_gc_boolean_get_value(ARV_GC_BOOLEAN(node), NULL));
      return QVariant(arv_gc_feature_node_get_value_as_string(fnode, NULL));
    }
  } else return QVariant();
  return QVariant();
}

Qt::ItemFlags ArCam::flags(const QModelIndex& index) const {
  return QAbstractItemModel::flags(index);
}

QVariant ArCam::headerData(int section, Qt::Orientation orientation, int role) const {
  switch (section) {
  case 0:
    return QVariant(QString("Feature"));
  case 1:
    return QVariant(QString("Value"));
  }
  return QVariant();
}
