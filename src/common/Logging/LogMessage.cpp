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

#include "LogMessage.h"
#include "StringFormat.h"
#include "Util.h"

LogMessage::LogMessage(LogLevel _level, std::string_view _type, std::string _text)
    : level(_level), type(_type), text(std::move(_text)), mtime(time(nullptr)), mtimems(std::chrono::system_clock::now())
{
}

LogMessage::LogMessage(LogLevel _level, std::string_view _type, std::string _text, std::string _param1)
    : level(_level), type(_type), text(std::move(_text)), param1(std::move(_param1)), mtime(time(nullptr)), mtimems(std::chrono::system_clock::now())
{
}

std::string LogMessage::getTimeStr(time_t time)
{
    tm aTm;
    localtime_r(&time, &aTm);
    return Trinity::StringFormat("{:04}-{:02}-{:02}_{:02}:{:02}:{:02}", aTm.tm_year + 1900, aTm.tm_mon + 1, aTm.tm_mday, aTm.tm_hour, aTm.tm_min, aTm.tm_sec);
}

std::string LogMessage::getTimeStr() const
{
    return getTimeStr(mtime);
}

std::string LogMessage::getTimeStrMs() const
{
    // NOTE: 改用毫秒级时间
    char buffer[80];

    auto transformed = mtimems.time_since_epoch().count() / 1000000;

    auto millis = transformed % 1000;

    std::time_t tt;
    tt = std::chrono::system_clock::system_clock::to_time_t (mtimems);
    auto timeinfo = localtime (&tt);
    strftime (buffer,80,"%F %H:%M:%S",timeinfo);
    sprintf(buffer, "%s:%03d",buffer,(int)millis);

    return std::string(buffer);
}
