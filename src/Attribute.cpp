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

#include "Attribute.h"

#include "H5Cpp.h"

#include <QDebug>
#include <QStringList>

#ifndef H5_NO_NAMESPACE
using namespace H5;
#endif

namespace emd
{

static const int MAX_ARRAY_DISPLAY_SIZE = 4;

Attribute::Attribute(Node *p)
	: Node(p),
	m_length(1),
	m_isArray(false),
	m_value(0)
{

}

/***************************** File Operations **************************/

template <typename T>
void Attribute::writeAttribute(const H5::DataType &hdfType, const H5Object *parentObject)
{
	// Get the char* representation of the name
	QByteArray ba = m_name.toLocal8Bit();	
	const char *name = ba.constData();
	htri_t exists = H5Aexists(parentObject->getId(), name);
	hsize_t length = m_length;
	H5::DataSpace space(1, &length);
	H5::Attribute atr;
	if(exists < 1)
		atr = parentObject->createAttribute(name, hdfType, space);
	else
		atr = parentObject->openAttribute(name);

	if(m_isArray)
	{
		QVector<T> vec = m_value.value<QVector<T> >();
		const T *val = vec.constData();
		atr.write(hdfType, val);
	}
	else
	{
		T val = m_value.value<T>();
		atr.write(hdfType, &val);
	}
	
	atr.close();
}

void Attribute::save(const QString &/*path*/, H5Object *parentObject)
{
	if(!parentObject)
		return;

	// If the attribute is not dirty it doesn't need to be saved
	if(m_status & DIRTY) 
	{
	try 
	{
		removeStatus(DIRTY);

		if(m_type == DataTypeString)
		{
			// Get the char* representation of the name
			QByteArray ba = m_name.toLocal8Bit();	
			const char *name = ba.constData();
			QString s = m_value.value<QString>();

			hid_t strType = H5Tcopy(H5T_C_S1);
			H5Tset_size(strType, s.length());
			hid_t attr;
			hsize_t length = m_length;
			H5::DataSpace space(1, &length);
			htri_t exists = H5Aexists(parentObject->getId(), name);
			if(exists < 1)
			{
				attr = H5Acreate(parentObject->getId(), name, strType, 
					space.getId(), H5P_DEFAULT, H5P_DEFAULT);
			}
			else
			{
				attr = H5Aopen(parentObject->getId(), name, NULL);
				hid_t type = H5Aget_type(attr);
				// If the attribute is a variable length string,
				//	we destroy it and recreate it as fixed-length.
				if(H5Tis_variable_str(type) || H5Tget_size(type) != s.length())
				{
					H5Aclose(attr);
					H5Adelete(parentObject->getId(), name);
					//parentObject->removeAttr(name);
					attr = H5Acreate(parentObject->getId(), name, strType, 
						space.getId(), H5P_DEFAULT, H5P_DEFAULT);
				}
				H5Tclose(type);
			}
				
			ba = s.toLocal8Bit();
			const char *val = ba.constData();
			H5Awrite(attr, strType, val);
			H5Aclose(attr);
		}
		else
		{
			switch(m_type)
			{
			case DataTypeInt8:
				{
					IntType type(PredType::NATIVE_CHAR);
					writeAttribute<char>(type, parentObject);
					break;
				}
			case DataTypeUInt8:
				{
					IntType type(PredType::NATIVE_UCHAR);
					writeAttribute<unsigned char>(type, parentObject);
					break;
				}
			case DataTypeInt16:
				{
					IntType type(PredType::NATIVE_SHORT);
					writeAttribute<qint16>(type, parentObject);
					break;
				}
			case DataTypeUInt16:
				{
					IntType type(PredType::NATIVE_USHORT);
					writeAttribute<quint16>(type, parentObject);
					break;
				}
			case DataTypeInt32:
				{
					IntType type(PredType::NATIVE_INT);
					writeAttribute<qint32>(type, parentObject);
					break;
				}
			case DataTypeUInt32:
				{
					IntType type(PredType::NATIVE_UINT);
					writeAttribute<quint32>(type, parentObject);
					break;
				}
			case DataTypeInt64:
				{
					IntType type(PredType::NATIVE_LONG);
					writeAttribute<qint64>(type, parentObject);
					break;
				}
			case DataTypeUInt64:
				{
					IntType type(PredType::NATIVE_ULONG);
					writeAttribute<quint64>(type, parentObject);
					break;
				}
			case DataTypeFloat32:
				{
					FloatType type(PredType::NATIVE_FLOAT);
					writeAttribute<float>(type, parentObject);
					break;
				}
			case DataTypeFloat64:
				{
					FloatType type(PredType::NATIVE_DOUBLE);
					writeAttribute<double>(type, parentObject);
					break;
				}
			case DataTypeBool:
				{
					IntType type(PredType::NATIVE_HBOOL);
					writeAttribute<bool>(type, parentObject);
					break;
				}
			default:
				qWarning() << "Error saving attribute " << m_name
							<< ": unrecognized data type";
			}
		}
	}
	catch(AttributeIException error) {
		qDebug() << "Attribute save failed: " << m_name;
	}
	}
}

/******************************** Accessors *************************************/

bool Attribute::setValue(const QVariant &value)
{
	if(m_isArray)
		return false;

	m_value = value;
	setStatus(Node::DIRTY);
	return true;
}

template <typename T>
QVariant variantValueString(const QVector<T> &vec, const emd::DataType &type)
{
	QString result("");
	int size = vec.size();

	if(size <= MAX_ARRAY_DISPLAY_SIZE)
	{
		for(int iii = 0; iii < size; ++iii)
		{
			if(iii > 0)
			{
				if(type == DataTypeString)
					result += ", ";
				else
					result += " ";
			}
			result += QVariant(vec.at(iii)).toString();
		}
	}
	else
	{
		result = emd::emdTypeString(type) + " array " + QString("1 x %1").arg(size);
	}

	return QVariant(result);
}

QVariant Attribute::variantRepresentation() const
{
	if( !m_isArray )
		return m_value;

	QVariant result;

	switch(m_type)
	{
	case DataTypeInt8:
		{
			QVector<char> vector = m_value.value<QVector<char> >();
			result = variantValueString(vector, m_type);
			break;
		}
	case DataTypeUInt8:
		{
			QVector<unsigned char> vector = m_value.value<QVector<unsigned char> >();
			result = variantValueString(vector, m_type);
			break;
		}
	case DataTypeInt16:
		{
			QVector<qint16> vector = m_value.value<QVector<qint16> >();
			result = variantValueString(vector, m_type);
			break;
		}
	case DataTypeUInt16:
		{
			QVector<quint16> vector = m_value.value<QVector<quint16> >();
			result = variantValueString(vector, m_type);
			break;
		}
	case DataTypeInt32:
		{
			QVector<qint32> vector = m_value.value<QVector<qint32> >();
			result = variantValueString(vector, m_type);
			break;
		}
	case DataTypeUInt32:
		{
			QVector<quint32> vector = m_value.value<QVector<quint32> >();
			result = variantValueString(vector, m_type);
			break;
		}
	case DataTypeInt64:
		{
			QVector<qint64> vector = m_value.value<QVector<qint64> >();
			result = variantValueString(vector, m_type);
			break;
		}
	case DataTypeUInt64:
		{
			QVector<quint64> vector = m_value.value<QVector<quint64> >();
			result = variantValueString(vector, m_type);
			break;
		}
	case DataTypeFloat32:
		{
			QVector<float> vector = m_value.value<QVector<float> >();
			result = variantValueString(vector, m_type);
			break;
		}
	case DataTypeFloat64:
		{
			QVector<double> vector = m_value.value<QVector<double> >();
			result = variantValueString(vector, m_type);
			break;
		}
	case DataTypeString:
		{
			QVector<QString> vector = m_value.value<QVector<QString> >();
			result = variantValueString(vector, m_type);
			break;
		}
	case DataTypeBool:
		{
			QVector<bool> vector = m_value.value<QVector<bool> >();
			result = variantValueString(vector, m_type);
			break;
		}
	default:
		break;
	}

	return result;
}

} // namespace emd
