#include "ServerClient.h"
#include "ClientBase.h"
#include <algorithm>


class ServerClient::Impl : public ClientBase
{
public:
    Impl(const Impl&) = delete;
    Impl& operator = (const Impl&) = delete;

    Impl(Impl&&) = default;
    Impl& operator = (Impl&&) = default;

    Impl() noexcept : m_id(idCounter++) {}
    Impl(SOCKET listenSock) noexcept : Impl()
    {
        Init(listenSock);
    }
    virtual ~Impl() noexcept {}

    bool Init(SOCKET listenSock) noexcept
    {
        int len = m_addr.Size();
        m_socket.Reset(::accept(listenSock, m_addr, &len));
        return !!*this;
    }

    size_t Id() const
    {
        return m_id;
    }

    explicit operator bool() const noexcept { return !!*this; }
    bool operator !() const noexcept { return !m_socket; }
protected:
    static size_t idCounter;
    size_t m_id;
};

size_t ServerClient::Impl::idCounter = 0;


//-------------------------------------------------------------------------------------------

ServerClient::ServerClient(ServerClient &&) = default;
ServerClient & ServerClient::operator=(ServerClient &&) = default;

ServerClient::ServerClient() : m_impl(new Impl) {}
ServerClient::ServerClient(SOCKET listenSock) : m_impl(new Impl(listenSock)) {}
ServerClient::~ServerClient() = default;

bool ServerClient::Init(SOCKET listenSock) noexcept
{
    return m_impl->Init(listenSock);
}
CSOCKET* ServerClient::GetSocket() noexcept
{
    return m_impl->GetSocket();
}
CSOCKADDR_IN* ServerClient::GetAddr() noexcept
{
    return m_impl->GetAddr();
}
const std::wstring* ServerClient::GetName() const noexcept
{
    return m_impl->GetName();
}
bool ServerClient::SetName(const std::wstring name) noexcept
{
    m_impl->SetName(std::move(name));
    return true;
}
size_t ServerClient::Id() const noexcept
{
    return m_impl->Id();
}

bool ServerClient::SendData(const void* data, uint32_t size) const noexcept
{
    return m_impl->SendData(data, size);
}
bool ServerClient::RecvData(std::vector<char>& data, uint32_t* recved) const noexcept
{
    return m_impl->RecvData(data, recved);
}

bool ServerClient::operator !() const
{
    return !*m_impl;
}
