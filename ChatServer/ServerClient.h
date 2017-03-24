#ifndef _SERVER_CLIENT_H_
#define _SERVER_CLIENT_H_

#include <memory>
#include <vector>
#include "Common.h"

class ServerClient
{
public:
    ServerClient(const ServerClient&) = delete;
    ServerClient& operator = (const ServerClient&) = delete;

    ServerClient(ServerClient&&);
    ServerClient& operator = (ServerClient&&);

    ServerClient();
    ~ServerClient();
    explicit ServerClient(SOCKET listenSock);

    bool Init(SOCKET listenSock) noexcept;
    CSOCKET* GetSocket() noexcept;
    CSOCKADDR_IN* GetAddr() noexcept;
    const std::wstring* GetName() const noexcept;
    bool SetName(std::wstring name) noexcept; // uses move copy of name
    size_t Id() const noexcept;

    bool SendData(const void* data, uint32_t size) const noexcept;
    bool RecvData(std::vector<char>& data, uint32_t* recved = nullptr) const noexcept;

    bool operator !() const;
    explicit operator bool() const { return !!*this; }
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // !_SERVER_CLIENT_H_

