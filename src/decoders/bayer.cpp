/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2014 Jure Varlec <jure.varlec@ad-vega.si>

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

#include "decoders/bayer.h"

using namespace QArv;

Q_EXPORT_PLUGIN2(BayerGR8, BayerGR8)
Q_IMPORT_PLUGIN(BayerGR8)
Q_EXPORT_PLUGIN2(BayerRG8, BayerRG8)
Q_IMPORT_PLUGIN(BayerRG8)
Q_EXPORT_PLUGIN2(BayerGB8, BayerGB8)
Q_IMPORT_PLUGIN(BayerGB8)
Q_EXPORT_PLUGIN2(BayerBG8, BayerBG8)
Q_IMPORT_PLUGIN(BayerBG8)

Q_EXPORT_PLUGIN2(BayerGR10, BayerGR10)
Q_IMPORT_PLUGIN(BayerGR10)
Q_EXPORT_PLUGIN2(BayerRG10, BayerRG10)
Q_IMPORT_PLUGIN(BayerRG10)
Q_EXPORT_PLUGIN2(BayerGB10, BayerGB10)
Q_IMPORT_PLUGIN(BayerGB10)
Q_EXPORT_PLUGIN2(BayerBG10, BayerBG10)
Q_IMPORT_PLUGIN(BayerBG10)

Q_EXPORT_PLUGIN2(BayerGR12, BayerGR12)
Q_IMPORT_PLUGIN(BayerGR12)
Q_EXPORT_PLUGIN2(BayerRG12, BayerRG12)
Q_IMPORT_PLUGIN(BayerRG12)
Q_EXPORT_PLUGIN2(BayerGB12, BayerGB12)
Q_IMPORT_PLUGIN(BayerGB12)
Q_EXPORT_PLUGIN2(BayerBG12, BayerBG12)
Q_IMPORT_PLUGIN(BayerBG12)

#ifdef ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED
Q_EXPORT_PLUGIN2(BayerGR12_PACKED, BayerGR12_PACKED)
Q_IMPORT_PLUGIN(BayerGR12_PACKED)
#endif
#ifdef ARV_PIXEL_FORMAT_BAYER_RG_12_PACKED
Q_EXPORT_PLUGIN2(BayerRG12_PACKED, BayerRG12_PACKED)
Q_IMPORT_PLUGIN(BayerRG12_PACKED)
#endif
#ifdef ARV_PIXEL_FORMAT_BAYER_GB_12_PACKED
Q_EXPORT_PLUGIN2(BayerGB12_PACKED, BayerGB12_PACKED)
Q_IMPORT_PLUGIN(BayerGB12_PACKED)
#endif
Q_EXPORT_PLUGIN2(BayerBG12_PACKED, BayerBG12_PACKED)
Q_IMPORT_PLUGIN(BayerBG12_PACKED)

#ifdef ARV_PIXEL_FORMAT_BAYER_GR_16
Q_EXPORT_PLUGIN2(BayerGR16, BayerGR16)
Q_IMPORT_PLUGIN(BayerGR16)
Q_EXPORT_PLUGIN2(BayerRG16, BayerRG16)
Q_IMPORT_PLUGIN(BayerRG16)
Q_EXPORT_PLUGIN2(BayerGB16, BayerGB16)
Q_IMPORT_PLUGIN(BayerGB16)
Q_EXPORT_PLUGIN2(BayerBG16, BayerBG16)
Q_IMPORT_PLUGIN(BayerBG16)
#endif
