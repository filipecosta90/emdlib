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

#include "Group.h"

#include <H5Cpp.h>

#include <QDebug>

#include "Attribute.h"
#include "DataGroup.h"

#ifndef H5_NO_NAMESPACE
	using namespace H5;
#endif

namespace emd
{

Group::Group(Node *p)
	: Node(p)
{

}

Group::~Group()
{
	//qDebug() << "Deleting group " << m_name;
}

/***************************** File Operations **************************/

void Group::save(const QString &path, H5Object *parentObject)
{
	// If the group is not dirty, none of its data has been changed
	//if(m_status & DIRTY)
	{
		removeStatus(DIRTY);
		// Check to see if the group exists, and create it if
		//	it doesn't. Then save all its children.
		QString groupPath = QString(path + "/" + m_name);
		QByteArray ba = m_name.toLocal8Bit();	// gotta store bytes
		const char *charPath = ba.constData();
		H5::Group *group = 0;

		//int status = H5Eset_auto(NULL, NULL, NULL);
		int status;
		H5I_type_t objType = parentObject->getHDFObjType(parentObject->getId());
		switch(objType)
		{
		case H5I_FILE:
		{
			H5File *file = (H5File*)parentObject;
			status = H5Gget_objinfo(file->getId(), charPath, 0, NULL);
			if(status == 0)		// group exists
				group = new H5::Group(file->openGroup(charPath));
			else				// group must be created
				group = new H5::Group(file->createGroup(charPath));
			break;
		}
		case H5I_GROUP:
		{
			H5::Group *parentGroup = (H5::Group*)parentObject;
			status = H5Gget_objinfo(parentObject->getId(), charPath, 0, NULL);
			if(status == 0)		// group exists
				group = new H5::Group(parentGroup->openGroup(charPath));
			else				// group must be created
				group = new H5::Group(parentGroup->createGroup(charPath));
			break;
		}
		case H5I_DATASET:		// none of these should ever happen
		case H5I_ATTR:
		default:
			break;
		}

		if(group)
		{
			foreach(Node *node, m_children)
				node->save(groupPath, group);

			group->close();
			delete group;
		}
	}
}

QVariant Group::variantRepresentation() const
{
	QString result("%1 children");
	return QVariant(result.arg(m_children.size()));
}

} // namespace emd
