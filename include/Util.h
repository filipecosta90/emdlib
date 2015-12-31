/*
 * emdlib, a library for reading and writing electron microscopy dataset 
 * (emd) files.
 * Copyright (C) 2015  Phil Ophus
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef EMD_UTIL_H
#define EMD_UTIL_H

#include "EmdLib.h"

#include <limits>
#include <stdint.h>
#include <vector>

#include <QVariant>

class QString;

const float kInvalidFloatValue = -std::numeric_limits<float>::max();

namespace H5
{
    class DataSet;
	class DataType;
}

namespace emd
{

enum DataType
{
	DataTypeUnknown = -1,

	DataTypeInt8	= 0x1,
	DataTypeInt16	= 0x2,
	DataTypeInt32	= 0x4,
	DataTypeInt64	= 0x8,

	DataTypeUInt8	= 0x10,
	DataTypeUInt16	= 0x20,
	DataTypeUInt32	= 0x40,
	DataTypeUInt64	= 0x80,

	DataTypeFloat32	= 0x100,
	DataTypeFloat64	= 0x200,

	DataTypeString	= 0x1000,
	DataTypeBool	= 0x2000,

	DataTypeArray	= 0x10000
};

EMDLIB_API DataType hdfToEmdType(const H5::DataType &h5Type);
EMDLIB_API DataType dataTypeFromHdfDataSet(const H5::DataSet &dataSet);
EMDLIB_API H5::DataType emdToHdfType(const emd::DataType &emdType);
EMDLIB_API int emdTypeDepth(const DataType &type);
EMDLIB_API QString emdTypeString(const DataType &type);
EMDLIB_API QVariant emdTypeFromString(const QString &string, const DataType &type);
EMDLIB_API bool isFloatType(DataType type);

EMDLIB_API const std::vector<int> &randomIndexes(const int &length, const int &max);

template <typename T>
struct TypeTraits 
{
    static DataType dataType() {return DataTypeUnknown;}
};

template<>
struct TypeTraits<uint8_t>
{
    static DataType dataType() {return DataTypeUInt8;}
};

template<>
struct TypeTraits<uint16_t>
{
    static DataType dataType() {return DataTypeUInt16;}
};

template<>
struct TypeTraits<uint32_t>
{
    static DataType dataType() {return DataTypeUInt32;}
};

template<>
struct TypeTraits<uint64_t>
{
    static DataType dataType() {return DataTypeUInt64;}
};

template<>
struct TypeTraits<int8_t>
{
    static DataType dataType() {return DataTypeInt8;}
};

template<>
struct TypeTraits<int16_t>
{
    static DataType dataType() {return DataTypeInt16;}
};

template<>
struct TypeTraits<int32_t>
{
    static DataType dataType() {return DataTypeInt32;}
};

template<>
struct TypeTraits<int64_t>
{
    static DataType dataType() {return DataTypeInt64;}
};

template<>
struct TypeTraits<float>
{
    static DataType dataType() {return DataTypeFloat32;}
};

template<>
struct TypeTraits<double>
{
    static DataType dataType() {return DataTypeFloat64;}
};

template<>
struct TypeTraits<bool>
{
    static DataType dataType() {return DataTypeBool;}
};

template<>
struct TypeTraits<char *>
{
    static DataType dataType() {return DataTypeString;}
};

}

Q_DECLARE_METATYPE(emd::DataType);

#endif