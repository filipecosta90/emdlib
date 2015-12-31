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

#ifndef EMD_FRAME_H
#define EMD_FRAME_H

#include "EmdLib.h"

#include <stdint.h>

#include <QDebug>

#include "Util.h"

namespace emd
{
    // TODO: move this to plugin lib
class EMDLIB_API Frame
{
public:
	enum Attribute {
		AttributeComplex					= 0x010,
		
		AttributeFourierTransformed			= 0x0100,
		AttributeFourierTransformedNoShift	= 0x0200
	};

	enum DataIndex {
		DataIndexUnassigned = -99,
		DataIndexRaw		= -1
	};

	template <typename T>
	struct Data {
		int64_t attributes;
		int64_t hStep;
		int64_t vStep;
		int32_t hSize;
		int32_t vSize;
		T* real;
		T* imaginary;
		Data(int64_t attr, 
			int64_t hstp, int64_t vstp,
			int32_t hsz, int32_t vsz,
			T *r, T *i)
			: attributes(attr),
			hStep(hstp), vStep(vstp),
			hSize(hsz), vSize(vsz),
			real(r), imaginary(i)
		{}
        template <typename Y>
        Data<T>(const Data<Y> &other)
            : attributes(other.attributes),
            hStep(other.hStep), vStep(other.vStep),
            hSize(other.hSize), vSize(other.vSize),
            real((T*)other.real), imaginary((T*)other.imaginary)
        {}
		void setAttribute(Attribute attribute)
		{
			attributes |= attribute;
		}
		void unsetAttribute(Attribute attribute)
		{
			attributes &= ~attribute;
		}
        int64_t size()
        {
            return hSize * vSize;
        }
	};

	Frame(void *real, void *imaginary, 
		int hStep, int vStep, 
		int hSize, int vSize, 
		DataType type = DataTypeUnknown,
        bool ownsData = true);
    Frame(const Data<void> &data, const DataType &type, bool ownsData = true);
    Frame(const Frame *other);
	~Frame();

    template <typename T>
    Data<T> data() const
    {
        return static_cast<Data<T> >(m_data);
    }
    
	DataType dataType() const;

    bool isComplex() const;

    int64_t index() const;
    void setIndex(int64_t index);

    // Gets the data range if available.
    void checkDataRange(float &min, float &max) const;

    // Gets the data range, calculating it if needed.
    template <typename T>
    void getDataRange(T &min, T &max);

    void saveRawData(const QString &filePath) const;

protected:
	Data<void> m_data;
	emd::DataType m_dataType;
    bool m_ownsData;
    int64_t m_index;
    float m_minValue;
    float m_maxValue;
};

typedef std::vector<Frame *> FrameList;

} // namespace emd

#endif