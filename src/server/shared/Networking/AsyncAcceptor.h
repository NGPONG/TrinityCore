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

#ifndef __ASYNCACCEPT_H_
#define __ASYNCACCEPT_H_

#include "IoContext.h"
#include "IpAddress.h"
#include "Log.h"
#include <boost/asio/ip/tcp.hpp>
#include <atomic>
#include <functional>

using boost::asio::ip::tcp;

#define TRINITY_MAX_LISTEN_CONNECTIONS boost::asio::socket_base::max_listen_connections

class AsyncAcceptor
{
public:
    typedef void(*AcceptCallback)(tcp::socket&& newSocket, uint32 threadIndex);

    AsyncAcceptor(Trinity::Asio::IoContext& ioContext, std::string const& bindIp, uint16 port) :
        _acceptor(ioContext), _endpoint(Trinity::Net::make_address(bindIp), port),
        _socket(ioContext), _closed(false), _socketFactory(std::bind(&AsyncAcceptor::DefeaultSocketFactory, this))
    {
    }

    template<class T>
    void AsyncAccept();

    template<AcceptCallback acceptCallback>
    void AsyncAcceptWithCallback()
    {
        // load blance 取出一个连接数最小的 socket(io_context)
        // 这里的 socket 使用的是 NetworkThread 中的 io_context
        tcp::socket* socket;
        uint32 threadIndex;
        std::tie(socket, threadIndex) = _socketFactory();

        // NOTE:
        // 这里获取的 socket 作为 NetworkThread 中的成员，但是下文逻辑会对其进行移动，这样的做法不会有bug吗？
        // 实际测试下来所有和该 socket 有关的逻辑打印出来的地址都是同一个，上一条结论没错
        // 而且实际测试下来不会有问题，疑问？

        // 这里的 _acceptor 是 main 线程的 io_context 创建；所以这里还是主线程去执行的
        // socket 用的是开的子线程创建的 io_context，当接受成功后会把连接放到这个 socket 里面
        _acceptor.async_accept(*socket, [this, socket, threadIndex](boost::system::error_code error)
        {
            if (!error)
            {
                try
                {
                    // 设置 socket 非阻塞
                    socket->non_blocking(true);

                    acceptCallback(std::move(*socket), threadIndex);
                }
                catch (boost::system::system_error const& err)
                {
                    TC_LOG_INFO("network", "Failed to initialize client's socket {}", err.what());
                }
            }

            if (!_closed)
                this->AsyncAcceptWithCallback<acceptCallback>();
        });
    }

    bool Bind()
    {
        boost::system::error_code errorCode;
        _acceptor.open(_endpoint.protocol(), errorCode);
        if (errorCode)
        {
            TC_LOG_INFO("network", "Failed to open acceptor {}", errorCode.message());
            return false;
        }

#if TRINITY_PLATFORM != TRINITY_PLATFORM_WINDOWS
        _acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true), errorCode);
        if (errorCode)
        {
            TC_LOG_INFO("network", "Failed to set reuse_address option on acceptor {}", errorCode.message());
            return false;
        }
#endif

        _acceptor.bind(_endpoint, errorCode);
        if (errorCode)
        {
            TC_LOG_INFO("network", "Could not bind to {}:{} {}", _endpoint.address().to_string(), _endpoint.port(), errorCode.message());
            return false;
        }

        _acceptor.listen(TRINITY_MAX_LISTEN_CONNECTIONS, errorCode);
        if (errorCode)
        {
            TC_LOG_INFO("network", "Failed to start listening on {}:{} {}", _endpoint.address().to_string(), _endpoint.port(), errorCode.message());
            return false;
        }

        return true;
    }

    void Close()
    {
        if (_closed.exchange(true))
            return;

        boost::system::error_code err;
        _acceptor.close(err);
    }

    void SetSocketFactory(std::function<std::pair<tcp::socket*, uint32>()> func) { _socketFactory = func; }

private:
    std::pair<tcp::socket*, uint32> DefeaultSocketFactory() { return std::make_pair(&_socket, 0); }

    tcp::acceptor _acceptor;
    tcp::endpoint _endpoint;
    tcp::socket _socket;
    std::atomic<bool> _closed;
    std::function<std::pair<tcp::socket*, uint32>()> _socketFactory;
};

template<class T>
void AsyncAcceptor::AsyncAccept()
{
    _acceptor.async_accept(_socket, [this](boost::system::error_code error)
    {
        if (!error)
        {
            try
            {
                // this-> is required here to fix an segmentation fault in gcc 4.7.2 - reason is lambdas in a templated class
                std::make_shared<T>(std::move(this->_socket))->Start();
            }
            catch (boost::system::system_error const& err)
            {
                TC_LOG_INFO("network", "Failed to retrieve client's remote address {}", err.what());
            }
        }

        // lets slap some more this-> on this so we can fix this bug with gcc 4.7.2 throwing internals in yo face
        if (!_closed)
            this->AsyncAccept<T>();
    });
}

#endif /* __ASYNCACCEPT_H_ */
