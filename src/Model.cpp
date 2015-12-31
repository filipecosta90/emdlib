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

#include "Model.h"

#include <stdint.h>
#include <string.h>

#include <H5Cpp.h>

#include "Node.h"
#include "Group.h"
#include "DataGroup.h"
#include "Attribute.h"
#include "Dataset.h"
#include "DataSpace.h"
#include "Frame.h"

#ifndef H5_NO_NAMESPACE
using namespace H5;
#endif

namespace emd
{

Model::Model(QObject *parent)
	: QAbstractItemModel(parent)
{
	m_root = new Node();
	m_root->setName("root");

	/* We automatically create the following top level groups:
			data		- the actual stored data, contains zero or more data groups
			user		- information about the user who generated the data
			microscope	- informatiom about the microscope used
			sample		- information about the sample analyzed
			comments	- inormation about the file/data, e.g. history
	*/
	addNode("data",			Node::GROUP);
	addNode("user",			Node::GROUP);
	addNode("microscope",	Node::GROUP);
	addNode("sample",		Node::GROUP);
	addNode("comments",		Node::GROUP);

	// Init data
	m_chunks = 0;
	m_chunkDims = 0;
	m_FrameSpaceDims = 0;
	m_chunkCount = 0;
}

Model::~Model()
{
	if(m_chunkCount)
		delete [] m_chunkCount;
	if(m_FrameSpaceDims)
		delete [] m_FrameSpaceDims;
	if(m_chunkDims)
		delete [] m_chunkDims;
	if(m_chunks)
	{
		for(int iii = 0; iii < m_nChunks; ++iii)
			if(m_chunks[iii])
				delete [] m_chunks[iii];
		delete [] m_chunks;
	}

	if(m_root)
		delete m_root;
}

/****************** QAbstractItemModel implementation ********************/

int Model::rowCount(const QModelIndex &parent) const
// Returns the number of child rows of the element "parent"; if parent is
// not a valid index, zero is returned.
{
	// get the image node associated with the model index
	Node *node = getNode(parent);
	if(!node)
		return 0;

	int count = node->childCount();
    return count;
}

int Model::columnCount(const QModelIndex &/*parent*/) const
// Returns the number of columns of an item (always 2).
{
	return 2;
}

QModelIndex Model::index(int row, int column,
                    const QModelIndex &parent) const
{
	if(parent.isValid() && parent.column() != 0)
		return QModelIndex();

    if(!parent.isValid())
        return createIndex(0, 0, m_root);

	Node *parentNode = getNode(parent);

	Node *childNode = parentNode->child(row);
	if(childNode)
		return createIndex(row, column, childNode);
	else
		return QModelIndex();
}

QModelIndex Model::parent(const QModelIndex &index) const
{
	if(!index.isValid())
        return QModelIndex();

    Node *childNode = getNode(index);
	if(!childNode)
		return QModelIndex();

    Node *parentNode = childNode->parent();

    if(!parentNode)
        return QModelIndex();

    return createIndex(parentNode->rowNumber(), 0, parentNode);
}

QVariant Model::data(const QModelIndex &index, int role) const
{
	if(!index.isValid())
        return QVariant();

    //if (role != Qt::DisplayRole && role != Qt::EditRole)
    //    return QVariant();

    Node *node = getNode(index);
	if(!node)
		return QVariant();

	if(role == Qt::DisplayRole || role == Qt::EditRole)
	{
        if(node == m_root)
        {
            if(index.column() == 0)
                return QVariant(m_fileName);
            else if(index.column() == 1)
                return QVariant(m_fileExtension + (isDirty() ? "*" : ""));
		    else return QVariant();
        }
        else
        {
		    if(index.column() == 0)
			    return QVariant(node->name());
		    else if(index.column() == 1)
		    {
			    QVariant val = node->variantRepresentation();
			    return val;
		    }
		    else return QVariant();
        }
	}
	//else if(role == Qt::TextAlignmentRole)
	//{
	//	if(index.column() == 1)
	//		return Qt::AlignCenter;
	//	else
	//		return QVariant();
	//}
	//else if(role == Qt::DecorationRole)
	//{
	//	if(index.column() == 0)
	//		return facet->GetVariantColour();
	//	else
	//		return QVariant();
	//}
	else
		return QVariant();
}

bool Model::setData(const QModelIndex &index, const QVariant &value, int role)
{
	// If we're not editing or it's not the value column (column 1),
	// then we don't want to change the data.
	if(role != Qt::EditRole || index.column() != 1)
		return false;

	Node *node = getNode(index);
	if( !node || node == m_root || !(node->status() & Node::ATTRIBUTE) )
		return false;

	bool result = ((Attribute*)node)->setValue(value);

	if(result)
	{
		node->setStatus(Node::DIRTY);
		emit dataChanged(index, index);
	}

	return result;
}

QVariant Model::headerData(int section, Qt::Orientation /*orientation*/,
					int role) const
{
	if(role == Qt::DisplayRole)
	{
		if(section == 0)
			return QString("Element");
		else if(section == 1)
			return QString("Value");
	}
	else if(role == Qt::TextAlignmentRole)
		return Qt::AlignCenter;

    return QVariant();
}

bool Model::setHeaderData(int section, Qt::Orientation orientation,
                    const QVariant &/*value*/, int role)
{
	if(role != Qt::EditRole || orientation != Qt::Horizontal)
		return false;

	//bool result = rootFacet->SetData(section, value);

	//if (result)
		emit headerDataChanged(orientation, section, section);

	return true;
}

Qt::ItemFlags Model::flags(const QModelIndex &index) const
{
	if(!index.isValid())
		return 0;

	// TODO: only names and attribute values should be editable
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

bool Model::insertRows(int position, int rows, 
					const QModelIndex &parent)
{
	Node *parentNode = getNode(parent);
	if(!parentNode)
		parentNode = m_root;

	beginInsertRows(parent, position, position + rows - 1);
	bool success = parentNode->addChildren(position, rows, m_currentType);
	endInsertRows();

	return success;
}

bool Model::removeRows(int position, int rows, 
					const QModelIndex &parent)
{
	Node *parentNode = getNode(parent);
	if(!parentNode)
		parentNode = m_root;

	beginRemoveRows(parent, position, position + rows - 1);
	bool success = parentNode->removeChildren(position, rows);
	endRemoveRows();

	return success;
}

/*********************** Helper functions ***************************/

void Model::validateDataGroups()
{
    m_dataGroups.clear();

    QList<Node*> nodes;
    nodes.append(m_root);

    while(!nodes.empty())
    {
        Node *node = nodes.takeFirst();
        nodes.append(node->children());

        if((node->status() & Node::DATAGROUP) == Node::DATAGROUP)
        {
            DataGroup *dataGroup = static_cast<DataGroup*>(node);

            if(dataGroup->validate())
            {
                m_dataGroups.append(dataGroup);
                dataGroup->setModel(this);
            }
        }
    }
}

Node *Model::getNode(const QModelIndex &index) const
// Gets the Node associated with given model index; if the index is
// not valid, zero is returned.
{
	if(index.isValid()) 
	{
		Node *node = static_cast<Node*>(index.internalPointer());
		if(node) 
			return node;
	}

	return NULL;
}

bool Model::canDeleteNode(const Node *node) const
{
    if(!node)
        return false;

    if(node->parent() == m_root)
    {
        QString name = node->name();

        if(name.compare("data", Qt::CaseInsensitive) == 0)
            return false;
        if(name.compare("user", Qt::CaseInsensitive) == 0)
            return false;
        if(name.compare("microscope", Qt::CaseInsensitive) == 0)
            return false;
        if(name.compare("sample", Qt::CaseInsensitive) == 0)
            return false;
        if(name.compare("comments", Qt::CaseInsensitive) == 0)
            return false;
    }

    return true;
}

bool Model::ownsIndex(const QModelIndex &index) const
{
    if(!index.isValid())
        return false;

    Node *node = static_cast<Node*>(index.internalPointer());
    if(!node)
        return false;

    while(node->parent())
        node = node->parent();

    return node == m_root;
}

/**************************** Accessors ******************************/

QString Model::fileName() const
{
    return m_fileName;
}

QString Model::fileDir() const
{
    return m_fileDir;
}

QString Model::fileExtension() const
{
    return m_fileExtension;
}

QString Model::filePath() const
{
    return m_fileDir % m_fileName % "." % m_fileExtension;
}

void Model::setFilePath(const QString &path)
{
    QFileInfo fileInfo(path);

	m_fileName = fileInfo.completeBaseName();
    m_fileDir = fileInfo.absolutePath() + "/";
    m_fileExtension = fileInfo.suffix();
}

Node *Model::node(const QString &name, const Node *parent) const
{
	if(!parent)
		parent = m_root;

	return parent->child(name);
}
Node *Model::getPath(const QString &path) const
{
	QStringList nodes = path.split('/', QString::SkipEmptyParts);
	// Find the parent of the addressed node. For example, if the
	//	path is /data/dim1 then the parent is "data" in the root 
	//	group and the name of the new node is "dim1".
	Node *parent = m_root;
	int iii;
	for(iii = 0; iii < nodes.size() - 1; ++iii)
	{
		parent = parent->child(nodes.at(iii));
		// If the named node at this level does not exist, the
		//	operation is terminated.
		if(!parent)
			return 0;
	}
	// Now that we have the parent node, we access the desired
	//	child node and return it if it exists (else 0).
	return parent->child(nodes[iii]);
}

int Model::dataGroupCount() const
{
    return m_dataGroups.count();
}

DataGroup *Model::dataGroupAtIndex(const int &index) const
{
    if(index < m_dataGroups.count())
        return m_dataGroups.at(index);

    return NULL;
}

bool Model::isDataGroup(void *ptr) const
{
    return m_dataGroups.contains((DataGroup *)ptr);
}

void Model::print()
{
    QList<QModelIndex> indexes;

    //for(int iii = 0; iii < this->rowCount(); ++iii)
        indexes.append(this->index(0, 0));

    while(indexes.count() > 0)
    {
        QModelIndex index = indexes.takeFirst();

        for(int iii = 0; iii < this->rowCount(index); ++iii)
            indexes.append(this->index(iii, 0, index));

        qDebug() << this->data(index, Qt::DisplayRole).toString() << "(" << this->data(this->parent(index), Qt::DisplayRole).toString() << ")";
    }
}

/***************************** Mutators ******************************/

Node *Model::addNode(const QString &name, const int &type, Node *parent)
{
	// Get the parent's model index
	QModelIndex modelIndex = QModelIndex();
	if(parent)
		modelIndex = createIndex(parent->rowNumber(), 0, parent);
	else
		parent = m_root;

	// Create the new node
	m_currentType = type;
	insertRows(parent->childCount(), 1, modelIndex);

	// Set the node data
	Node *node = parent->child(parent->childCount() - 1);
	node->setName(name);
	node->setStatus(type);		// TODO: handle dirty better

	return node;
}
Node *Model::addPath(const char* path, const int &type)
{
	// Break the path into its constituent groups
	QString sPath(path);
	QStringList nodes = sPath.split('/', QString::SkipEmptyParts);
	// Find the parent of the addressed node. For example, if the
	//	path is /data/dim1 then the parent is "data" in the root 
	//	group and the name of the new node is "dim1".
	Node *parent = m_root;
	int iii;
	for(iii = 0; iii < nodes.size() - 1; ++iii)
	{
		parent = parent->child(nodes.at(iii));
		// If the named node at this level does not exist, the
		//	operation is terminated.
		if(!parent)
			return 0;
	}
	// Now that we have the parent node, the child node is added
	//	as normal. If it already exists, it is skipped.
	Node *child = parent->child(nodes[iii]);
	if(!child)
		child = addNode(nodes[iii], type, parent);

	return child;
}
void Model::setAxisIndexes(const int &hIndex, const int &vIndex)
{
	m_hIndex = hIndex;
	m_vIndex = vIndex;
	initChunks();
}
void Model::initChunks()
{
	//// Delete old data
	//if(m_chunks)
	//{
	//	for(int iii = 0; iii < m_nChunks; ++iii)
	//		if(m_chunks[iii])
	//			delete [] m_chunks[iii];
	//	delete [] m_chunks;
	//}

	//m_nChunkDims = m_nDims - 2;
	//// Calculate image size and count
	//long imageSize = m_depth * m_dim[m_hIndex] * m_dim[m_vIndex];
	//m_nFrames = 1;
	//if(m_nChunkDims > 0)
	//{
	//	m_FrameSpaceDims = new int[m_nChunkDims];
	//	m_chunkCount = new int[m_nChunkDims];
	//}
	//int count = 0;
	//for(int iii = m_nDims-1; iii >= 0; --iii)
	//{
	//	if(iii != m_hIndex && iii != m_vIndex)
	//	{
	//		m_nFrames *= m_dim[iii];
	//		m_FrameSpaceDims[count++] = m_dim[iii];
	//	}
	//}
	//long totalImageSize = imageSize * m_nFrames;

	//// Determine how many images will fit in a chunk
	//
	//if(0 == m_nChunkDims)				// 2D data set (single image)
	//{
	//	m_chunkDims = new int(1);
	//	m_chunkCount = new int[1];
	//	m_nChunks = 1;
	//}
	//else
	//{
	//	m_chunkDims = new int[m_nChunkDims];
	//	// Determine appropriate chunk dimensions:
	//	if(1 == m_nChunkDims)
	//	{
	//		m_chunkCount[0] = totalImageSize / CHUNK_SIZE + (totalImageSize % CHUNK_SIZE > 0 ? 1 : 0);
	//		m_chunkDims[0] = m_FrameSpaceDims[0] / m_chunkCount[0] 
	//					+ (m_FrameSpaceDims[0] % m_chunkCount[0] > 0 ? 1 : 0);
	//		m_nChunks = m_chunkCount[0];
	//		m_chunkSize = m_chunkDims[0] * imageSize;
	//	}
	//	else if(2 == m_nChunkDims)
	//	{
	//		long target = (long)sqrt((double)CHUNK_SIZE / imageSize);
	//		m_chunkDims[0] = m_chunkDims[1] = target;
	//		m_chunkCount[0] = m_FrameSpaceDims[0] / m_chunkDims[0] 
	//					+ (m_FrameSpaceDims[0] % m_chunkDims[0] > 0 ? 1 : 0);
	//		m_chunkCount[1] = m_FrameSpaceDims[1] / m_chunkDims[1] 
	//					+ (m_FrameSpaceDims[1] % m_chunkDims[1] > 0 ? 1 : 0);
	//		m_nChunks = m_chunkCount[0] * m_chunkCount[1];
	//		m_chunkSize = m_chunkDims[0] * m_chunkDims[1] * imageSize;
	//	}
	//}

	//// Initialize the chunks
	//m_chunks = new char*[m_nChunks];
	//for(int iii = 0; iii < m_nChunks; ++iii)
	//	m_chunks[iii] = 0;
}
void Model::loadChunk(const int &/*chunkIndex*/)
{
	//try {
	//	Exception::dontPrint();

	//	QByteArray ba = m_fileName.toLocal8Bit();
	//	m_file = new H5File(ba.constData(), H5F_ACC_RDWR);

	//	// File dataspace
	//	DataSet data = m_file->openDataSet("/data/data");
	//	DataSpace fileSpace = data.getSpace();

	//	// File hyperslab parameters
	//	hsize_t *fileCount	= new hsize_t[m_nDims];
	//	hsize_t *fileOffset = new hsize_t[m_nDims];
	//	hsize_t *fileStride = new hsize_t[m_nDims];
	//	hsize_t *fileBlock	= new hsize_t[m_nDims];

	//	int index = 0;
	//	for(int iii = 0; iii < m_nDims; ++iii)
	//	{
	//		fileCount[iii]	= 1;
	//		fileOffset[iii] = 0;
	//		fileStride[iii] = 1;
	//		fileBlock[iii]	= 1;
	//		if(iii != m_hIndex && iii != m_vIndex)
	//			fileCount[iii] = m_chunkDims[index++];
	//	}
	//	fileBlock[m_hIndex] = m_dim[m_hIndex];
	//	fileBlock[m_vIndex] = m_dim[m_vIndex];

	//	// Select hyperslab
	//	fileSpace.selectHyperslab(H5S_SELECT_SET, fileCount, fileOffset, fileStride, fileBlock);

	//	// Memory dataspace
	//	hsize_t memSize = m_chunkSize / m_depth;	// Number of data points per chunk
	//	DataSpace memSpace = DataSpace(1, &memSize);

	//	// Memory hyperslab parameters
	//	//hsize_t memCount  = m_imagesPerChunk;
	//	//hsize_t memOffset = 0;
	//	//hsize_t memStride = m_dim[m_hIndex] * m_dim[m_vIndex];
	//	//hsize_t memBlock = memStride;

	//	// Select hyperslab
	//	//fileSpace.selectHyperslab(H5S_SELECT_SET, &memCount, &memOffset, &memStride, &memBlock);

	//	// Copy data
	//	m_chunks[chunkIndex] = new char[m_chunkSize];
	//	data.read(m_chunks[chunkIndex], m_dataType, memSpace, fileSpace);

	//	// Create the Frames of this chunk
	//	//int FrameIndex = chunkIndex * m_imagesPerChunk;
	//	//char *dataPointer = m_chunks[chunkIndex];
	//	//int dataStride = 
	//}	
	//// catch failure caused by the H5File operations
	//catch(FileIException error) {
	//	qDebug() << "Bad file operation.";
	//	return;
	//}
	//// catch failure caused by the DataSet operations
	//catch(DataSetIException error) {
	//	qDebug() << "Bad dataset operation.";
	//	return;
	//}
	//// catch failure caused by the DataSpace operations
	//catch(DataSpaceIException error) {
	//	qDebug() << "Bad dataspace operation.";
	//	return;
	//}
	//// catch failure caused by the Attribute operations
	//catch(AttributeIException error){
	//	qDebug() << "Bad attribute operation.";
	//	return;
	//}
}

bool Model::isDirty() const
{
    return m_root->status() & Node::DIRTY;
}

void Model::setDirty()
{
	// Pass dirty status to the whole node tree
	m_root->setStatus(Node::DIRTY, true);
}

int Model::getChunkIndex(const int * /*indexes*/)
{
	//if(2 == m_nDims)
		return 0;

	//int offset = 1;
	//int chunkIndex = 0;
	//int count = m_nChunkDims-1;
	//for(int iii = m_nDims-1; iii >= 0; --iii)
	//{
	//	if(iii != m_hIndex && iii != m_vIndex)
	//	{
	//		chunkIndex += (indexes[iii] / m_chunkDims[count]) * offset;
	//		offset *= m_chunkCount[count--];
	//	}
	//}
	////int *chunkCounts = new int[m_nChunkDims];

	//return chunkIndex;
}

/*************************** File operations *************************/

herr_t parseAttribute(hid_t objID, const char *name, 
			const H5A_info_t * /*ainfo*/, void *opData)
{
	Model *model = (Model*) opData;

	hid_t attrID = H5Aopen(objID, name, H5P_DEFAULT);
	H5::Attribute atr(attrID);
	H5T_class_t typeClass = atr.getTypeClass();
	hid_t ftype = H5Aget_type(atr.getId());  
	size_t byteSize = H5Tget_size(ftype);
	H5T_sign_t sign = H5Tget_sign(atr.getDataType().getId());
	uint64_t totalSize = atr.getInMemDataSize();
	uint64_t length = totalSize / byteSize;

	Attribute *node = dynamic_cast<Attribute*>( model->addNode(name, 
							Node::ATTRIBUTE, model->currentNode()) );

	if(H5T_INTEGER == typeClass)	// TODO: replace cascading ifs with switches
	{
		if(byteSize == 1)
		{
			if(sign == 1)	// signed
			{
				char *data = new char[length];
				atr.read(atr.getDataType(), data);
				node->setType(DataTypeInt8);
				node->storeData(data, length);
				delete[] data;
			}
			else if(sign == 0)	// unsigned
			{
				unsigned char *data = new unsigned char[length];
				atr.read(atr.getDataType(), data);
				node->setType(DataTypeUInt8);
				node->storeData(data, length);
				delete[] data;
			}
			else
				qWarning() << "Error reading attribute signedness for " << QString(name);
		}
		else if(byteSize == 2)
		{
			if(sign == 1)
			{
				qint16 *data = new qint16[length];
				atr.read(atr.getDataType(), data);
				node->setType(DataTypeInt16);
				node->storeData(data, length);
				delete[] data;
			}
			else if(sign == 0)
			{
				quint16 *data = new quint16[length];
				atr.read(atr.getDataType(), data);
				node->setType(DataTypeUInt16);
				node->storeData(data, length);
				delete[] data;
			}
			else
				qWarning() << "Error reading attribute signedness for " << QString(name);
		}
		else if(byteSize == 4)
		{
			if(sign == 1)
			{
				qint32 *data = new qint32[length];
				atr.read(atr.getDataType(), data);
				node->setType(DataTypeInt32);
				node->storeData(data, length);
				delete[] data;
			}
			else if(sign == 0)
			{
				quint32 *data = new quint32[length];
				atr.read(atr.getDataType(), data);
				node->setType(DataTypeUInt32);
				node->storeData(data, length);
				delete[] data;
			}
			else
				qWarning() << "Error reading attribute signedness for " << QString(name);
		}
		else if(byteSize == 8)
		{
			if(sign == 1)
			{
				qint64 *data = new qint64[length];
				atr.read(atr.getDataType(), data);
				node->setType(DataTypeInt64);
				node->storeData(data, length);
				delete[] data;
			}
			else if(sign == 0)
			{
				quint64 *data = new quint64[length];
				atr.read(atr.getDataType(), data);
				node->setType(DataTypeUInt64);
				node->storeData(data, length);
				delete[] data;
			}
			else
				qWarning() << "Error reading attribute signedness for " << QString(name);
		}
		else
			qWarning() << "Error: unsupported bit depth for attribute " << QString(name);
	}
	else if(H5T_FLOAT == typeClass)
	{
		if(byteSize == 4)
		{
				float *data = new float[length];
				atr.read(atr.getDataType(), data);
				node->setType(DataTypeFloat32);
				node->storeData(data, length);
				delete[] data;
		}
		else if(byteSize == 8)
		{
				double *data = new double[length];
				atr.read(atr.getDataType(), data);
				node->setType(DataTypeFloat64);
				node->storeData(data, length);
				delete[] data;
		}
		else
			qWarning() << "Error: unsupported bit depth for attribute " << QString(name);
	}
	else if(H5T_STRING == typeClass)
	{
		char *data;

		H5::DataType type = atr.getDataType();
		if(type.isVariableStr())
			atr.read(type, &data);
		else
		{
			data = new char[totalSize];
			atr.read(type, data);
		}

		QString *strings = new QString[length];
		for(int iii = 0; iii < length; ++iii)
		{
			char *element = data  + iii*byteSize;
			QString value = QString::fromUtf8(element, (int)byteSize);

			value.remove(QChar('\0'));
			//value = value + '\0';
			strings[iii] = value;
		}
		
		node->setType(DataTypeString);
		node->storeData(strings, length);

		if(!type.isVariableStr())
			delete[] data;	// TODO: will this leak memory for variable strings?
		delete[] strings;
	}
	else
		qWarning() << "Unsupported data type: " << typeClass << " for attribute " << QString(name);

	return 0;
}

herr_t checkDataGroup(hid_t objID, const char *name, 
			const H5A_info_t * /*ainfo*/, void *opData)
{
    bool *isDataGroup = static_cast<bool*>(opData);

    // If we've already identified the group as a data group, just return.
    if(*isDataGroup)
        return 0;

    // If this is not a group type marker, skip it. 
    if(strcmp(name, "emd_group_type") != 0)
        return 0;

	hid_t attrID = H5Aopen(objID, name, H5P_DEFAULT);
	H5::Attribute atr(attrID);

	H5T_class_t typeClass = atr.getTypeClass();

	hid_t ftype = H5Aget_type(atr.getId());  
	size_t byteSize = H5Tget_size(ftype);

	H5T_sign_t sign = H5Tget_sign(atr.getDataType().getId());

	if(H5T_INTEGER != typeClass)
        return 0;

    switch(byteSize)
	{
    case 1:
		if(sign == 1)
		{
			int8_t *data = new int8_t(0);
			atr.read(atr.getDataType(), data);
            *isDataGroup = (*data == 1);
			delete data;
		}
		else if(sign == 0)
		{
			uint8_t *data = new uint8_t(0);
			atr.read(atr.getDataType(), data);
            *isDataGroup = (*data == 1);
			delete data;
		}
        break;

    case 2:
		if(sign == 1)
		{
			int16_t *data = new int16_t(0);
			atr.read(atr.getDataType(), data);
            *isDataGroup = (*data == 1);
			delete data;
		}
		else if(sign == 0)
		{
			uint16_t *data = new uint16_t(0);
			atr.read(atr.getDataType(), data);
            *isDataGroup = (*data == 1);
			delete data;
		}
        break;

    case 4:
		if(sign == 1)
		{
			int32_t *data = new int32_t(0);
			atr.read(atr.getDataType(), data);
            *isDataGroup = (*data == 1);
			delete data;
		}
		else if(sign == 0)
		{
			uint32_t *data = new uint32_t(0);
			atr.read(atr.getDataType(), data);
            *isDataGroup = (*data == 1);
			delete data;
		}
        break;

    case 8:
		if(sign == 1)
		{
			int64_t *data = new int64_t(0);
			atr.read(atr.getDataType(), data);
            *isDataGroup = (*data == 1);
			delete data;
		}
		else if(sign == 0)
		{
			uint64_t *data = new uint64_t(0);
			atr.read(atr.getDataType(), data);
            *isDataGroup = (*data == 1);
			delete data;
		}
        break;

    default:
		qWarning() << "Error: unsupported bit depth for attribute " << QString(name);
        break;
	}

	return 0;
}

herr_t parseFileNode(hid_t fileID, const char *name, const H5O_info_t *info,
            void *data)
{
    // Skip the root node.
    if (name[0] == '.')
        return 0;

	// Convert the data back to the Model.
	Model *model = static_cast<Model*>(data);
	Node *node;
	hid_t objID = H5Oopen(fileID, name, H5P_DEFAULT);

    switch (info->type) 
	{
    case H5O_TYPE_GROUP:
    {
        bool isDataGroup = false;
        H5Aiterate(objID, H5_INDEX_NAME, H5_ITER_NATIVE, 0,
			checkDataGroup, &isDataGroup);

        if(isDataGroup)
        {
            node = model->addPath(name, Node::DATAGROUP);
            if(!node)
            {
                qWarning() << "Invalid data group: " << name;
                node = model->addPath(name, Node::GROUP);
            }
        }
        else
        {
		    node = model->addPath(name, Node::GROUP);
        }

        if(node)
        {
		    model->setCurrentNode(node);
		    H5Aiterate(objID, H5_INDEX_NAME, H5_ITER_NATIVE, 0,
			    parseAttribute, model);
        }
        else
        {
            qWarning() << "Failed to create group: " << name;
        }

        break;
    }
    case H5O_TYPE_DATASET:
        node = model->addPath(name, Node::DATASET);
		model->setCurrentNode(node);
		H5Aiterate(objID, H5_INDEX_NAME, H5_ITER_NATIVE, 0,
			parseAttribute, model);
		{
			DataSet dataSet(objID);
			(dynamic_cast<Dataset*>(node))
				->setDataSpace(DataSpace::fromHdfDataSet(dataSet));
			(dynamic_cast<Dataset*>(node))
				->setDataType(dataTypeFromHdfDataSet(dataSet));
		}
        break;
    case H5O_TYPE_NAMED_DATATYPE:
        qDebug() << "datatype: " << name;
        break;
    default:
        qDebug() << "unknown type: " << name;
    }

	H5Oclose(objID);

    return 0;
}

bool Model::open(const QString &filePath)
{
    this->setFilePath(filePath);

    qDebug() << (m_fileDir + m_fileName + "." + m_fileExtension);

	m_file = 0;
	try {
		// Open the file in read-write mode.
		Exception::dontPrint();
		QByteArray ba = filePath.toLocal8Bit();
		const char *path = ba.constData();
		m_file = new H5File(path, H5F_ACC_RDWR);
	
		// Go through all of the objects in the root group and process them
		//	appropriately. A recursive parsing function is called on each
		//	group that is found. We pass in a pointer to this Model which
		//	is required to create and manipulate nodes.
		H5Ovisit(m_file->getId(), H5_INDEX_NAME, H5_ITER_NATIVE, 
			parseFileNode, (void*)this);

		validateDataGroups();

		// Clean up
		if(m_file)
		{
			m_file->close();
			delete m_file;
		}
	}
	// catch failure caused by the H5File operations
	catch(FileIException error) {
		qDebug() << "Bad file operation.";
		// Clean up
		if(m_file)
		{
			m_file->close();
			delete m_file;
		}
		return false;
	}
	// catch failure caused by the DataSet operations
	catch(DataSetIException error) {
		qDebug() << "Bad dataset operation.";
		// Clean up
		if(m_file)
		{
			m_file->close();
			delete m_file;
		}
		return false;
	}
	// catch failure caused by the DataSpace operations
	catch(DataSpaceIException error) {
		qDebug() << "Bad dataspace operation.";
		// Clean up
		if(m_file)
		{
			m_file->close();
			delete m_file;
		}
		return false;
	}
	// catch failure caused by the Attribute operations
	catch(AttributeIException error){
		qDebug() << "Bad attribute operation.";
		// Clean up
		if(m_file)
		{
			m_file->close();
			delete m_file;
		}
		return false;
	}
	return true;
}

void Model::save(const QString &filePath)
{
	// Update the file name (it might not have changed).
	this->setFilePath(filePath);

	const H5std_string FILE_NAME = filePath.toLocal8Bit().constData();
	H5File *file;
	try {
		// Disable exception printing
		Exception::dontPrint();

		QByteArray ba = filePath.toLocal8Bit();
		// First, attempt to open the file. This call will fail if the
		//	file does not exist.
		bool fileExists = true;
		try {
			file = new H5File(ba.constData(), H5F_ACC_RDWR);
		} 
		catch(FileIException error) {
			fileExists = false;
		}

		// If the open fails, create a new file
		if(!fileExists)
		{
			file = new H5File(ba.constData(), H5F_ACC_TRUNC);
		}

		// Save all of the nodes (groups and attributes)
		m_root->save(QString(""), (H5::Group*)file);
		
		// Clean up
		if(file)
		{
			file->close();
			delete file;
		}
	}

	// catch failure caused by the H5File operations
	catch(FileIException error) {
		qDebug() << "Bad file operation.";
		if(file)
		{
			file->close();
			delete file;
		}
	}
	// catch failure caused by the DataSet operations
	catch(DataSetIException error) {
		qDebug() << "Bad dataset operation.";
		if(file)
		{
			file->close();
			delete file;
		}
	}
	// catch failure caused by the DataSpace operations
	catch(DataSpaceIException error) {
		qDebug() << "Bad dataspace operation.";
		if(file)
		{
			file->close();
			delete file;
		}
	}
	// catch failure caused by the Attribute operations
	catch(AttributeIException error){
		qDebug() << "Bad attribute operation.";
		if(file)
		{
			file->close();
			delete file;
		}
	}
}

bool Model::loadDataGroup(const int &groupIndex)
{
	if(groupIndex < 0 || groupIndex >= m_dataGroups.count())
		return false;

	DataGroup *newGroup = m_dataGroups.at(groupIndex);

    if(newGroup->isLoaded())
    {
        return true;
    }

	try {
		// Open the file in read-write mode.
		Exception::dontPrint();
        QString filePath = m_fileDir + m_fileName + "." + m_fileExtension;
		QByteArray ba = filePath.toLocal8Bit();
		m_file = new H5File(ba.constData(), H5F_ACC_RDONLY);
		// TODO: verify file integrity
		
		bool success = newGroup->load(m_file);
		if(!success)
		{
			m_file->close();
			delete m_file;
			return false;
		}

		// Clean up
		m_file->close();
		delete m_file;
	}
	// catch failure caused by the H5File operations
	catch(FileIException error) {
		qDebug() << "Bad file operation.";
		// Clean up
		m_file->close();
		delete m_file;
		return false;
	}
	// catch failure caused by the DataSet operations
	catch(DataSetIException error) {
		qDebug() << "Bad dataset operation: " << error.getCDetailMsg();
		// Clean up
		m_file->close();
		delete m_file;
		return false;
	}
	// catch failure caused by the DataSpace operations
	catch(DataSpaceIException error) {
		qDebug() << "Bad dataspace operation.";
		// Clean up
		m_file->close();
		delete m_file;
		return false;
	}
	// catch failure caused by the Attribute operations
	catch(AttributeIException error){
		qDebug() << "Bad attribute operation.";
		// Clean up
		m_file->close();
		delete m_file;
		return false;
	}

	return true;
}

bool Model::loadDataGroup(DataGroup *dataGroup)
{
    return this->loadDataGroup(indexOfDataGroup(dataGroup));
}

void Model::unloadDataGroups()
{
    foreach(DataGroup *dataGroup, m_dataGroups)
        dataGroup->unload();
}

int Model::indexOfDataGroup(DataGroup *group)
{
	int index = -1;
	QString name = group->name();
	for(int iii = 0; iii < m_dataGroups.count(); ++iii)
	{
		if(m_dataGroups.at(iii)->name().compare(name) == 0)
		{
			index = iii;
			break;
		}
	}
	return index;
}

bool Model::anyLoaded()
{
    for(DataGroup *group : m_dataGroups)
    {
        if(group->isLoaded())
            return true;
    }

    return false;
}

} // namespace emd
