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

#ifndef EMD_FILEMANAGER_H
#define EMD_FILEMANAGER_H

#include "EmdLib.h"

#include "Util.h"

namespace emd
{

class Model;

class EMDLIB_API FileManager 
{
public:
    enum Error {
        ErrorNone = 0,

        ErrorUnrecognizedFileType,
        ErrorFileOpenFailed,
        ErrorInvalidOperation,
        ErrorFileIncomplete,

        ErrorInvalidDataFormat,
        ErrorInvalidDataType,

        ErrorUnknown
    };

    static Error openFile(const char *filePath, Model *model);

    // Loading
    static Error loadSer(const char *filePath, Model *model);
	static Error loadDm3(const char *filePath, Model *model);
	static Error loadTiff(const char *filePath, Model *model);

    // Utility
    static emd::DataType serToEmdType(const int &serType);
	static emd::DataType dm3ToEmdType(const int &type);
	static int dm3TypeDepth(const int &type);

};

} // namespace emd

#endif