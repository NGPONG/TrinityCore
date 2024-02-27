/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "DBCStore.h"
#include "DBCDatabaseLoader.h"

DBCStorageBase::DBCStorageBase(char const* fmt) : _fieldCount(0), _fileFormat(fmt), _dataTable(nullptr), _indexTableSize(0)
{
}

DBCStorageBase::~DBCStorageBase()
{
    delete[] _dataTable;
    for (char* strings : _stringPool)
        delete[] strings;
}

bool DBCStorageBase::Load(char const* path, char**& indexTable)
{
    indexTable = nullptr;

    DBCFileLoader dbc;
    // 读取 .dbc 文件的二进制数据，数据存放于 data 与 stringTable 中
    // data: 是一整个大内存，包含 records(所有行) 的数据 + stringTable 的数据
    // stringTable: 单独指向 stringTable 那块数据首地址的指针
    // Check if load was sucessful, only then continue
    if (!dbc.Load(path, _fileFormat))
        return false;

    // 字段数量
    _fieldCount = dbc.GetCols();

    // 此函数通过 DBCFileLoader 加载指定 dbc 文件中的 record 数据，加载完的数据可以直接作为二维表使用
    // 返回的数据为 record(加载后的) 二维表退化为一维表的体现
    // indexTable 为索引 _dataTable 每一行的数据，每一个元素代表每一行的起始地址
    // _indexTableSize 为总行数
    // load raw non-string data
    _dataTable = dbc.AutoProduceData(_fileFormat, _indexTableSize, indexTable);

    // 将 stringTable 初始化为 stringBlock
    // load strings from dbc data
    if (char* stringBlock = dbc.AutoProduceStrings(_fileFormat, _dataTable))
        _stringPool.push_back(stringBlock);

    // error in dbc file at loading if NULL
    return indexTable != nullptr;
}

bool DBCStorageBase::LoadStringsFrom(char const* path, char** indexTable)
{
    // DBC must be already loaded using Load
    if (!indexTable)
        return false;

    DBCFileLoader dbc;
    // Check if load was successful, only then continue
    if (!dbc.Load(path, _fileFormat))
        return false;

    // load strings from another locale dbc data
    if (char* stringBlock = dbc.AutoProduceStrings(_fileFormat, _dataTable))
        _stringPool.push_back(stringBlock);

    return true;
}

void DBCStorageBase::LoadFromDB(char const* table, char const* format, char const* index, char**& indexTable)
{
    _stringPool.push_back(DBCDatabaseLoader(table, format, index, _fileFormat, _stringPool).Load(_indexTableSize, indexTable));
}
