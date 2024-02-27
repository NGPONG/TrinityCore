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

#ifndef SocketMgr_h__
#define SocketMgr_h__

#include "AsyncAcceptor.h"
#include "Errors.h"
#include "NetworkThread.h"
#include <boost/asio/ip/tcp.hpp>
#include <memory>

using boost::asio::ip::tcp;

template<class SocketType>
class SocketMgr
{
public:
    virtual ~SocketMgr()
    {
        ASSERT(!_threads && !_acceptor && !_threadCount, "StopNetwork must be called prior to SocketMgr destruction");
    }

    virtual bool StartNetwork(Trinity::Asio::IoContext& ioContext, std::string const& bindIp, uint16 port, int threadCount)
    {
        // authserver threadCount = 1
        ASSERT(threadCount > 0);

        // 主线程创建 AsyncAcceptor
        AsyncAcceptor* acceptor = nullptr;
        try
        {
            acceptor = new AsyncAcceptor(ioContext, bindIp, port);
        }
        catch (boost::system::system_error const& err)
        {
            TC_LOG_ERROR("network", "Exception caught in SocketMgr.StartNetwork ({}:{}): {}", bindIp, port, err.what());
            return false;
        }

        // acceptor 开始设置监听，直到 main_threads io_context 调用 run 后才开始工作
        if (!acceptor->Bind())
        {
            TC_LOG_ERROR("network", "StartNetwork failed to bind socket acceptor");
            delete acceptor;
            return false;
        }

        _acceptor = acceptor;
        _threadCount = threadCount;
        // CreaetThreads 由派生类实现，如 AuthSocketMgr 和 WorldSocketMgr
        // 返回 N 个 NetworkThread<<Name>Session> 的数组指针
        // 每一个独立的 NetworkThread 内都有一个独立的 io_context
        _threads = CreateThreads();

        ASSERT(_threads);

        // 每一项开启一个线程绑定执行对应的 io_context
        for (int32 i = 0; i < _threadCount; ++i)
            _threads[i].Start();

        // 初始化 AsyncAcceptor 的 _socketFactory 成员
        // 每一个 NetworkThread 都暴露其 _accept_socket 以提供给建立链接使用
        _acceptor->SetSocketFactory([this]() { return GetSocketForAccept(); });

        return true;
    }

    virtual void StopNetwork()
    {
        _acceptor->Close();

        if (_threadCount != 0)
            for (int32 i = 0; i < _threadCount; ++i)
                _threads[i].Stop();

        Wait();

        delete _acceptor;
        _acceptor = nullptr;
        delete[] _threads;
        _threads = nullptr;
        _threadCount = 0;
    }

    void Wait()
    {
        if (_threadCount != 0)
            for (int32 i = 0; i < _threadCount; ++i)
                _threads[i].Wait();
    }

    virtual void OnSocketOpen(tcp::socket&& sock, uint32 threadIndex)
    {
        try
        {
            // socket -> SocketType
            // SocketType 继承自 tcp::socket
            // SocketType 在项目中呈 <name>Session 的类
            std::shared_ptr<SocketType> newSocket = std::make_shared<SocketType>(std::move(sock));

            // start 发起一个异步查询，查询当前连接进来的远端IP是否在 ip_banned 列表当中，如果是的话则给客户端
            // 返回错误消息，告知禁止登录
            newSocket->Start();

            // 个体 NetworkThread 添加一个建立的 socket 连接让它去处理
            _threads[threadIndex].AddSocket(newSocket);
        }
        catch (boost::system::system_error const& err)
        {
            TC_LOG_WARN("network", "Failed to retrieve client's remote address {}", err.what());
        }
    }

    int32 GetNetworkThreadCount() const { return _threadCount; }

    uint32 SelectThreadWithMinConnections() const
    {
        uint32 min = 0;

        for (int32 i = 1; i < _threadCount; ++i)
            if (_threads[i].GetConnectionCount() < _threads[min].GetConnectionCount())
                min = i;

        return min;
    }

    std::pair<tcp::socket*, uint32> GetSocketForAccept()
    {
        // 获取 _threads 中连接数最小的一个(负载最小的)，简易版的 load balance
        uint32 threadIndex = SelectThreadWithMinConnections();

        // 获取其用于 accept connection 的 io_context
        return std::make_pair(_threads[threadIndex].GetSocketForAccept(), threadIndex);
    }

protected:
    SocketMgr() : _acceptor(nullptr), _threads(nullptr), _threadCount(0)
    {
    }

    virtual NetworkThread<SocketType>* CreateThreads() const = 0;

    AsyncAcceptor* _acceptor;
    NetworkThread<SocketType>* _threads;
    int32 _threadCount;
};

#endif // SocketMgr_h__
