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

#ifndef EMD_GROUP_H
#define EMD_GROUP_H

#include "EmdLib.h"

#include "Node.h"

namespace emd
{

class DataGroup;

class EMDLIB_API Group : public Node
{
public:
	Group(Node *parent = 0);
	virtual ~Group();

	// File operations
	virtual void save(const QString &path, H5::H5Object *group);

	// Accessors
	virtual QVariant variantRepresentation() const;
};

} // namespace emd

#endif