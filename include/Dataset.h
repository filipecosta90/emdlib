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

#ifndef EMD_DATA_H
#define EMD_DATA_H

#include "EmdLib.h"

#include "Node.h"

#include <vector>

#include "DataSpace.h"
#include "Util.h"

namespace H5
{
class H5Object;
}

namespace emd
{

class Frame;

class EMDLIB_API Dataset : public Node
{
public:
    typedef std::vector<int> Slice;

    static const int HorizontalDimension = -1;
    static const int VerticalDimension = -2;

    enum class DimensionRole
    {
        Displayed,
        Free,
        Selection,
        Special
    };

	typedef struct {
		int start;
		int end;
	} Range;
	
	typedef std::vector<Range> Selection;

    Dataset(const Dataset &other);
	Dataset(Node *parent = 0);
	Dataset(const int &length, const emd::DataType &type, bool createDefault = false);
	Dataset(const int &nDims, int const *dimLengths, emd::DataType type, 
		char *data, bool descendingData = true);
	virtual ~Dataset();

	// File operations
	void loadData(const H5::DataSet &dataSet);
	void unloadData();
    bool isLoaded() const;
	virtual void save(const QString &path, H5::H5Object *group);

	// Accessors
	QString dataName() const;
	emd::DataType dataType() const;
	QString units() const;
	int dimCount() const;
	int dimLength(const int &dimIndex) const;
	void setDataOrder(bool descending) {m_descendingData = descending;}
	bool dataOrder() const {return m_descendingData;}
	const DataSpace& dataSpace() const {return m_space;}
    Slice defaultSlice() const;
	void setDataSpace(const DataSpace &space);
    void setDataType(DataType type);
	int complexIndex() const;
	void setComplexIndex(const int &index);
	bool isComplexDim() const;
	Selection selectAll() const;
    
    void setTrueLength(int length);
	
	virtual QVariant variantRepresentation() const;
	QVariant variantDataValue(const int &index) const;
    QVariant variantDataValue(const Slice &state) const;
    QString valueString(const int &index) const;

	Frame *frame(const Slice &slice) const;

private:
    template <typename T>
    T value(int index) const
    {
        T *data = (T*) m_data;
        return data[index];
    }

    template <typename T>
    T extrapolate(int index) const
    {
        T* data = (T*) m_data;
        return data[0] + index * (data[1] - data[0]);
    }

private:
	char *m_data;
	DataSpace m_space;
    DataType m_dataType;
    int m_dataTypeSize;
	bool m_descendingData;			// data order; ascending = dim1, dim2, ..., dimN
									//			   descending = dimN, dimN-1, ..., dim1
	int m_complexIndex;
    bool m_truncatedDim;
    int m_trueLength;
};

//template <typename T>
//class DatasetIterator
//{
//public:
//	DatasetIterator(const Dataset *data, Dataset::Selection = std::vector<Dataset::Range>());
//	DatasetIterator(const Dataset *data, Dataset::State state);
//
//	const T &operator*();
//	void operator++();
//
//	void initialize();
//	void nextFrame();
//
//private:
//	const Dataset * const m_data;
//	Dataset::Selection m_selection;
//	T *m_currentValue;
//};

} // namespace emd

#endif