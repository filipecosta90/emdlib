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

#ifndef EMD_MODEL_H
#define EMD_MODEL_H

#include "EmdLib.h"

#include <QtGui>

#include "Util.h"
#include "DataGroup.h"

namespace H5
{
class H5Object;
class H5File;
}

namespace emd
{

class Node;
class DataGroup;
class DimensionModel;
class Dataset;
class Frame;

class EMDLIB_API Model : public QAbstractItemModel
{
	Q_OBJECT

 public:
	Model(QObject *parent = 0);
	Model(const Model &) {}
	~Model();

	// These functions are required to subclass QAbstractItemModel.
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex &parent = QModelIndex()) const;

	QModelIndex index(int row, int column,
					const QModelIndex &parent = QModelIndex()) const;
	QModelIndex parent(const QModelIndex &index) const;

	QVariant data(const QModelIndex &index, int role) const;
	bool setData(const QModelIndex &index, const QVariant &value,
				int role = Qt::EditRole);

	QVariant headerData(int section, Qt::Orientation orientation,
						int role = Qt::DisplayRole) const;
	bool setHeaderData(int section, Qt::Orientation orientation,
					const QVariant &value, int role = Qt::EditRole);

	Qt::ItemFlags flags(const QModelIndex &index) const;

	bool insertRows(int position, int rows, const QModelIndex &parent = QModelIndex());
	bool removeRows(int position, int rows, const QModelIndex &parent = QModelIndex());

	// Accessors
	QString fileName() const;
    QString fileDir() const;
    QString fileExtension() const;
    QString filePath() const;
    void setFilePath(const QString &path);

	H5::H5File *file() const {return m_file;}
	Node *root() const {return m_root;}
	Node *node(const QString &name, const Node *parent = 0) const;
	Node *currentNode() const {return m_currentNode;}
	Node *getPath(const QString &path) const;
    int dataGroupCount() const;
    DataGroup *dataGroupAtIndex(const int &index) const;
    bool isDataGroup(void *ptr) const;
	Node *getNode(const QModelIndex &index) const;
    bool canDeleteNode(const Node *node) const;
    bool ownsIndex(const QModelIndex &index) const;

    bool isDirty() const;

    void print();

	// Mutators
	Node *addNode(const QString &name, const int &type, Node *parent = 0);
	Node *addPath(const char *path, const int &type);
	void setCurrentNode(Node *node) {m_currentNode = node;}
	void setAxisIndexes(const int &hindex, const int &vIndex);
	void setDirty();

	// File operations
	bool open(const QString &filePath);
	void save(const QString &filePath);
	bool loadDataGroup(const int &groupIndex);
    bool loadDataGroup(DataGroup *dataGroup);
    void unloadDataGroups();
	int indexOfDataGroup(DataGroup *group);
    bool anyLoaded();

    void validateDataGroups();

private:
	// Data
	Node *m_root;
	Node *m_currentNode;
	QString m_fileName;
    QString m_fileDir;
    QString m_fileExtension;
	H5::H5File *m_file;
	// TODO: handle image setting more gracefully
	int m_currentType;

	QList<DataGroup*> m_dataGroups;

	bool m_chunking;
	int m_nFrames;
	char **m_chunks;
	int m_nChunks;
	long m_chunkSize;
	int *m_chunkDims;
	int m_nChunkDims;
	int *m_FrameSpaceDims;
	int *m_chunkCount;
	int m_imagesPerChunk;
	int m_hIndex;
	int m_vIndex;

	// Actions
	QAction *m_loadDataGroup;

	// Helper functions
	template <typename T> 
	void createDimVector(T offset, T spacing, T *&data, int length)
	{
		data = new T[length];
		data[0] = offset;
		for(int iii = 1; iii < length; ++iii)
			data[iii] = data[iii-1] + spacing;
	}
	void initChunks();
	int getChunkIndex(const int *indexes);
	void loadChunk(const int &chunkIndex);
};

} // namespace emd

#endif