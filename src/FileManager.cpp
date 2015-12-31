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

#include "FileManager.h"

#include <fstream>
#include <stack>
#include <stdint.h>

#include "Attribute.h"
#include "Model.h"

namespace emd
{

// Helper functions
QVariant readEmdType(QDataStream &stream, emd::DataType emdType);
QVariant readEmdArray(QDataStream &stream, emd::DataType emdType, const int &size);
template <typename T> QVariant readTypedValue(QDataStream &stream)
{
	T value;
	stream >> value;
	return QVariant(value);
}

/******************************** File Opening **********************************/

FileManager::Error FileManager::openFile(const char *filePath, Model *model)
{
    std::string stringPath(filePath);

    // Get the file type
    std::string::size_type dotIndex = stringPath.find_last_of('.');
    if(dotIndex == std::string::npos)
        return ErrorUnrecognizedFileType;

    std::string extension = stringPath.substr(dotIndex + 1);

    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    if(extension.compare("ser") == 0)
        return FileManager::loadSer(filePath, model);
    else if(extension.compare("dm3") ==0)
        return FileManager::loadDm3(filePath, model);
    else if(extension.compare("tif") == 0 || extension.compare("tiff") == 0)
        return FileManager::loadTiff(filePath, model);

    return ErrorUnrecognizedFileType;
}

// With thanks to Peter Ercius
FileManager::Error FileManager::loadSer(const char *filePath, Model *model)
{
    std::ifstream fin(filePath, std::ios_base::in | std::ios_base::binary);

    if(!fin.is_open())
        return ErrorFileOpenFailed;

    if(!fin.good())
    {
        if(fin.bad())
            return ErrorFileOpenFailed;
        else if(fin.fail())
            return ErrorInvalidOperation;
        else if(fin.eof())
            return ErrorFileIncomplete;
    }

    struct SerHeader {
        int16_t byteOrder;          // should be 0x4949
        int16_t seriesID;           // value 0x0197 indicates ES Vision Series Data File
        int16_t seriesVersion;      

        int32_t dataTypeID;         // 0x4120 = 1D arrays; 0x4122 = 2D arrays
        int32_t tagTypeID;          // 0x4152 = Tag is time only; 0x4142 = Tag is 2D with time

        int32_t totalElementCount;  // the number of data elements in the original data set
        int32_t validElementCount;  // the number of data elements written to the file
        int32_t offsetArrayOffset;  // the offset (in bytes) to the beginning of the data offset array

        int32_t dimensionCount;     // the number of dimensions of the indices (not the data)
    } serHeader; 
	
    // The first three entries must be read one-by-one because of
    // half-word size.
    fin.read((char*)&serHeader.byteOrder, 2);
    fin.read((char*)&serHeader.seriesID, 2);
    fin.read((char*)&serHeader.seriesVersion, 2);
    fin.read((char*)&serHeader.dataTypeID, 6 * sizeof(int32_t));

    
	// Dimension arrays (starts at byte offset 30)
    struct DimensionArray {
        int32_t dimensionSize;      // the number of elements in this dimension
        double calibrationOffset;   // calibration value at element calibrationElement
        double calibrationDelta;    // calibration delta between elements of the series
        int32_t calibrationElement; // the element in the series with a calibraion value of calibrationOffset
        int32_t descriptionLength;  // length of the description string
        char *description;          // describes the dimension
        int32_t unitsLength;        // length of the units string
        char *units;                // name of the units in this dimension

        DimensionArray()
            : description(NULL),
            units(NULL)
            {}

        ~DimensionArray()
        {
            if(description)
                delete[] description;
            if(units)
                delete[] units;
        }
    };

    DimensionArray *dimArr = new DimensionArray[serHeader.dimensionCount];

    for(int index = 0; index < serHeader.dimensionCount; ++index)
    {
        fin.read((char*)&dimArr[index].dimensionSize, 4); 

        fin.read((char*)&dimArr[index].calibrationOffset, 8); 
        fin.read((char*)&dimArr[index].calibrationDelta, 8); 
        fin.read((char*)&dimArr[index].calibrationElement, 4); 

        fin.read((char*)&dimArr[index].descriptionLength, 4); 
        dimArr[index].description = new char[dimArr[index].descriptionLength + 1];
        fin.read(dimArr[index].description, dimArr[index].descriptionLength); 
        dimArr[index].description[dimArr[index].descriptionLength] = '\0';

        fin.read((char*)&dimArr[index].unitsLength, 4); 
        dimArr[index].units = new char[dimArr[index].unitsLength + 1];
        fin.read(dimArr[index].units, dimArr[index].unitsLength); 
        dimArr[index].units[dimArr[index].unitsLength] = '\0';
    }
    
	// Get arrays containing byte offsets for data and tags
    fin.seekg(serHeader.offsetArrayOffset); // seek in the file to the offset arrays
	// Data offset array (the byte offsets of the individual data elements)
    int32_t *dataOffsetArray = new int32_t[serHeader.totalElementCount];
	fin.read((char*)dataOffsetArray, sizeof(int32_t) *serHeader.totalElementCount);
	// Tag Offset Array (byte offsets of the individual data tags)
    int32_t *tagOffsetArray = new int32_t[serHeader.totalElementCount];
	fin.read((char*)tagOffsetArray, sizeof(int32_t) * serHeader.totalElementCount);
	
	// 1D data
    if(serHeader.dataTypeID == 0x4120)
    {
        int32_t *arrayLengths = new int32_t[serHeader.validElementCount];

        double calibrationOffset;       // calibration value at element calibrationElement
        double calibrationDelta;        // calibration delta between elements of the array
        int32_t calibrationElement;     // element in the array with calibration value of calibrationOffset

        int16_t serDataType;            // data format
        
        for(int index = 0; index < serHeader.validElementCount; ++index)
        {
            fin.seekg(dataOffsetArray[index]);

			fin.read((char*)&calibrationOffset, 8); 
			fin.read((char*)&calibrationDelta, 8); 
			fin.read((char*)&calibrationElement, 4); 
			fin.read((char*)&serDataType, 2);
            
			emd::DataType emdDataType = serToEmdType(serDataType);
            if(emdDataType == DataTypeUnknown)
            {
                fin.close();

                delete[] dimArr;
                delete[] dataOffsetArray;
                delete[] tagOffsetArray;

                return ErrorInvalidDataFormat;
            }

			fin.read((char*)&arrayLengths[index], 4); 

            //char *data = new char[arrayLengths[index] * emd::emdTypeDepth(emdDataType)];
	//		Ser.data{ii} = fread(FID,arrayLength(ii),Type);

	//		energy = linspace(calibrationOffset,calibrationOffset+(arrayLength(ii)-1)*calibrationDelta,arrayLength(ii));
	//		Ser.calibration{ii} = energy;
        }
 //       
 //       %Save and pre-calculate some useful image things
 //       Ser.dispersion = calibrationDelta;
 //       Ser.energyOffset = calibrationOffset;
 //       Ser.filename = fName;
 //       
 //       %If possible, change cell array to matrix
 //       if cellFlag
 //           if length(dimensionSize) == 1
 //               Ser.spectra = reshape(cell2mat(Ser.data)',validNumberElements,arrayLength0);
 //               Ser.calibration = energy;
 //           elseif length(dimensionSize) == 2
 //               Ser.SI = reshape(cell2mat(Ser.data)',dimensionSize(1),dimensionSize(2),arrayLength0);
 //               Ser.calibration = energy;
 //           end
 //           Ser = rmfield(Ser,'data'); %remove the data cell from the struct
 //       end
    }
    // 2D data
    else if(serHeader.dataTypeID == 0x4122)
    {
        char *data = NULL;
        int32_t dataSizeX, dataSizeY;
        int32_t elementSize;
        emd::DataType emdDataType;

        for(int index = 0; index < serHeader.validElementCount; ++index)
        {
			fin.seekg(dataOffsetArray[index]);

            double calibrationOffsetX;      // calibration at element calibrationElement along x
            double calibrationDeltaX;
            int32_t calibrationElementX;    // element in the array along x with calibration value of calibrationOffset

			fin.read((char*)&calibrationOffsetX, 8); 
			fin.read((char*)&calibrationDeltaX, 8);
			fin.read((char*)&calibrationElementX, 4);
            
            double calibrationOffsetY;      // calibration at element calibrationElement along y
            double calibrationDeltaY;
            int32_t calibrationElementY;    // element in the array along y with calibration value of calibrationOffset

			fin.read((char*)&calibrationOffsetY, 8); 
			fin.read((char*)&calibrationDeltaY, 8);
			fin.read((char*)&calibrationElementY, 4);

            int16_t serDataType;
			fin.read((char*)&serDataType, 2);

            int32_t xSize, ySize;

			fin.read((char*)&xSize, 4);
            fin.read((char*)&ySize, 4);
            
            if(!data)
            {
                dataSizeX = xSize;
                dataSizeY = ySize;
			    emdDataType = serToEmdType(serDataType);
                elementSize = dataSizeX * dataSizeY * emd::emdTypeDepth(emdDataType);
                int32_t dataSize = serHeader.validElementCount * elementSize;
                data = new char[dataSize];
            }

            if(dataSizeX == xSize && dataSizeY == ySize)
            {
			    fin.read(data + index * elementSize, elementSize);
            }
            else
            {
                fin.close();

                delete[] dimArr;
                delete[] dataOffsetArray;
                delete[] tagOffsetArray;

                if(data)
                    delete[] data;

                return ErrorInvalidDataFormat;
            }

			//Ser.calibration(:,ii) = [calibrationDeltaX calibrationDeltaY]';
        }

        DataGroup *dataGroup = static_cast<DataGroup*>
			(model->addPath("/data/ser_file", Node::DATAGROUP));

        if(dataGroup)
        {
            int *dimSizes;
            int dimCount;
            if(serHeader.validElementCount == 1)
            {
                dimCount = 2;
                dimSizes = new int[dimCount];
            }
            else
            {
                dimCount = 3;
                dimSizes = new int[dimCount];
                dimSizes[2] = serHeader.validElementCount;
            }
            
            dimSizes[0] = dataSizeX;
            dimSizes[1] = dataSizeY;

            Dataset *dataNode = new Dataset(dimCount, dimSizes, emdDataType, data, false);

			dataNode->setName("data");

			dataGroup->setData(dataNode);
            dataNode->setParentNode(dataGroup);

            Attribute *groupType = new Attribute(dataGroup);
            groupType->setName("emd_group_type");
            groupType->setValue(QVariant(1));
			groupType->setType(DataTypeInt32);
			dataGroup->addChild(groupType);

			Attribute *dataOrder = new Attribute(dataGroup);
            dataOrder->setName("data_order");
            dataOrder->setValue(QVariant(0));
			dataOrder->setType(DataTypeInt32);
			dataGroup->addChild(dataOrder);
			
            dataGroup->setStatus(Node::DIRTY, true);

            Dataset *dimNode;
			QString nameStem = "dim%1";
			for(int iii = 0; iii < 2; ++iii)
			{
				dimNode = new Dataset(dimSizes[iii], DataTypeInt32, true);
				dimNode->setName(nameStem.arg(iii+1));
				dataGroup->addDim(dimNode);
				dimNode->setParentNode(dataGroup);
					
				Attribute *destNode = new Attribute(dimNode);
				destNode->setName("units");
				destNode->setValue(QVariant("[px]"));
				destNode->setType(DataTypeString);
				dimNode->addChild(destNode);

				destNode = new Attribute(dimNode);
				destNode->setName("name");
				destNode->setValue(QVariant(nameStem.arg(iii+1)));
				destNode->setType(DataTypeString);
				dimNode->addChild(destNode);
			}

            if(dimCount == 3)
            {
                dimNode = new Dataset(serHeader.validElementCount, DataTypeInt32, true);
				dimNode->setName(nameStem.arg(3));
				dataGroup->addDim(dimNode);
				dimNode->setParentNode(dataGroup);
					
				Attribute *destNode = new Attribute(dimNode);
				destNode->setName("units");
				destNode->setValue(QVariant("[element]"));
				destNode->setType(DataTypeString);
				dimNode->addChild(destNode);

				destNode = new Attribute(dimNode);
				destNode->setName("name");
				destNode->setValue(QVariant(nameStem.arg(3)));
				destNode->setType(DataTypeString);
				dimNode->addChild(destNode);
            }

            delete[] dimSizes;
        }
    }

	//%%
	//%Read in the tags
	//%Get data from "Time-only" tags
	//if tagTypeID == hex2dec('4152')
	//	for jj=1:validNumberElements
	//		fseek(FID,tagOffsetArray(jj),-1);
	//		tagTypeID2 = fread(FID,1,'int16'); %type of the tag (should be 0x4152 or 16722)
	//		Ser.timeTag(jj) = fread(FID,1,'int32'); %number of seconds since Jan. 1, 1970
	//end
	//%Get data from Time-and-Position tags
	//elseif tagTypeID == hex2dec('4142')
 //       newTags = zeros(3,validNumberElements);
	//	for jj=1:validNumberElements
	//		fseek(FID,tagOffsetArray(jj),-1);
	//		tagTypeID2 = fread(FID,1,'int16');
	//		Ser.scanTags(1,jj) = fread(FID,1,'uint32'); %time Tag
 //           Ser.scanTags(2,jj) = fread(FID,1,'float64'); %xTag
 //           Ser.scanTags(3,jj) = fread(FID,1,'float64'); %yTag
 //           %timeTag = fread(FID,1,'int32');
	//		%xTag = fread(FID,1,'float');
	//		%yTag = fread(FID,1,'float');
 //           %disp(tagTypeID2)
 //           %disp([num2str(xTag) ' ' num2str(yTag)]);
 //       end
 //       if length(dimensionSize) == 2
 //           Ser.scanTags = reshape(Ser.scanTags',dimensionSize(1),dimensionSize(2),3);
 //       end
	//else disp(['Unknown time tag type ' num2str(tagTypeID) ' in ' fName])	
	//end

    fin.close();

    delete[] dimArr;
    delete[] dataOffsetArray;
    delete[] tagOffsetArray;

    return ErrorNone;
}

FileManager::Error FileManager::loadDm3(const char *filePath, Model *emdModel)
{
    QString fileName(filePath);

	qDebug() << "Attempting .dm3 load: " << fileName;

	QFile file(fileName);
	if(!file.open(QIODevice::ReadOnly))
	{
		qWarning() << "Failed to open dm3 file " << fileName;
		return ErrorFileOpenFailed;
	}

	QDataStream in(&file);

	// Check that the file is valid dm3 by reading in the first 3 header bytes.
	qint32 h1, h2, h3;
	in >> h1 >> h2 >> h3;
	// First byte should be file version number (3)
	if(3 != h1)
	{
		qWarning() << "Not a valid dm3 file";
		file.close();
		return ErrorInvalidDataFormat;
	}
	// File data should be big-endian
	if(1 != h3)
	{
		qWarning() << "Not a valid dm3 file";
		file.close();
		return ErrorInvalidDataFormat;
	}

	// The next two bytes are flags for "sorted" and "open"
	qint8 fSorted, fOpen;
	in >> fSorted >> fOpen;

	// The next 4 bytes give the number of tags in the root directory
	qint32 tagCount;
	in >> tagCount;

	// Create a child node of the model root to hold the dm3 tags
	Node *dm3Root = emdModel->addNode("dm3", Node::GROUP);
	// TODO: shouldn't have to specify dirty here maybe?
	//dm3Root->setStatus(Node::DIRTY);
	std::stack<Node*> nodeStack;
	nodeStack.push(dm3Root);
	std::stack<int> tagsRemainingStack;

	bool endOfFile = false;
	bool readError = false;
	qint8 sectionType;
	quint16 tagNameLength;
	QString nameString;
	int unnamedTagCount = 1;
	std::stack<int> utcStack;
	QVariant attrValue;
	emd::DataType emdType;
	QList<emd::DataType> dataTypeList;
	QList<char*> dataList;
	QList<QVariant> attrList;
	char *tagName = NULL;
	int tagsRemaining = tagCount;
	// Iterate over all of the tags/tag directories
	while( !endOfFile && !readError )
	{
		// First byte is section type (tag, directory, eof)
		in >> sectionType;
		// Second byte is tag name length (can be zero)
		in >> tagNameLength;
		// Following bytes are tag name
		// If we have a tag name length, just read in the name
		if(tagNameLength > 0)
		{
			if(tagName)
				delete[] tagName;
			tagName = new char[tagNameLength + 1];
			tagName[tagNameLength] = 0;
			in.readRawData(tagName, tagNameLength);
			nameString = QString(tagName);
		}
		else
		{
			nameString = "" + QString::number(unnamedTagCount++);
		}

		if(20 == sectionType)	// tag directory
		{
			//qDebug() << "Reading directory: " << tagName;
			utcStack.push(unnamedTagCount);
			unnamedTagCount = 1;

			// The next two bytes are sorted/open flags
			in >> fSorted >> fOpen;

			// The next uint32 is the number of tags in the directory
			in >> tagCount;
			//qDebug() << "Directory has " << tagCount << " tags";
			tagsRemainingStack.push(tagsRemaining);
			tagsRemaining = tagCount;

			// Create a node for the new directory
			Node *group = emdModel->addNode(nameString, Node::GROUP, nodeStack.top());
			nodeStack.push(group);
		}
		else if(21 == sectionType)	// tag
		{
			//qDebug() << "Reading tag: " << tagName;
			--tagsRemaining;

			// Add a node for the tag
			Attribute *attribute = static_cast<Attribute*>
				(emdModel->addNode(nameString, Node::ATTRIBUTE, nodeStack.top()));

			// We first expect a delimiter string '%%%%'
			char delimiter[5];
			delimiter[4] = 0;
			in.readRawData(delimiter, 4);
			if(strcmp(delimiter, "%%%%") != 0)
			{
				qWarning() << "Invalid file: missing delimiter";
				readError = true;
				continue;
			}

			// The next 4-byte integer holds the size of the info array
			qint32 infoLength;
			in >> infoLength;
			// The following 4*(infoLength) bytes hold the info array itself
			qint32 *info = new qint32[infoLength];
			for(int iii = 0; iii < infoLength; ++iii)
				in >> info[iii];

			// If the info array has size 1, it's just a single data value
			if(infoLength == 1)
			{
				// The info array contains the data type
				emdType = dm3ToEmdType(info[0]);
				if(emdType != DataTypeUnknown)
				{
					attrValue = readEmdType(in, emdType);
					attribute->setValue(attrValue);
					attribute->setType(emdType);
				}
				else
				{
					qWarning() << "Non-simple single value";
					delete[] info;
					return ErrorInvalidDataType;
				}
			}
			else
			{
				// The first entry of the info array describes the tag data type
				int depth, index;
				switch(info[0])
				{
				case 0x0f:		// struct
					nameString = "";
					attrList.clear();
					// info[2] contains the number of members
					for(int iii = 1; iii <= info[2]; ++iii)
					{
						// The following alternating entries contain the member types
						//	(other values are empty)
						emdType = dm3ToEmdType(info[2 + 2*iii]);
						if(emdType != DataTypeUnknown)
						{
							if(iii > 1)
								nameString += " ";
							attrValue = readEmdType(in, emdType);
							attrList.append(attrValue);
							nameString += attrValue.toString();
						}
						else
						{
							qWarning() << "Non-simple group value";
							delete[] info;
							return ErrorInvalidDataType;
						}	
					}
					// Right now we're just storing the string representation of the array.
					//	We can use the emd::STRUCT type later and implement it properly if
					//	we need to actually access struct values beyond just viewing them.
					attribute->setValue(QVariant(nameString));
					attribute->setType(DataTypeString);
					break;
				case 0x014:		// array
					// info[1] contains the array data type
					emdType = dm3ToEmdType(info[1]);
					// Simple data type
					if(emdType != DataTypeUnknown)
					{
						depth = emd::emdTypeDepth(emdType);
						uint byteSize = depth * info[2];
						if(strcmp(tagName, "Data") == 0)
						{
							char *rawData = new char[byteSize];
							in.readRawData(rawData, byteSize);
							dataList.append(rawData);
							dataTypeList.append(emdType);

                            attribute->setValue(QVariant("data array"));
                            attribute->setType(DataTypeString);
						}
						else
						{
							if( (emdType == DataTypeUInt16) 
								&& (   strcmp(tagName, "Name") == 0
									|| strcmp(tagName, "Units") == 0 ) )
							{
								// Interpret as a unicode string
								char *rawData = new char[byteSize];
								in.readRawData(rawData, byteSize);
								QString nameVal((QChar*)rawData, info[2]);
								attrValue = QVariant(nameVal);
							}
							else if(info[2] > 4)
							{
								in.skipRawData(info[2] * emd::emdTypeDepth(emdType));
								attrValue = QVariant(QString::number(info[2]) + " element array");
							}
							else
							{
								// info[2] contains the array size
								attrValue = readEmdArray(in, emdType, info[2]);
							}
							attribute->setValue(attrValue);
							attribute->setType(DataTypeString);
						}
					}
					// Complex type
					else
					{
						switch(info[1])
						{
						case 0x0f:	// array of structs
							
							// The final value contains the array size
							//if(strcmp(tagName, "Data") == 0)
							//{
							//	int channelCount = info[3];

							//}
							//else
							//{
								depth = 0;
								// info[3] contains the number of members
								for(index = 1; index <= info[3]; ++index)
								{
									depth += dm3TypeDepth(info[3 + 2*index]);
								}
								in.skipRawData(depth * info[2 + 2*index]);
							//}
							break;
						}
					}
				default:		// unrecognized
					break;
				}
			}

			delete[] info;
		}
		else if(0 == sectionType)	// eof
		{
			qDebug() << "Reached end of file";
			endOfFile = true;
		}
		else
		{
			qWarning() << "Tag type read error: " << sectionType;
			readError = true;
		}
		// Decrement tags remaining. If we're at zero, pop the stacks
		while(tagsRemaining == 0 && tagsRemainingStack.size() > 0)
		{
			// Pop the stacks until we find a directory with some tags remaining.
			//	If we are at the end of the last group, don't pop.
			nodeStack.pop();
			tagsRemaining = tagsRemainingStack.top();
			tagsRemainingStack.pop();
			--tagsRemaining;

			unnamedTagCount = utcStack.top();
			utcStack.pop();
		}
	}
	
	if(tagName)
		delete tagName;

	file.close();

	if(readError)
		return ErrorInvalidDataFormat;

	if(!endOfFile)
	{
		qWarning() << "Ran out of tags to read before end of file";
	}
	if(tagsRemaining > 0)
	{
		qWarning() << "Reached end of file with tags remaining";
	}

	// If we found the data, process it
	if(dataList.size() > 0)
	{
		// Try to find the index of the actual image
		Node *refNode = emdModel->getPath("/dm3/ImageSourceList/1/ImageRef");
		if(refNode)
		{
			int imgIndex = refNode->variantRepresentation().toInt();
			Node *imageDataNode = emdModel->getPath("/dm3/ImageList/" 
												+ QString::number(imgIndex+1)
												+ "/ImageData");
			Node *dimNode = imageDataNode->child("Dimensions");
			if(dimNode)
			{
				// Create the data group
				DataGroup *dataGroup = static_cast<DataGroup*>
					(emdModel->addPath("/data/dm3_file", Node::DATAGROUP));

				// Get the list of dimension sizes
				QList<Node*> dimList = dimNode->children();
				int *dimSizes = new int[dimList.size()];
				int dataLength = 1;
				QString dataString = "";
				for(int iii = 0; iii < dimList.count(); ++iii)
				{
					dimSizes[iii] = dimList.at(iii)->variantRepresentation().toInt();
					dataLength *= dimSizes[iii];

					if(iii > 0)
						dataString += " x ";
					dataString += QString::number(dimSizes[iii]);
				}
				int dataDepth = emd::emdTypeDepth(dataTypeList.at(imgIndex));

				// Init the data
				char *data = new char[dataLength * dataDepth];

				// Store the data
				char *rawData = dataList.at(imgIndex);
				memcpy(data, rawData, dataDepth * dataLength);
				Dataset *dataNode = new Dataset(dimList.count(),
					dimSizes, dataTypeList.at(imgIndex), data, false);
				dataNode->setName("data");
                dataNode->setStatus(Node::DIRTY);

				dataGroup->setData(dataNode);

                Attribute *groupType = new Attribute(dataGroup);
                groupType->setName("emd_group_type");
                groupType->setValue(QVariant(1));
			    groupType->setType(DataTypeInt32);
			    dataGroup->addChild(groupType);

			    Attribute *dataOrder = new Attribute(dataGroup);
                dataOrder->setName("data_order");
                dataOrder->setValue(QVariant(0));
			    dataOrder->setType(DataTypeInt32);
			    dataGroup->addChild(dataOrder);

				// Create dimensions
				Attribute *unitNode = 0;
				Dataset *dimNode;
				QString nameStem = "dim%1";
				for(int iii = 1; iii <= dimList.count(); ++iii)
				{
					dimNode = new Dataset(dimSizes[iii-1], DataTypeInt32, true);
					dimNode->setName(nameStem.arg(iii));
					dataGroup->addDim(dimNode);
					dimNode->setParentNode(dataGroup);
					
					Attribute *destNode = new Attribute(dimNode);
					destNode->setName("units");
				    destNode->setType(DataTypeString);
					dimNode->addChild(destNode);

					unitNode = static_cast<Attribute*>
						(imageDataNode->childAtPath("Calibrations/Dimension/"
															+ QString::number(iii)
															+ "/Units"));
					if(unitNode)
						destNode->setValue(unitNode->value());
					else
						destNode->setValue(QVariant("[px]"));

					destNode = new Attribute(dimNode);
					destNode->setName("name");
					destNode->setValue(QVariant(nameStem.arg(iii)));
				    destNode->setType(DataTypeString);
					dimNode->addChild(destNode);
				}
			}
		}
	}

	foreach(char *data, dataList)
		delete[] data;

	return ErrorNone;
}

FileManager::Error FileManager::loadTiff(const char *filePath, Model *emdModel)
{
    QString fileName(filePath);

	qDebug() << "Attempting .tiff load: " << fileName;

	// Attempt to open the tiff file
	QImage rawImage(fileName);
    if(rawImage.isNull()) 
	{
		qDebug() << "tiff load failed.";
        return ErrorFileOpenFailed;
    }
	// Initialize the image data
	const int dims[2] = {rawImage.width(), rawImage.height()};
	char *data = new char[dims[0] * dims[1]];

	// Store the data
	int index = 0;
	for(int row = 0; row < rawImage.height(); row++)
	{
		for(int col = 0; col < rawImage.width(); col++)
		{
			QRgb color = qGray(rawImage.pixel(row, col));
			data[index++] = (char)color;
		}
	}

	// Populate the image model
	Node *dataNode = emdModel->node("data");
	// If the data group is missing, something is wrong
	if(!dataNode)
		return ErrorInvalidOperation;
	
	// Create the data group
	DataGroup *dataGroup = (DataGroup*) emdModel->addNode("tiff_data", 
														Node::DATAGROUP, dataNode);
	//emdModel->setCurrentDataGroup(dataGroup);

    // Data group type attribute
    Attribute *groupType = new Attribute(dataGroup);
    groupType->setName("emd_group_type");
    groupType->setValue(QVariant(1));
	groupType->setType(DataTypeInt32);
	dataGroup->addChild(groupType);

	// Create the dataset
	Dataset *dataSet = new Dataset(2, dims, DataTypeUInt8, data);
	dataSet->setName("data");
	dataGroup->setData(dataSet);

	// Create the dims
	Dataset *dim1 = new Dataset(dims[0], DataTypeInt32, true);
	dim1->setName("dim1");
	dataGroup->addDim(dim1);
	dim1->setParentNode(dataGroup);

	Dataset *dim2 = new Dataset(dims[1], DataTypeInt32, true);
	dim2->setName("dim2");
	dataGroup->addDim(dim2);
	dim2->setParentNode(dataGroup);

	Attribute *node;
	// first dimension (x-axis)
	node = dynamic_cast<Attribute*>( emdModel->addNode
		("name", Node::ATTRIBUTE, dim1) );
	node->setValue(QVariant("x"));
    node->setType(DataTypeString);
	node = dynamic_cast<Attribute*>( emdModel->addNode
		("units", Node::ATTRIBUTE, dim1) );
	node->setValue(QVariant("[px]"));
    node->setType(DataTypeString);
	// second dimension (y-axis)
	node = dynamic_cast<Attribute*>( emdModel->addNode
		("name", Node::ATTRIBUTE, dim2) );
	node->setValue(QVariant("y"));
    node->setType(DataTypeString);
    node = dynamic_cast<Attribute*>( emdModel->addNode
		("units", Node::ATTRIBUTE, dim2) );
	node->setValue(QVariant("[px]"));
    node->setType(DataTypeString);

	return ErrorNone;
}

/************************************ Utility  **************************************/

DataType FileManager::serToEmdType(const int &serType)
{
    emd::DataType emdType;

    switch (serType)
    {
    case 1:
        emdType = DataTypeUInt8;
        break;
    case 2:
        emdType = DataTypeUInt16;
        break;
    case 3:
        emdType = DataTypeUInt32;
        break;
    case 4:
        emdType = DataTypeInt8;
        break;
    case 5:
        emdType = DataTypeInt16;
        break;
    case 6:
        emdType = DataTypeInt32;
        break;
    case 7:
        emdType = DataTypeFloat32;
        break;
    case 8:
        emdType = DataTypeFloat64;
        break;
    default:
        emdType = DataTypeUnknown;
        break;
    }

    return emdType;
}

emd::DataType FileManager::dm3ToEmdType(const int &type)
{
	emd::DataType emdType = DataTypeUnknown;
	switch(type)
	{
	case 8:		// bool
		emdType = DataTypeBool;
		break;
	case 9:		// char
		emdType = DataTypeInt8;
		break;
	case 10:	// octet
		emdType = DataTypeInt8;
		break;
	case 2:		// short
		emdType = DataTypeInt16;
		break;
	case 4:		// ushort
		emdType = DataTypeUInt16;
		break;
	case 3:		// long
		emdType = DataTypeInt32;
		break;
	case 5:		// ulong
		emdType = DataTypeUInt32;
		break;
	case 6:		// float
		emdType = DataTypeFloat32;
		break;
	case 7:		// double
		emdType = DataTypeFloat64;
		break;
	default:
		break;
	}
	return emdType;
}

int FileManager::dm3TypeDepth(const int &type) 
{
	// Returns the size in bytes of the dm3 data type
	int depth = -1;
	switch(type)
	{
	case 8:		// bool
	case 9:		// char
	case 10:	// octet
		depth = 1;
		break;
	case 2:		// short
	case 4:		// ushort
		depth = 2;
		break;
	case 3:		// long
	case 5:		// ulong
	case 6:		// float
		depth = 4;
		break;
	case 7:		// double
		depth = 8;
		break;
	default:
		break;
	}
	return depth;
}

QVariant readEmdType(QDataStream &stream, emd::DataType emdType)
{
	QVariant value;
	QDataStream::ByteOrder byteOrder = stream.byteOrder();
	stream.setByteOrder(QDataStream::LittleEndian);

	switch(emdType)
	{
	case DataTypeBool:
		value = readTypedValue<bool>(stream);
		break;
	case DataTypeInt8:
		value = readTypedValue<qint8>(stream);
		break;
	case DataTypeInt16:
		value = readTypedValue<qint16>(stream);
		break;
	case DataTypeUInt16:
		value = readTypedValue<quint16>(stream);
		break;
	case DataTypeInt32:
		value = readTypedValue<qint32>(stream);
		break;
	case DataTypeUInt32:
		value = readTypedValue<quint32>(stream);
		break;
	case DataTypeFloat32:
		stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
		value = readTypedValue<float>(stream);
		stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
		break;
	case DataTypeFloat64:
		value = readTypedValue<double>(stream);
		break;
	default:
		break;
	}

	stream.setByteOrder(byteOrder);
	return value;
}

QVariant readEmdArray(QDataStream &stream, emd::DataType emdType, const int &size)
{
	QVariant value;
	QDataStream::ByteOrder byteOrder = stream.byteOrder();
	stream.setByteOrder(QDataStream::LittleEndian);
	QString nameString = "";
	switch(emdType)
	{
	case DataTypeBool:
		for(int iii = 0; iii < size; ++iii)
		{
			value = readTypedValue<bool>(stream);
			nameString += value.toString() + " ";
		}
		break;
	case DataTypeInt8:
		for(int iii = 0; iii < size; ++iii)
		{
			value = readTypedValue<qint8>(stream);
			nameString += value.toString() + " ";
		}
		break;
	case DataTypeInt16:
		for(int iii = 0; iii < size; ++iii)
		{
			value = readTypedValue<qint16>(stream);
			nameString += value.toString() + " ";
		}
		break;
	case DataTypeUInt16:
		for(int iii = 0; iii < size; ++iii)
		{
			value = readTypedValue<quint16>(stream);
			nameString += value.toString() + " ";
		}
		break;
	case DataTypeInt32:
		for(int iii = 0; iii < size; ++iii)
		{
			value = readTypedValue<qint32>(stream);
			nameString += value.toString() + " ";
		}
		break;
	case DataTypeUInt32:
		for(int iii = 0; iii < size; ++iii)
		{
			value = readTypedValue<quint32>(stream);
			nameString += value.toString() + " ";
		}
		break;
	case DataTypeFloat32:
		stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
		for(int iii = 0; iii < size; ++iii)
		{
			value = readTypedValue<float>(stream);
			nameString += value.toString() + " ";
		}
		stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
		break;
	case DataTypeFloat64:
		for(int iii = 0; iii < size; ++iii)
		{
			value = readTypedValue<double>(stream);
			nameString += value.toString() + " ";
		}
		break;
	default:
		break;
	}
	
	stream.setByteOrder(byteOrder);
	return QVariant(nameString);
}

} // namespace emd