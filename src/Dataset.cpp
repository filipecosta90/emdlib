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

#include "Dataset.h"

#include <stack>

#include "H5Cpp.h"

#include <QDebug>
#include <QString>

#include "Attribute.h"
#include "Frame.h"

#ifndef H5_NO_NAMESPACE
using namespace H5;
#endif

namespace emd
{

const uint64_t MEMORY_LIMIT = 2048LL * 1024LL * 1024LL * 1024LL;	// 2TB

Dataset::Dataset(const Dataset &other)
    : m_space(other.m_space)
{

}

Dataset::Dataset(Node *p)
	: Node(p),
    m_space(0),
    m_dataType(DataTypeUnknown),
    m_dataTypeSize(0),
	m_complexIndex(-1),
    m_truncatedDim(false)
{
	m_data = 0;
	m_descendingData = true;
}

Dataset::Dataset(const int &length, const emd::DataType &type, bool createDefault)
	: m_space(1, &length),
    m_dataType(type),
	m_complexIndex(-1),
    m_truncatedDim(false)
{
	m_data = NULL;
	m_descendingData = false;	// Doesn't matter for 1D data
    m_dataTypeSize = emdTypeDepth(m_dataType);

	if(createDefault)
	{
		m_data = new char[length * m_dataTypeSize];

		// TODO: temp
		if(type == DataTypeInt32)
		{
			int *intData = (int*)m_data;
			for(int iii = 0; iii < length; ++iii)
				intData[iii] = iii + 1;
		}
	}
}

Dataset::Dataset(const int &rank, int const *dimLengths, DataType type, 
		char *data, bool descendingData)
	: m_space(rank, dimLengths),
    m_dataType(type),
	m_complexIndex(-1),
    m_truncatedDim(false)
{
	m_data = data;
	m_descendingData = descendingData;
    m_dataTypeSize = emdTypeDepth(m_dataType);
}

Dataset::~Dataset()
{
	//qDebug() << "Deleting data " << m_name;
	if(m_data)
		delete[] m_data;
}

/***************************** File Operations **************************/
void Dataset::loadData(const DataSet &dataSet)
{
    //H5PLset_loading_state(1);
    
	H5::DataSpace space = dataSet.getSpace();
	// Data element size
	H5T_class_t dataClass = dataSet.getTypeClass();
	H5::DataType type;
	if(H5T_FLOAT == dataClass)
		type = dataSet.getFloatType();
	else if(H5T_INTEGER == dataClass)
		type = dataSet.getIntType();
	else if(H5T_STRING == dataClass)
	{
		type = dataSet.getStrType();
		if(type.isVariableStr())
		{
			// need to fix byte size
		}
		else
		{
			m_dataTypeSize = (int)type.getSize();
		}
	}
	
	unsigned long long size = m_dataTypeSize;
	for(int iii = 0; iii < m_space.rank(); ++iii)
		size *= m_space.dimLength(iii);

	if(size < MEMORY_LIMIT)
	{
		// Init data
		// TODO: catch memory allocation exception
		m_data = new char[size];
		// Copy data
		dataSet.read(m_data, type, space, space);
	}
	else
	{
		qWarning() << "Data size exceeds memory limit";
	}
}

void Dataset::unloadData()
{
    // Disallow unloading of unsaved data
    if(m_data && !(m_status & Node::DIRTY))
    {
	    delete[] m_data;
	    m_data = 0;
    }
}

bool Dataset::isLoaded() const
{
    return (m_data != NULL);
}

void Dataset::save(const QString & path, H5Object *parentObject)
{
	if(!parentObject)
		return;
	
	DataSet *dataset = 0;

	if(m_data) try
	{
		QByteArray ba = m_name.toUtf8();
		const char *name = ba.constData();

		int exists = H5Lexists(parentObject->getId(), name, (hid_t)NULL);
		if(exists < 0)
			return;

		if(exists == 0)
		{
			// Create property list for a dataset and set up fill values.
			uint64_t fillvalue = 0; 
			DSetCreatPropList plist;
			plist.setFillValue(emd::emdToHdfType(m_dataType), &fillvalue);
 
			// Create dataspaces for the dataset in the file and memory.
			hsize_t *fdim = new hsize_t[m_space.rank()];
			for(int iii = 0; iii < m_space.rank(); ++iii)
				fdim[iii] = m_space.dimLength(iii);
			H5::DataSpace fspace(m_space.rank(), fdim);

			H5::DataType dataType(emd::emdToHdfType(m_dataType));
		
			// Create the dataset
			dataset = new DataSet(H5Dcreate(parentObject->getId(),
				name, dataType.getId(), fspace.getId(), 
				H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
			
			// For now, write the dataset iff it didn't already exist
			//	This will have to be changed later if we want to allow
			//	the modification of existing data sets.
			if(dataset)
			{
				dataset->write(m_data, dataType, fspace, fspace);
			}

			delete[] fdim;
		}
		else
		{
			dataset = new DataSet(H5Dopen(parentObject->getId(), name, (hid_t)NULL));
		}

		if(dataset)
		{
			foreach(Node *node, m_children)
				node->save(path + "/" + m_name, dataset);

			dataset->close();
			delete dataset;
		}

		removeStatus(DIRTY);
	}
	catch(FileIException error) {
		qDebug() << "Data save failed (FileIException): " << m_name;
	}
	catch(PropListIException error) {
		qDebug() << "Data save failed (PropListIException): " << m_name;
	}
}

/************************************** Accessors *************************************/
QString Dataset::dataName() const
{
	Attribute *name = static_cast<Attribute*> (this->child("name"));
	if(name)
		return name->variantRepresentation().toString();

	return Node::name();
}

emd::DataType Dataset::dataType() const
{
	return m_dataType;
}

QString Dataset::units() const
{
	Node *unitsAttr = this->child("units");
	if(unitsAttr)
		return unitsAttr->variantRepresentation().toString();

	// If the corresponding dimension dataset does not exist, or it does
	//	not have a units attribute, return an empty string.
	return QString("");
}

int Dataset::dimCount() const
{
	return m_space.rank();
}

int Dataset::dimLength(const int &dimIndex) const
{
    if(m_truncatedDim && dimIndex == 0)
        return m_trueLength;

	return m_space.dimLength(dimIndex);
}

auto Dataset::defaultSlice() const -> Slice
{
    Slice index;
    index.resize(m_space.rank(), 0);
    index[0] = HorizontalDimension;
    index[1] = VerticalDimension;

    return index;
}

void Dataset::setDataSpace(const DataSpace &space)
{
	m_space = space;
}

void Dataset::setDataType(DataType type)
{
    m_dataType = type;
    m_dataTypeSize = emdTypeDepth(type);
}

int Dataset::complexIndex() const
{
	return m_complexIndex;
}

void Dataset::setComplexIndex(const int &index)
{
	m_complexIndex = index;
}

bool Dataset::isComplexDim() const
{
	// Check if this dataset represents a complex dimension of a larger dataset
	if(m_space.rank() != 1)
		return false;

	Node *nameNode = this->child("name");
	if(nameNode)
	{
		QString name = nameNode->variantRepresentation().toString();
		if(name.compare("complex", Qt::CaseInsensitive) == 0)
			return true;
	}

	return false;
}

Dataset::Selection Dataset::selectAll() const
{
	Selection selection;
	for(int iii = 0; iii < m_space.rank(); ++iii)
	{
		Range range = {0, m_space.dimLength(iii)};
		selection.push_back(range);
	}

	return selection;
}

void Dataset::setTrueLength(int length)
{
    if(this->dimCount() != 1)
        return;

    if(this->dimLength(0) != 2)
        return;

    m_trueLength = length;
    m_truncatedDim = true;
}

QVariant Dataset::variantRepresentation() const
{
	return QVariant(m_space.stringRepresentation());
}

QVariant Dataset::variantDataValue(const int &index) const
{
	// If this is a dummy data node (created when a dim node does not
	//	exist) then we just return the index value itself (shifted).
	if(!m_data)
		return QVariant(index + 1);

	switch(m_dataType)
	{
	case DataTypeInt8:
		{
			if(m_truncatedDim)
                return extrapolate<int8_t>(index);

            return value<int8_t>(index);
		}
		break;
	case DataTypeInt16:
		{
			if(m_truncatedDim)
                return extrapolate<int16_t>(index);

            return value<int16_t>(index);
		}
		break;
	case DataTypeInt32:
		{
			if(m_truncatedDim)
                return extrapolate<int32_t>(index);

            return value<int32_t>(index);
		}
		break;
	case DataTypeInt64:
		{
			if(m_truncatedDim)
                return extrapolate<int64_t>(index);

            return value<int64_t>(index);
		}
		break;
	case DataTypeUInt8:
		{
			if(m_truncatedDim)
                return extrapolate<uint8_t>(index);

            return value<uint8_t>(index);
		}
		break;
	case DataTypeUInt16:
		{
			if(m_truncatedDim)
                return extrapolate<uint16_t>(index);

            return value<uint16_t>(index);
		}
		break;
	case DataTypeUInt32:
		{
			if(m_truncatedDim)
                return extrapolate<uint32_t>(index);

            return value<uint32_t>(index);
		}
		break;
	case DataTypeUInt64:
		{
			if(m_truncatedDim)
                return extrapolate<uint64_t>(index);

            return value<uint64_t>(index);
		}
		break;
	case DataTypeFloat32:
		{
			if(m_truncatedDim)
                return extrapolate<float>(index);

            return value<float>(index);
		}
		break;
	case DataTypeFloat64:
		{
			if(m_truncatedDim)
                return extrapolate<double>(index);

            return value<double>(index);
		}
		break;
	case DataTypeString:
		{
            if(m_truncatedDim && index > 1)
                return QVariant("invalid");

            int byteSize = emdTypeDepth(m_dataType);
			int offset = index * byteSize;
			char *val = new char[byteSize + 1];
			memcpy(val, m_data + offset, byteSize);
			val[byteSize] = '\0';
			return QVariant( QString(val) );
		}
		break;
	default:
		break;
	}

	return QVariant(index);
}

QVariant Dataset::variantDataValue(const Slice &slice) const
{
    // TODO: move to separate method
    if(slice.size() != m_space.rank())
        return QVariant();

    for(int iii = 0; iii < m_space.rank(); ++iii)
    {
        if(m_space.dimLength(iii) <= slice[iii])
            return QVariant();
    }

    // TODO: generalize this
	int offset = 0;
	int step = 1;

	int cStart = m_space.rank() - 1;
	int cEnd = -1, cStep = -1;

	if(!m_descendingData)
	{
		cStart = 0;
		cEnd = m_space.rank();
		cStep = 1;
	}

	for(int iii = cStart; iii != cEnd; iii += cStep)
	{
		offset += slice[iii] * step;

		step *= m_space.dimLength(iii);
	}

    //offset *= m_space.byteSize();

	return variantDataValue(offset);
}

QString Dataset::valueString(const int &index) const
{
	if(!m_data)
		return QString();

    switch(m_dataType)
	{
	case DataTypeInt8:
		{
			if(m_truncatedDim)
                return QString::number(extrapolate<int8_t>(index));

            return QString::number(value<int8_t>(index));
		}
		break;
	case DataTypeInt16:
		{
			if(m_truncatedDim)
                return QString::number(extrapolate<int16_t>(index));

            return QString::number(value<int16_t>(index));
		}
		break;
	case DataTypeInt32:
		{
			if(m_truncatedDim)
                return QString::number(extrapolate<int32_t>(index));

            return QString::number(value<int32_t>(index));
		}
		break;
	case DataTypeInt64:
		{
			if(m_truncatedDim)
                return QString::number(extrapolate<int64_t>(index));

            return QString::number(value<int64_t>(index));
		}
		break;
	case DataTypeUInt8:
		{
			if(m_truncatedDim)
                return QString::number(extrapolate<uint8_t>(index));

            return QString::number(value<uint8_t>(index));
		}
		break;
	case DataTypeUInt16:
		{
			if(m_truncatedDim)
                return QString::number(extrapolate<uint16_t>(index));

            return QString::number(value<uint16_t>(index));
		}
		break;
	case DataTypeUInt32:
		{
			if(m_truncatedDim)
                return QString::number(extrapolate<uint32_t>(index));

            return QString::number(value<uint32_t>(index));
		}
		break;
	case DataTypeUInt64:
		{
			if(m_truncatedDim)
                return QString::number(extrapolate<uint64_t>(index));

            return QString::number(value<uint64_t>(index));
		}
		break;
	case DataTypeFloat32:
		{
			if(m_truncatedDim)
                return QString::number(extrapolate<float>(index));

            return QString::number(value<float>(index));
		}
		break;
	case DataTypeFloat64:
		{
			if(m_truncatedDim)
                return QString::number(extrapolate<double>(index));

            return QString::number(value<double>(index));
		}
		break;
	case DataTypeString:
		{
            if(m_truncatedDim && index > 1)
                return QString("invalid");

			int offset = index * m_dataTypeSize;
			char *val = new char[m_dataTypeSize + 1];
			memcpy(val, m_data + offset, m_dataTypeSize);
			val[m_dataTypeSize] = '\0';
			return QString(val);
		}
		break;
	default:
		break;
	}

    return QString();
}

Frame *Dataset::frame(const Slice &slice) const
{
	int hor = -1, ver = -1;
	int hStep, vStep;
	int offset = 0;
	int step = 1;
	int complexStep;

	int cStart = m_space.rank() - 1;
	int cEnd = -1, cStep = -1;
	if(!m_descendingData)
	{
		cStart = 0;
		cEnd = m_space.rank();
		cStep = 1;
	}

	for(int iii = cStart; iii != cEnd; iii += cStep)
	{
		int dimLength = m_space.dimLength(iii);
		if(slice[iii] == HorizontalDimension)		// horizontal axis
		{
			hor = iii;
			hStep = step;
		}
		else if(slice[iii] == VerticalDimension)	// vertical axis
		{
			ver = iii;
			vStep = step;
		}
		else if(iii == m_complexIndex)
		{
			complexStep = step;
		}
		else
		{
			offset += slice[iii] * step;
		}

		step *= dimLength;
	}

	// Check to make sure the axes have been assigned to dimensions
	// TODO: report appropriate error
	if(hor < 0 || ver < 0)
		return 0;

    if(hor > ver)
    {
        int temp = ver;
        ver = hor;
        hor = temp;

        temp = vStep;
        vStep = hStep;
        hStep = temp;
    }

	void *real, *imaginary = NULL;
	real = (void*) (m_data + offset * m_dataTypeSize);
	if(m_complexIndex >= 0)
		imaginary = (void*) (m_data + (offset + complexStep) * m_dataTypeSize); 

	Frame *frame = new Frame(real, imaginary, hStep, vStep, m_space.dimLength(hor), m_space.dimLength(ver), this->dataType(), false);

    frame->setIndex(offset);

	return frame;
}

} // namespace emd


// ---------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------

//template <typename T>
//DatasetIterator<T>::DatasetIterator(const Dataset *data, Dataset::Selection selection)
//	: m_data(data)
//{
//	if(selection.size() == 0)
//		m_selection = m_data->selectAll();
//	else
//		m_selection = selection;
//}
//
//template <typename T>
//DatasetIterator<T>::DatasetIterator(const Dataset *data, Dataset::State state)
//	: m_data(data)
//{
//	// If instantiated with a state, treat as a selection with a size of one
//	// (i.e. a single Frame)
//	for(int iii = 0; iii < m_data->dimCount(); ++iii)
//		m_selection.push_back( {state.indexes.at(iii), 1} );
//}
//
//template <typename T>
//const T &DatasetIterator<T>::initialize()
//{
//	
//}
//
//template <typename T>
//const T &DatasetIterator<T>::operator*()
//{
//	return *m_currentValue;
//}
//
//template <typename T>
//void DatasetIterator<T>::operator++()
//{
//
//}
//
//template <typename T>
//void DatasetIterator<T>::nextFrame()
//{
//
//}
//
//
//// Explicit instantiation for the linker
//template class DatasetIterator<int8_t>;
//template class DatasetIterator<int16_t>;
//template class DatasetIterator<int32_t>;
//template class DatasetIterator<int64_t>;
//template class DatasetIterator<uint8_t>;
//template class DatasetIterator<uint16_t>;
//template class DatasetIterator<uint32_t>;
//template class DatasetIterator<uint64_t>;
//template class DatasetIterator<float>;
//template class DatasetIterator<double>;
