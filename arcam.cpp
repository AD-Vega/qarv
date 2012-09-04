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

/*!
 * Acquisition mode is set to CONTINUOUS when the camera is opened.
 */
ArCam::ArCam(ArCamId id, QObject* parent):
  QObject(parent), acquiring(false) {
  camera = arv_camera_new(id.id);
  arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS);
  device = arv_camera_get_device(camera);
}

ArCam::~ArCam() {
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
