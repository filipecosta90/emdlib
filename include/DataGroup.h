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

#ifndef EMD_DATAGROUP_H
#define EMD_DATAGROUP_H

#include "EmdLib.h"

#include "Group.h"
#include "Dataset.h"
#include <QList>

class Frame;
class Node;

namespace H5
{
class H5File;
}

namespace emd
{

class Model;

class EMDLIB_API DataGroup : public Group
{
public:
	DataGroup(Node *parent = 0);
    DataGroup(const DataGroup &other);
	virtual ~DataGroup(void);

	// Accessors
    Model *model() const;
    void setModel(Model *model);

	int dimCount() const;
	Dataset *dimData(const int &index) const;
	int dimLength(const int &index) const {return m_data->dimLength(index);}
	emd::DataType type() const {return m_data->dataType();}
	bool isIntType() const;
	bool hasComplexDim() const;
	int complexIndex() const;

	const Dataset *data() const {return m_data;}
	void setDataOrder(bool descending) {m_data->setDataOrder(descending);}
	bool dataOrder() const {return m_data->dataOrder();}

	void setData(Dataset *data);
	void addDim(Dataset *dim);

    void checkDimLengths();

    bool validate();

	Frame *getFrame();
	bool load(H5::H5File *file);
	void unload();
    bool isLoaded() const;

private:
	Dataset *m_data;
	QList<Dataset*> m_dims;
    Model *m_model;
};

} // namespace emd

#endif
