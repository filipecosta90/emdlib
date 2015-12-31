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

#include "DataSpace.h"

#include <H5Cpp.h>

#include <QDebug>

namespace emd
{

DataSpace::DataSpace(const int rank, const int *dimLengths)
	: m_rank(rank)
{
    if(rank > 0 && dimLengths)
    {
	    for(int iii = 0; iii < rank; ++iii)
		    m_dimLengths.append(dimLengths[iii]);
    }
}

DataSpace::DataSpace(int length)
    : m_rank(1)
{
    m_dimLengths.append(length);
}

DataSpace DataSpace::fromHdfDataSet(const H5::DataSet &dataSet)
{
	H5::DataSpace space = dataSet.getSpace();
	int rank = space.getSimpleExtentNdims();
	int *dimLengths = new int[rank];
	hsize_t *dims = new hsize_t[rank];
	space.getSimpleExtentDims(dims, NULL);

	// Convert the dimension lengths from long long to int. Probably we
	//	won't be dealing with dimensions with length > 2 billion.
	for(int iii = 0; iii < rank; ++iii)
		dimLengths[iii] = (int) dims[iii];

	DataSpace emdSpace(rank, dimLengths);

	delete[] dims;
	delete[] dimLengths;

	return emdSpace;
}

int DataSpace::rank() const
{
	return m_rank;
}

int DataSpace::dimLength(int index) const
{
	if(index < 0 || index > (m_rank - 1))
		return 0;

	return m_dimLengths.at(index);
}

bool DataSpace::isValid() const
{
    return (m_rank > 0 && m_rank == m_dimLengths.size());
}

QString DataSpace::stringRepresentation() const
{
	QString val;
	for(int iii = 0; iii < m_rank; ++iii)
	{
		if(iii > 0)
			val += " x ";
		val += QString::number(m_dimLengths[iii]);
	}
	return val;
}

} // namespace emd
