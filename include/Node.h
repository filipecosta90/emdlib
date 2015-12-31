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

#ifndef EMD_NODE_H
#define EMD_NODE_H

#include "EmdLib.h"

#include <QObject>
#include <QList>
#include <QString>
#include <QVariant>

namespace H5
{
class H5Object;
}

namespace emd
{

class EMDLIB_API Node : public QObject
{
	Q_OBJECT

public:
	Node(Node *parent = 0);
	virtual ~Node();

	// Accessors
	Node *parent() const {return m_parent;}
	void setParentNode(Node *parent) {m_parent = parent;}
	int childCount() const {return m_children.size();}
	Node *child(const int &index) const;
	Node *child(const QString &name) const;
	Node *childAtPath(const QString &path);
	QList<Node*> &children() {return m_children;}
	void setChildren(const QList<Node*> &children) {m_children = children;}

	int rowNumber();
	
	virtual QString name() const {return m_name;}
	const QString path() const;
	int status() const {return m_status;}
	virtual QVariant variantRepresentation() const;

	// Mutators
	bool addChildren(const int &position, const int &count, const int &type);
	bool addChild(Node *child);
	bool removeChildren(const int &position, const int &count);

	void setName(const QString &name) {m_name = name;}
	void setStatus(const int &status, bool cascade = false);
	void removeStatus(const int &status);

	// File operations
	virtual void save(const QString &path, H5::H5Object *group); 

	// Status codes
	enum Status
	{
		DIRTY		= 0x00000010,

		GROUP		= 0x00001000,
		DATASET		= 0x00002000,
		ATTRIBUTE	= 0x00004000,
		DATATYPE	= 0x00008000,

		DATAGROUP	= 0x00011000
	};

protected:
	QString m_name;
	int m_status;

	Node *m_parent;
	QList<Node*> m_children;
};

} // namespace emd

#endif