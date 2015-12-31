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

#include "DataGroup.h"

#include "H5Cpp.h"

#include <QDebug>

#include "Attribute.h"
#include "Dataset.h"

namespace emd
{

DataGroup::DataGroup(Node *parent)
	: Group(parent),
    m_model(nullptr)
{
	m_data = 0;
	m_status = Node::DATAGROUP;
}

DataGroup::~DataGroup(void)
{
	qDebug() << "Deleting datagroup " << m_name;
}

/***************************** Accessors ***********************************/

Model *DataGroup::model() const
{
    return m_model;
}

void DataGroup::setModel(Model *model)
{
    m_model = model;
}

int DataGroup::dimCount() const
{
	return m_data->dimCount();
}

Dataset *DataGroup::dimData(const int &index) const
{
	if(index >= 0 && index < m_dims.count())
		return m_dims.at(index);

	return 0;
}

bool DataGroup::isIntType() const
{
	emd::DataType type = m_data->dataType();
	if(type == DataTypeUnknown
		|| type == DataTypeFloat32
		|| type == DataTypeFloat64)
		return false;

	return true;
}

bool DataGroup::hasComplexDim() const
{
	bool complex = false;
	if(m_data)
		complex = (m_data->complexIndex() >= 0);

	return complex;
}

int DataGroup::complexIndex() const
{
	if(m_data)
		return m_data->complexIndex();

	return -1;
}

void DataGroup::setData(Dataset *data)
{
	m_data = data;
	if(!m_children.contains(data))
		m_children.append(data);
	//data->setParentNode(this);	// TODO: what's wrong here?
}

void DataGroup::addDim(Dataset *dim)
{
	m_dims.append(dim);
	if(!m_children.contains(dim))
		m_children.append(dim);
	//dim->setParentNode(this);		// and here
}

bool DataGroup::validate()
{
    Dataset *data = dynamic_cast<Dataset*>(child("data"));
	if(!data)
		return false;

	int nDims = data->dimCount();
	Dataset *dim = 0;
	QString stem("dim%1");
	QList<QString> dimNames;
	for(int iii = 0; iii < nDims; ++iii)
	{
		dim = (Dataset*) child(stem.arg(iii+1));
		if(!dim)
		{
			// TODO: create a default dimension
			return false;
		}

		addDim(dim);

		QString name = stem.arg(iii+1);
		Node *nameNode = dim->child("name");
		if(nameNode)
		{
			name = nameNode->variantRepresentation().toString();
			if(name.compare("complex", Qt::CaseInsensitive) == 0)
				data->setComplexIndex(iii);
		}

		dimNames.append(name);
	}
	
	setData(data);

    checkDimLengths();


	Attribute *dataOrderAttr = dynamic_cast<Attribute*>(childAtPath("data_order"));
	if(dataOrderAttr)
	{
		int dataOrder = dataOrderAttr->value().toInt();
		// 0 = ascending, 1 = descending (default)
		if(dataOrder == 0)
			setDataOrder(false);
	}

    return true;
}

void DataGroup::checkDimLengths()
{
    for(int index = 0; index < this->dimCount(); ++index)
    {
        if(m_data->dimLength(index) != this->dimData(index)->dimLength(0))
        {
            this->dimData(index)->setTrueLength(m_data->dimLength(index));
        }
    }
}

Frame *DataGroup::getFrame()
{
	//return m_data->getFrame();
	return 0;
}

bool DataGroup::load(H5::H5File *file)
{
	if(!m_data)
		return false;
	// Load data
	QString path = m_data->path();
	QByteArray ba = path.toLocal8Bit();
	const char *cPath = ba.data();
	hid_t objID = H5Oopen(file->getId(), cPath, H5P_DEFAULT);
	H5::DataSet dataSet(objID);
	m_data->loadData(dataSet);

	// Load dims
	for(Dataset *dim : m_dims)
	{
		path = dim->path();
		ba = path.toLocal8Bit();
		cPath = ba.data();
		hid_t objID = H5Oopen(file->getId(), cPath, H5P_DEFAULT);
		dataSet = H5::DataSet(objID);
		dim->loadData(dataSet);
	}
	return true;
}

void DataGroup::unload()
{
	m_data->unloadData();
}

bool DataGroup::isLoaded() const
{
    if(!m_data)
        return false;

    return m_data->isLoaded();
}

} // namespace emd
