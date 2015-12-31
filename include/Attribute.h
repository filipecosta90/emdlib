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

#ifndef EMD_ATTRIBUTE_H
#define EMD_ATTRIBUTE_H

#include "EmdLib.h"

#include "Node.h"
#include "Util.h"

namespace H5
{
	class H5Object;
}

Q_DECLARE_METATYPE(QVector<char>)
Q_DECLARE_METATYPE(QVector<unsigned char>)
Q_DECLARE_METATYPE(QVector<qint16>)
Q_DECLARE_METATYPE(QVector<quint16>)
Q_DECLARE_METATYPE(QVector<qint32>)
Q_DECLARE_METATYPE(QVector<quint32>)
Q_DECLARE_METATYPE(QVector<qint64>)
Q_DECLARE_METATYPE(QVector<quint64>)
Q_DECLARE_METATYPE(QVector<float>)
Q_DECLARE_METATYPE(QVector<double>)
Q_DECLARE_METATYPE(QVector<bool>)
Q_DECLARE_METATYPE(QVector<QString>)

namespace emd
{

class EMDLIB_API Attribute : public Node
{
public:
	Attribute(Node *parent = 0);

	// File operations
	virtual void save(const QString &path, H5::H5Object *group);

	// Accessors
	QVariant value() const {return m_value;}
	bool setValue(const QVariant &value);
	void setType(emd::DataType type) {m_type = type;}
	void setIsArray(const bool &isArray) {m_isArray = isArray;}
	virtual QVariant variantRepresentation() const;

	template <typename T>
	void storeData(const T *data, int length)
	{
		m_length = length;
		if(length == 1)
			m_value = QVariant(data[0]);
		else
		{
			QVector<T> values;
			for(int iii = 0; iii < length; ++iii)
				values.append(data[iii]);
			m_value = QVariant::fromValue< QVector<T> >(values);
			m_isArray = true;
		}
	}

private:

	template <typename T>
	void writeAttribute(const H5::DataType &hdfType, const H5::H5Object *parentObject);

protected:
	QVariant m_value;
	emd::DataType m_type;
	int m_length;
	bool m_isArray;
};

} // namespace emd

#endif