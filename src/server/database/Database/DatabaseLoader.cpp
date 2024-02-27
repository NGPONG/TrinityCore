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

#include "DatabaseLoader.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBUpdater.h"
#include "Log.h"

#include <mysqld_error.h>

DatabaseLoader::DatabaseLoader(std::string const& logger, uint32 const defaultUpdateMask)
    : _logger(logger), _autoSetup(sConfigMgr->GetBoolDefault("Updates.AutoSetup", true)),
    _updateFlags(sConfigMgr->GetIntDefault("Updates.EnableDatabases", defaultUpdateMask))
{
}

template <class T>
DatabaseLoader& DatabaseLoader::AddDatabase(DatabaseWorkerPool<T>& pool, std::string const& name)
{
    bool const updatesEnabledForThis = DBUpdater<T>::IsEnabled(_updateFlags);

    // 添加开启操作
    _open.push([this, name, updatesEnabledForThis, &pool]() -> bool
    {
        std::string const dbString = sConfigMgr->GetStringDefault(name + "DatabaseInfo", "");
        if (dbString.empty())
        {
            TC_LOG_ERROR(_logger, "Database {} not specified in configuration file!", name);
            return false;
        }

        // mysql 线程数，有多少个即创建多少个 MySQLConnection -> DatabaseWorker
        uint8 const asyncThreads = uint8(sConfigMgr->GetIntDefault(name + "Database.WorkerThreads", 1));
        if (asyncThreads < 1 || asyncThreads > 32)
        {
            TC_LOG_ERROR(_logger, "{} database: invalid number of worker threads specified. "
                "Please pick a value between 1 and 32.", name);
            return false;
        }

        // SyncThread 代表创建 N 个MYSQL连接，每个连接执行都会阻塞，由于网络模块本身是多线程的，当一个连接正在执行的时候就会换另一个连接
        // AsyncThread 代表创建 N 个MYSQL连接，每个连接又创建一个线程，所以每个连接在执行的时候是不会阻塞的

        uint8 const synchThreads = uint8(sConfigMgr->GetIntDefault(name + "Database.SynchThreads", 1));

        pool.SetConnectionInfo(dbString, asyncThreads, synchThreads);
        // 每个 DatabaseWorkrPool 创建X个 SYNC connection 和X个 ASYNC connection
        if (uint32 error = pool.Open())
        {
            // Database does not exist
            if ((error == ER_BAD_DB_ERROR) && updatesEnabledForThis && _autoSetup)
            {
                // 开启一个子进程(boost::process::child)去执行一段 sql command
                // Try to create the database and connect again if auto setup is enabled
                if (DBUpdater<T>::Create(pool) && (!pool.Open()))
                    error = 0;
            }

            // If the error wasn't handled quit
            if (error)
            {
                TC_LOG_ERROR("sql.driver", "\nDatabasePool {} NOT opened. There were errors opening the MySQL connections. Check your SQLDriverLogFile "
                    "for specific errors. Read wiki at http://www.trinitycore.info/display/tc/TrinityCore+Home", name);

                return false;
            }
        }
        // 添加关闭操作
        // Add the close operation
        _close.push([&pool]
        {
            pool.Close();
        });
        return true;
    });

    // authserver 不会执行任何操作，仅 worldserver 开启的时候才会填充数据
    // Populate and update only if updates are enabled for this pool
    if (updatesEnabledForThis)
    {
        // 使用 sql 填充数据库
        // 对于 auth 数据库使用 sql/base/auth_database.sql 填充
        // 对于 characters 数据库使用 sql/base/characters_database.sql 填充
        _populate.push([this, name, &pool]() -> bool
        {
            if (!DBUpdater<T>::Populate(pool))
            {
                TC_LOG_ERROR(_logger, "Could not populate the {} database, see log for details.", name);
                return false;
            }
            return true;
        });

        // 处理源sql文件(sql/*.sql)产生变更自动更新的情况
        _update.push([this, name, &pool]() -> bool
        {
            if (!DBUpdater<T>::Update(pool))
            {
                TC_LOG_ERROR(_logger, "Could not update the {} database, see log for details.", name);
                return false;
            }
            return true;
        });
    }

    _prepare.push([this, name, &pool]() -> bool
    {
        // DatabaseLoader ->
        // DatabaseWorkerPool<MySqlConnection<T>>::PrepareStatements ->
        // MySqlConnection<T>::PrepareStatements ->
        // T::DoPrepareStatements
        if (!pool.PrepareStatements())
        {
            TC_LOG_ERROR(_logger, "Could not prepare statements of the {} database, see log for details.", name);
            return false;
        }
        return true;
    });

    return *this;
}

bool DatabaseLoader::Load()
{
    if (!_updateFlags)
        TC_LOG_INFO("sql.updates", "Automatic database updates are disabled for all databases!");

    if (!OpenDatabases())
        return false;

    if (!PopulateDatabases())
        return false;

    if (!UpdateDatabases())
        return false;

    if (!PrepareStatements())
        return false;

    return true;
}

bool DatabaseLoader::OpenDatabases()
{
    return Process(_open);
}

bool DatabaseLoader::PopulateDatabases()
{
    return Process(_populate);
}

bool DatabaseLoader::UpdateDatabases()
{
    return Process(_update);
}

bool DatabaseLoader::PrepareStatements()
{
    return Process(_prepare);
}

bool DatabaseLoader::Process(std::queue<Predicate>& queue)
{
    while (!queue.empty())
    {
        if (!queue.front()())
        {
            // Close all open databases which have a registered close operation
            while (!_close.empty())
            {
                _close.top()();
                _close.pop();
            }

            return false;
        }

        queue.pop();
    }
    return true;
}

template TC_DATABASE_API
DatabaseLoader& DatabaseLoader::AddDatabase<LoginDatabaseConnection>(DatabaseWorkerPool<LoginDatabaseConnection>&, std::string const&);
template TC_DATABASE_API
DatabaseLoader& DatabaseLoader::AddDatabase<CharacterDatabaseConnection>(DatabaseWorkerPool<CharacterDatabaseConnection>&, std::string const&);
template TC_DATABASE_API
DatabaseLoader& DatabaseLoader::AddDatabase<WorldDatabaseConnection>(DatabaseWorkerPool<WorldDatabaseConnection>&, std::string const&);
