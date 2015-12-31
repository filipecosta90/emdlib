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

#ifndef EMD_DATASPACE_H
#define EMD_DATASPACE_H

#include "EmdLib.h"

#include <qvector.h>
#include <QString>

#include "Util.h"

namespace H5 
{
	class DataSet;
}

namespace emd
{

class EMDLIB_API DataSpace
{
public:
	DataSpace(int rank, const int *dimLengths);
    DataSpace(int length);

	static DataSpace fromHdfDataSet(const H5::DataSet &dataSet);

	int rank() const;
	int dimLength(int index) const;

    bool isValid() const;

	QString stringRepresentation() const;

private:
	int m_rank;
	QVector<int> m_dimLengths;
};

} // namespace emd

#endif
