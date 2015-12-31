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

#include "Frame.h"

#include <limits>
#include <cstdlib>
#include <cstdint>

#include <qdatastream.h>
#include <QDebug>
#include <qfile.h>

#include "Util.h"

namespace emd
{
    
static const int SAMPLE_CUTOFF = 10;
static const int RANDOM_SAMPLE_SIZE = 2000;

Frame::Frame(void *real, void *imaginary, int hStep, int vStep, int hSize, int vSize, emd::DataType type, bool ownsData)
	: m_data(0, hStep, vStep, hSize, vSize, real, imaginary),
	m_dataType(type),
    m_ownsData(ownsData),
    m_minValue(kInvalidFloatValue),
    m_maxValue(kInvalidFloatValue)
{
	if(imaginary)
		m_data.setAttribute(Frame::AttributeComplex);
}

Frame::Frame(const Data<void> &data, const DataType &type, bool ownsData)
    : m_data(data),
    m_dataType(type),
    m_ownsData(ownsData),
    m_minValue(kInvalidFloatValue),
    m_maxValue(kInvalidFloatValue)
{
}

Frame::Frame(const Frame *other)
    : m_data(other->data<void>()),
    m_dataType(other->dataType()),
    m_index(other->index()),
    m_ownsData(false)
{
    other->checkDataRange(m_minValue, m_maxValue);
}

Frame::~Frame()
{
	if(m_ownsData)
    {
        if(m_data.real)
            delete[] m_data.real;
        if(m_data.imaginary)
            delete[] m_data.imaginary;
    }
}

DataType Frame::dataType() const
{
    return m_dataType;
}

bool Frame::isComplex() const
{
    return (m_data.imaginary != NULL);
}

int64_t Frame::index() const
{
    return m_index;
}

void Frame::setIndex(int64_t index)
{
    m_index = index;
}

void Frame::checkDataRange(float &min, float &max) const
{
    min = m_minValue;
    max = m_maxValue;
}

template <typename T>
void Frame::getDataRange(T &min, T &max)
{
    if(m_minValue == kInvalidFloatValue || m_maxValue == kInvalidFloatValue)
    {
	    Frame::Data<T> data = this->data<T>();

        const std::vector<int> &randomIndexes = emd::randomIndexes(RANDOM_SAMPLE_SIZE, data.hSize * data.vSize);

	    if( data.attributes & (Frame::AttributeFourierTransformed | Frame::AttributeFourierTransformedNoShift) )
	    {
		    // For fourier transformed images, we scale according to the maximum pixel,
		    // which is usually the upper left. To find it, we check the size of the frame.
		    // We don't need to worry about strides because if we have a fourier transformed
		    // image, we must have at least one processed data vector.
		    int hOffset = 0, vOffset = 0;
		    if(data.attributes & Frame::AttributeFourierTransformed)
		    {
			    hOffset = data.hSize / 2;
			    vOffset = data.vSize / 2;
		    }
		    static const float ffac = 0.005f;
		    m_maxValue = data.real[vOffset * data.hSize + hOffset] * ffac;
		    m_minValue = 0;
	    }
	    else
	    {
		    // Determine the range of the Frame
		    std::vector<T> mins( SAMPLE_CUTOFF, std::numeric_limits<T>::max() );
		    std::vector<T> maxes( SAMPLE_CUTOFF, std::numeric_limits<T>::lowest() );
		    T val, newVal, oldVal;
		    int div, rem, index;
		    for(int iii = 0; iii < RANDOM_SAMPLE_SIZE; ++iii)
		    {
			    index = randomIndexes[iii];
			    div = index / data.hSize;
			    rem = index % data.hSize;
			    index = div * data.vStep + rem * data.hStep;
			    val = data.real[index];

			    index = -1;
			    while(index+1 < SAMPLE_CUTOFF && mins.at(index+1) > val)
				    ++index;
			    newVal = val;
			    while(index >= 0)
			    {
				    oldVal = mins[index];
				    mins[index] = newVal;
				    newVal = oldVal;
				    --index;
			    }

			    while(index+1 < SAMPLE_CUTOFF && maxes.at(index+1) < val)
				    ++index;
			    newVal = val;
			    while(index >= 0)
			    {
				    oldVal = maxes[index];
				    maxes[index] = newVal;
				    newVal = oldVal;
				    --index;
			    }
		    }

		    m_minValue = (float) mins.at(0);
		    m_maxValue = (float) maxes.at(0);
	    }
    }

    min = (T) m_minValue;
    max = (T) m_maxValue;
}

void Frame::saveRawData(const QString &filePath) const
{
    int depth = emdTypeDepth(m_dataType);
    int dataLength = depth * m_data.hSize * m_data.vSize;

    int combinedStep = m_data.hStep * m_data.vStep;
    if(combinedStep != m_data.hSize && combinedStep != m_data.vSize)
    {
        // If the step is too big, the data is not contiguous
        // and can't be saved directly.
        // TODO: implement if needed.
        return;
    }
        
    char *outputData = (char *) m_data.real;

    QFile file(filePath);
    file.open(QIODevice::WriteOnly);

    if(!file.isOpen())
        return;

    QDataStream stream(&file);

    stream.writeRawData(outputData, dataLength);
}

template EMDLIB_API void Frame::getDataRange<uint8_t>(uint8_t &, uint8_t &);
template EMDLIB_API void Frame::getDataRange<uint16_t>(uint16_t &, uint16_t &);
template EMDLIB_API void Frame::getDataRange<uint32_t>(uint32_t &, uint32_t &);
template EMDLIB_API void Frame::getDataRange<uint64_t>(uint64_t &, uint64_t &);
template EMDLIB_API void Frame::getDataRange<int8_t>(int8_t &, int8_t &);
template EMDLIB_API void Frame::getDataRange<int16_t>(int16_t &, int16_t &);
template EMDLIB_API void Frame::getDataRange<int32_t>(int32_t &, int32_t &);
template EMDLIB_API void Frame::getDataRange<int64_t>(int64_t &, int64_t &);
template EMDLIB_API void Frame::getDataRange<float>(float &, float &);
template EMDLIB_API void Frame::getDataRange<double>(double &, double &);

} // namespace emd
