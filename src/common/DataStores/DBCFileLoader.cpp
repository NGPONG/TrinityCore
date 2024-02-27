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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DBCFileLoader.h"
#include "Errors.h"

DBCFileLoader::DBCFileLoader() : recordSize(0), recordCount(0), fieldCount(0), stringSize(0), fieldsOffset(nullptr), data(nullptr), stringTable(nullptr) { }

bool DBCFileLoader::Load(char const* filename, char const* fmt)
{
    //                  ┌─┬─┬─┬─┬─┬────────────────────┬──────────────────────────────────┐
    //            data◄─┤ │ │ │ │ │    records_data    │         strings_data             │
    //                  └┬┴┬┴┬┴┬┴┬┴────────────────────┴───────────────┬──────────────────┘
    //                   │ │ │ │ │         ▲                           │
    //          header◄──┘ │ │ │ │         │                           │
    //                     │ │ │ │         │                           ▼
    //     recordCount◄────┘ │ │ │         │                      stringTable
    //                       │ │ │         │
    // fieldCount(fmt)◄──────┘ │ │         │
    //                         │ │         │
    // recordSize(fmt)◄────────┘ │         │
    //                           │         │
    //      stringSize◄──────────┘         │
    //                                     │
    //                                     │
    //                  ┌─┬─┬─┬─┬──────────┴-──────────────────┐
    //    fieldsOffset◄─┤ │ │ │ │                              │
    //                  └┬┴┬┴┬┴┬┴──────────────────────────────┘
    //                   │ │ │ │
    //                   │ │ │ │
    //                   │ │ │ └──►4...
    //                   │ │ │
    //                   │ │ └────►3field offset
    //                   │ │
    //                   │ └──────►2field offset
    //                   │
    //                   └────────►1field offset

    uint32 header;
    if (data)
    {
        delete [] data;
        data = nullptr;
    }

    FILE* f = fopen(filename, "rb");
    if (!f)
        return false;

    if (fread(&header, 4, 1, f) != 1)                        // Number of records
    {
        fclose(f);
        return false;
    }

    EndianConvert(header);

    if (header != 0x43424457)                                //'WDBC'
    {
        fclose(f);
        return false;
    }

    // record 数量，这里虽然用的是 record 的属于，但是其实就是一条 row
    // 一个 record 可以对应一个 <record_name>Entry，<filename> 这个文件
    // 存储的二进制数据起始就是所有 record 的扁平的数据
    if (fread(&recordCount, 4, 1, f) != 1)                   // Number of records
    {
        fclose(f);
        return false;
    }

    EndianConvert(recordCount);

    // 字段数量，即多少列，具体参考 <record_name>Entry 中相关的注释
    //  * 这里包含注释掉的成员
    if (fread(&fieldCount, 4, 1, f) != 1)                    // Number of fields
    {
        fclose(f);
        return false;
    }

    EndianConvert(fieldCount);

    // record 的大小，可以将其理解为 <record_name>Entry 中所有成员的大小，但是特殊字段会有不同的解释
    //  * 具体大小的计算还是要感觉 fmt 参数来，即参考 DBCFileLoader::GetFormatRecordSize 函数的逻辑
    //  * 这里包含注释掉的成员
    if (fread(&recordSize, 4, 1, f) != 1)                    // Size of a record
    {
        fclose(f);
        return false;
    }

    EndianConvert(recordSize);

    if (fread(&stringSize, 4, 1, f) != 1)                    // String size
    {
        fclose(f);
        return false;
    }

    EndianConvert(stringSize);

    // field 用于索引 offset 的表，比如我需要第 1 列的内容则通过该
    // 成员能够返回对应列在内存中的 offset
    fieldsOffset = new uint32[fieldCount];
    fieldsOffset[0] = 0;
    for (uint32 i = 1; i < fieldCount; ++i)
    {
        fieldsOffset[i] = fieldsOffset[i - 1];

        if (fmt[i - 1] == 'b' || fmt[i - 1] == 'X')         // byte fields
            fieldsOffset[i] += sizeof(uint8);
        else                                                // 4 byte fields (int32/float/strings)
            fieldsOffset[i] += sizeof(uint32);
    }

    // 一大块内存，包含 [record][string_table]
    data = new unsigned char[recordSize * recordCount + stringSize];

    // stringTable 是单独一块内存用于存储 fmt 中表示为 's' 的实际数据
    // 而前段(recordSize * recordCount)内存中的 's' 则表示在 StringTable 中的 offset
    stringTable = data + recordSize*recordCount;

    if (fread(data, recordSize * recordCount + stringSize, 1, f) != 1)
    {
        fclose(f);
        return false;
    }

    fclose(f);

    return true;
}

DBCFileLoader::~DBCFileLoader()
{
    delete[] data;

    delete[] fieldsOffset;
}

DBCFileLoader::Record DBCFileLoader::getRecord(size_t id)
{
    ASSERT(data);
    return Record(*this, data + id * recordSize);
}

uint32 DBCFileLoader::GetFormatRecordSize(char const* format, int32* index_pos)
{
    // example:
    // format = niiiixxxxxissssssssssssssssxiiiiixxx
    // struct = {
    //   uint32 ID;                                              // 0
    //   uint32 ContinentID;                                     // 1
    //   uint32 ParentAreaID;                                    // 2 if 0 then it's zone, else it's zone id of this area
    //   uint32 AreaBit;                                         // 3
    //   uint32 Flags;                                           // 4
    //   //uint32 SoundProviderPref;                             // 5
    //   //uint32 SoundProviderPrefUnderwater;                   // 6
    //   //uint32 AmbienceID;                                    // 7
    //   //uint32 ZoneMusic;                                     // 8
    //   //uint32 IntroSound;                                    // 9
    //   int32 ExplorationLevel;                                 // 10
    //   char const* AreaName[16];                               // 11-26
    //   //uint32 AreaName_lang_mask;                            // 27
    //   uint32 FactionGroupMask;                                // 28
    //   uint32 LiquidTypeID[4];                                 // 29-32 liquid override by type
    //   //float MinElevation;                                   // 33
    //   //float AmbientMultiplier;                              // 34
    //   //uint32 LightID;                                       // 35
    // }

    uint32 recordsize = 0;
    int32 i = -1;
    for (uint32 x = 0; format[x]; ++x)
    {
        switch (format[x])
        {
            case FT_FLOAT: // f
                recordsize += sizeof(float);
                break;
            case FT_INT: // i
                recordsize += sizeof(uint32);
                break;
            case FT_STRING: // s
                recordsize += sizeof(char*);
                break;
            case FT_SORT: // d
                i = x;
                break;
            case FT_IND: // n
                i = x;
                recordsize += sizeof(uint32);
                break;
            case FT_BYTE: // b
                recordsize += sizeof(uint8);
                break;
            case FT_NA: // x
            case FT_NA_BYTE: // X
                break;
            case FT_LOGIC: // l
                ABORT_MSG("Attempted to load DBC files that do not have field types that match what is in the core. Check DBCfmt.h or your DBC files.");
                break;
            default:
                ABORT_MSG("Unknown field format character in DBCfmt.h");
                break;
        }
    }

    if (index_pos)
        *index_pos = i;

    return recordsize;
}

char* DBCFileLoader::AutoProduceData(char const* format, uint32& records, char**& indexTable)
{
    /*
    format STRING, NA, FLOAT, NA, INT <=>
    struct{
    char* field0,
    float field1,
    int field2
    }entry;

    this func will generate  entry[rows] data;
    */

    typedef char* ptr;
    if (strlen(format) != fieldCount)
        return nullptr;

    // 这里的 i 代表 format 字符串里面的 'd' 或者 'n' 的下标，目前看要不都在第0位要不都没有
    // n 代表的是 ID 这一列
    // d 不太确定
    //get struct size and index pos
    int32 i;
    // 此处的 recordsize 要区别于 file_load 出来的 recordSize
    uint32 recordsize = GetFormatRecordSize(format, &i);

    // records: 总行数
    // indexTable: 即  _indexTable
    // 如果fmt起始列为 i 或者 d 属性的话，则 records 另外计算
    if (i >= 0)
    {
        uint32 maxi = 0;
        //find max index
        for (uint32 y = 0; y < recordCount; ++y)
        {
            uint32 ind = getRecord(y).getUInt(i);
            if (ind > maxi)
                maxi = ind;
        }

        ++maxi;
        records = maxi;
        indexTable = new ptr[maxi];
        memset(indexTable, 0, maxi * sizeof(ptr));
    }
    else
    {
        records = recordCount;
        indexTable = new ptr[recordCount];
    }

    // 这里的 recordSize 用的是 <record_name>Enrty 的真实长度

    char* dataTable = new char[recordCount * recordsize];

    uint32 offset = 0;

    // 行
    for (uint32 y = 0; y < recordCount; ++y)
    {
        if (i >= 0)
            indexTable[getRecord(y).getUInt(i)] = &dataTable[offset];
        else
            indexTable[y] = &dataTable[offset];

        // 列
        for (uint32 x=0; x < fieldCount; ++x)
        {
            switch (format[x])
            {
                case FT_FLOAT:
                    *((float*)(&dataTable[offset])) = getRecord(y).getFloat(x);
                    offset += sizeof(float);
                    break;
                case FT_IND:
                case FT_INT:
                    *((uint32*)(&dataTable[offset])) = getRecord(y).getUInt(x);
                    offset += sizeof(uint32);
                    break;
                case FT_BYTE:
                    *((uint8*)(&dataTable[offset])) = getRecord(y).getUInt8(x);
                    offset += sizeof(uint8);
                    break;
                case FT_STRING:
                    // 为什么这里计算的 string 是 8byte 的指针，但是在 DBCFileLoader::Load 的时候则计算为 4 byte？
                    // 可以看到这里压根就没有索引
                    *((char**)(&dataTable[offset])) = nullptr;   // will replace non-empty or "" strings in AutoProduceStrings
                    offset += sizeof(char*);
                    break;
                case FT_LOGIC:
                    ABORT_MSG("Attempted to load DBC files that do not have field types that match what is in the core. Check DBCfmt.h or your DBC files.");
                    break;
                case FT_NA:
                case FT_NA_BYTE:
                case FT_SORT:
                    break;
                default:
                    ABORT_MSG("Unknown field format character in DBCfmt.h");
                    break;
            }
        }
    }

    return dataTable;
}

char* DBCFileLoader::AutoProduceStrings(char const* format, char* dataTable)
{
    if (strlen(format) != fieldCount)
        return nullptr;

    char* stringPool = new char[stringSize];
    memcpy(stringPool, stringTable, stringSize);

    uint32 offset = 0;

    // 行
    for (uint32 y = 0; y < recordCount; ++y)
    {
        // 列
        for (uint32 x = 0; x < fieldCount; ++x)
        {
            switch (format[x])
            {
                case FT_FLOAT:
                    offset += sizeof(float);
                    break;
                case FT_IND:
                case FT_INT:
                    offset += sizeof(uint32);
                    break;
                case FT_BYTE:
                    offset += sizeof(uint8);
                    break;
                case FT_STRING:
                {
                    // fill only not filled entries
                    char** slot = (char**)(&dataTable[offset]);
                    if (!*slot || !**slot)
                    {
                        const char * st = getRecord(y).getString(x);
                        *slot = stringPool + (st - (char const*)stringTable);
                    }
                    offset += sizeof(char*);
                    break;
                 }
                 case FT_LOGIC:
                     ABORT_MSG("Attempted to load DBC files that does not have field types that match what is in the core. Check DBCfmt.h or your DBC files.");
                     break;
                 case FT_NA:
                 case FT_NA_BYTE:
                 case FT_SORT:
                     break;
                 default:
                     ABORT_MSG("Unknown field format character in DBCfmt.h");
                     break;
            }
        }
    }

    return stringPool;
}
