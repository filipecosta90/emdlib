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

#include "Node.h"

#include <H5Cpp.h>

#include <QDebug>

#include "Attribute.h"
#include "Dataset.h"
#include "DataGroup.h"
#include "Group.h"

#ifndef H5_NO_NAMESPACE
	using namespace H5;
#endif

namespace emd
{

Node::Node(Node *parent)
	: QObject()
{
	// TODO: clean up parent/child assignment
	m_parent = parent;

	m_status = 0;
}

Node::~Node()
{
	//qDebug() << "Deleting node " << m_name;
	foreach(Node *node, m_children)
		delete node;
}

/************************** Accessors ****************************/

const QString Node::path() const
{
	if(m_parent)
		return m_parent->path() + "/" + m_name;

	return "";
}

int Node::rowNumber()
{
	if(m_parent)
		return m_parent->m_children.indexOf(const_cast<Node*>(this));

	return 0;
}
Node *Node::child(const QString &name) const
{
	foreach(Node *node, m_children)
	{
		if(name.compare(node->name()) == 0)
			return node;
	}
	return 0;
}

Node *Node::child(const int &index) const
{
	if(index < 0 || index > m_children.size())
		return 0;

	return m_children.at(index);
}

Node *Node::childAtPath(const QString &path)
{
	Node *node = 0;
	int index = path.indexOf('/');
	if(index > 0)
	{
		QString stem = path;
		stem.truncate(index);
		QString stern = path;
		stern.remove(0, index+1);

		Node *c = child(stem);
		if(c)
			node = c->childAtPath(stern);
	}
	else
	{
		node = child(path);
	}
	return node;
}

QVariant Node::variantRepresentation() const
{
	return QVariant("Node base class");
}

/************************* Mutators ******************************/

bool Node::addChildren(const int &position, const int &count, const int &type)
{
	if(position < 0 || position > m_children.size())
		return false;

	switch(type)	// TODO: clean this up
	{
	case GROUP:
		for(int row = 0; row < count; ++row)
		{
			Node *node = new Group(this);
			m_children.insert(position, node);
		}
		break;
	case DATAGROUP:
		for(int row = 0; row < count; ++row)
		{
			Node *node = new DataGroup(this);
			m_children.insert(position, node);
		}
		break;
	case ATTRIBUTE:
		for(int row = 0; row < count; ++row)
		{
			Node *node = new Attribute(this);
			m_children.insert(position, node);
		}
		break;
	case DATASET:
		for(int row = 0; row < count; ++row)
		{
			Node *node = new Dataset(this);
			m_children.insert(position, node);
		}
		break;
	default:
		for(int row = 0; row < count; ++row)
		{
			Node *node = new Node(this);
			m_children.insert(position, node);
		}
	}

	return true;
}

bool Node::addChild(Node *child)
{
	if(m_children.contains(child))
		return false;

	m_children.append(child);
	child->setParentNode((Node*)this);
	return true;
}

bool Node::removeChildren(const int &position, const int &count)
{
	if (position < 0 || position + count > m_children.size())
		return false;

	for (int row = 0; row < count; ++row)
		delete m_children.takeAt(position);

	return true;
}

void Node::setStatus(const int &status, bool cascade)
{
	m_status |= status;
	if(cascade)
	{
		foreach(Node *child, m_children)
			child->setStatus(status, true);
	}
}

void Node::removeStatus(const int &status)
{
	if(m_status & status)
		m_status ^= status;
}

/***************************** File Operations **************************/

void Node::save(const QString &path, H5Object *parentObject)
{
	// The only node that should be calling this version of the function
	//	is the root, in which case we just save all of its children.
	foreach(Node *node, m_children)
		node->save(path, parentObject); 

    removeStatus(Node::DIRTY);
}

} // namespace emd

