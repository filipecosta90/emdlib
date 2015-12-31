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

#include "Util.h"

#include <H5Cpp.h>

#include <QString>
#include <QVariant>

namespace emd
{

DataType hdfToEmdType(const H5::DataType &h5Type)
{
	DataType emdType = DataTypeUnknown;

	H5T_class_t dataClass = h5Type.getClass();
	int depth = (int) h5Type.getSize();
	if(H5T_FLOAT == dataClass)
	{
		switch(depth)
		{
		case 4:
			emdType = DataTypeFloat32;
			break;
		case 8:
			emdType = DataTypeFloat64;
			break;
		}
	}
	else if(H5T_INTEGER == dataClass)
	{
		H5T_sign_t sign = H5Tget_sign(h5Type.getId());
		if(sign == H5T_SGN_NONE)		// unsigned
		{
			switch(depth)
			{
			case 1:
				emdType = DataTypeUInt8;
				break;
			case 2:
				emdType = DataTypeUInt16;
				break;
			case 4:
				emdType = DataTypeUInt32;
				break;
			case 8:
				emdType = DataTypeUInt64;
				break;
			}
		}
		else if(sign == H5T_SGN_2)		// two's complement
		{
			switch(depth)
			{
			case 1:
				emdType = DataTypeInt8;
				break;
			case 2:
				emdType = DataTypeInt16;
				break;
			case 4:
				emdType = DataTypeInt32;
				break;
			case 8:
				emdType = DataTypeInt64;
				break;
			}
		}
	}
	else if(H5T_STRING == dataClass)
		emdType = DataTypeString;

	return emdType;
}

DataType dataTypeFromHdfDataSet(const H5::DataSet &dataSet)
{
    // Data element size
	H5T_class_t dataClass = dataSet.getTypeClass();
	H5::DataType type;
	if(H5T_FLOAT == dataClass)
	{
		type = dataSet.getFloatType();
	}
	else if(H5T_INTEGER == dataClass)
	{
		type = dataSet.getIntType();
	}
	else if(H5T_STRING == dataClass)
	{
		type = dataSet.getStrType();
	}

    return hdfToEmdType(type);
}

H5::DataType emdToHdfType(const DataType &emdType)
{
	H5::DataType hdfType = 0;
	switch(emdType)
	{
	case DataTypeInt8:
		hdfType = H5::PredType::NATIVE_INT8;
		break;
	case DataTypeUInt8:
		hdfType = H5::PredType::NATIVE_UINT8;
		break;
	case DataTypeInt16:
		hdfType = H5::PredType::NATIVE_INT16;
		break;
	case DataTypeUInt16:
		hdfType = H5::PredType::NATIVE_UINT16;
		break;
	case DataTypeInt32:
		hdfType = H5::PredType::NATIVE_INT32;
		break;
	case DataTypeUInt32:
		hdfType = H5::PredType::NATIVE_UINT32;
		break;
	case DataTypeInt64:
		hdfType = H5::PredType::NATIVE_INT64;
		break;
	case DataTypeUInt64:
		hdfType = H5::PredType::NATIVE_UINT64;
		break;
	case DataTypeFloat32:
		hdfType = H5::PredType::NATIVE_FLOAT;
		break;
	case DataTypeFloat64:
		hdfType = H5::PredType::NATIVE_DOUBLE;
		break;
	case DataTypeString:
		hdfType = H5::PredType::C_S1;
		break;
	default:
		break;
	}
	return hdfType;
}

int emdTypeDepth(const DataType &type)
{
	int depth = -1;
	switch(type)
	{
	case DataTypeInt8:
	case DataTypeUInt8:
		depth = 1;
		break;
	case DataTypeInt16:
	case DataTypeUInt16:
		depth = 2;
		break;
	case DataTypeInt32:
	case DataTypeUInt32:
	case DataTypeFloat32:
		depth = 4;
		break;
	case DataTypeInt64:
	case DataTypeUInt64:
	case DataTypeFloat64:
		depth = 8;
		break;
	case DataTypeString:
		depth = 0;
	default:
		break;
	}
	return depth;
}

QString emdTypeString(const DataType &type)
{
	QString string;
	switch(type)
	{
	case DataTypeInt8:
		string = "int8";
		break;
	case DataTypeUInt8:
		string = "uint8";
		break;
	case DataTypeInt16:
		string = "int16";
		break;
	case DataTypeUInt16:
		string = "uint16";
		break;
	case DataTypeInt32:
		string = "int32";
		break;
	case DataTypeUInt32:
		string = "uint32";
		break;
	case DataTypeFloat32:
		string = "float";
		break;
	case DataTypeInt64:
		string = "int64";
		break;
	case DataTypeUInt64:
		string = "uint64";
		break;
	case DataTypeFloat64:
		string = "double";
		break;
	case DataTypeString:
		string = "string";
		break;
	case DataTypeBool:
		string = "bool";
		break;
	default:
		break;
	}
	return string;
}

bool isFloatType(DataType type)
{
    switch(type)
	{
	case DataTypeInt8:
	case DataTypeUInt8:
	case DataTypeInt16:
	case DataTypeUInt16:
	case DataTypeInt32:
	case DataTypeUInt32:
	case DataTypeInt64:
	case DataTypeUInt64:
		return false;
	case DataTypeFloat32:
	case DataTypeFloat64:
		return true;
	case DataTypeString:
	case DataTypeBool:
	default:
		break;
	}

	return false;
}

QVariant emdTypeFromString(const QString &string, const DataType &type)
{
	bool ok;
	switch(type)
	{
	case DataTypeInt8:
		{
			int8_t result = (int8_t) string.toShort(&ok);
			if(ok)
				return QVariant(result);
		}
		break;
	case DataTypeUInt8:
		{
			uint8_t result = (uint8_t) string.toUShort(&ok);
			if(ok)
				return QVariant(result);
		}
		break;
	case DataTypeInt16:
		{
			int16_t result = string.toShort(&ok);
			if(ok)
				return QVariant(result);
		}
		break;
	case DataTypeUInt16:
		{
			int16_t result = string.toUShort(&ok);
			if(ok)
				return QVariant(result);
		}
		break;
	case DataTypeInt32:
		{
			int32_t result = string.toInt(&ok);
			if(ok)
				return QVariant(result);
		}
		break;
	case DataTypeUInt32:
		{
			int32_t result = string.toUInt(&ok);
			if(ok)
				return QVariant(result);
		}
		break;
	case DataTypeInt64:
		{
			int64_t result = string.toLong(&ok);
			if(ok)
				return QVariant(result);
		}
		break;
	case DataTypeUInt64:
		{
			int64_t result = string.toULong(&ok);
			if(ok)
				return QVariant(result);
		}
		break;
	case DataTypeFloat32:
		{
			float result = string.toFloat(&ok);
			if(ok)
				return QVariant(result);
		}
		break;
	case DataTypeFloat64:
		{
			double result = string.toDouble(&ok);
			if(ok)
				return QVariant(result);
		}
		break;
	case DataTypeString:
		return QVariant(string);
		break;
	default:
		break;
	}

	return QVariant(0);
}

// Note: not at all thread safe.
const std::vector<int> &randomIndexes(const int &length, const int &max)
{
    static int s_length = 0;
    static int s_max = 0;
	static unsigned int s_seed = 0xfaceface;
    static std::vector<int> s_indexes;

	if(length != s_length || max != s_max)
    {
        s_indexes.clear();
	    s_indexes.reserve(length);
	    for(int iii = 0; iii < length; ++iii)
	    {
		    s_seed ^= (s_seed << 13);
		    s_seed ^= (s_seed >> 17);
		    s_seed ^= (s_seed << 5);
		    s_indexes.push_back(s_seed % max);
	    }

        s_length = length;
        s_max = max;
    }

    return s_indexes;
}

} // namespace emd

