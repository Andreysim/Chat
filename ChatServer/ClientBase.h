#ifndef _CLIENT_BASE_H_
#define _CLIENT_BASE_H_

#include "Common.h"
#include <vector>

class ClientBase
{
public:
    ClientBase(const ClientBase&) = delete;
    ClientBase& operator = (const ClientBase&) = delete;

    ClientBase(ClientBase&&) = default;
    ClientBase& operator = (ClientBase&&) = default;

    ClientBase() noexcept {};
    virtual ~ClientBase() noexcept {};

    const std::wstring* GetName() const noexcept
    {
        return &m_name;
    }
    void SetName(std::wstring name) noexcept
    {
        m_name = std::move(name);
    }

    CSOCKET* GetSocket() noexcept
    {
        return &m_socket;
    }
    CSOCKADDR_IN* GetAddr() noexcept
    {
        return &m_addr;
    }

    bool SendData(const void* data, uint32_t size) const noexcept;
    bool RecvData(std::vector<char>& data, uint32_t* recved) const noexcept;

    explicit operator bool() const noexcept { return !!*this; }
    bool operator !() const noexcept { return !m_socket; }

protected:
    CSOCKET m_socket;
    CSOCKADDR_IN m_addr;
    std::wstring m_name;
};

#endif // !_CLIENT_BASE_H_

