#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>
#include <algorithm>
#include <chrono>

#include "Server.h"
#include "ServerClient.h"
#include "ClientMessage.h"
#include "RWAccessManager.h"
#include "Console.h"

using namespace std::literals;


struct ClientThread
{
    std::atomic<bool> completed = true;
    std::thread clientThread;
    ServerClient client;
};

typedef std::unique_ptr<ClientThread> ClientThreadUPtr;
typedef std::unique_lock<std::mutex> MutexLock;


class Server::Impl
{
public:
    Impl(USHORT port) 
        : m_pair(std::_Zero_then_variadic_args_t()),
        m_exit(false),
        m_port(port),
        m_clientThreadsAccessManager(new RWAccessManager),
        m_console(Console::GetInstance())
    {
        if(!m_console.IsMultiThreaded())
            m_console.SetMultiThreaded(true);
    }
    ~Impl()
    {
        if (m_consoleInputThread.joinable())
            m_consoleInputThread.detach();
    }
    bool Run();

private:
    void Input();
    bool StartListen() noexcept;
    bool AcceptClient();
    void ClientFunction(size_t ind);
    void AddClient(ClientThreadUPtr clThr);

    bool ReceiveData(ServerClient& client, std::vector<char>& data);

    bool ProcessClientConnect(ServerClient* client);
    bool ProcessReceivedClientData(ClientMessage & msg, ServerClient * client);
    bool ProcessBroadcastSend(ClientMessage& msg, ServerClient* client = nullptr); // send to all clients except specified client if not nullptr
    bool ProcessPrivateSend(ClientMessage& msg, ServerClient* from);
    bool ProcessNameChange(ClientMessage& msg, ServerClient * client);
    bool ProcessClientsListRequest(ClientMessage & msg, ServerClient * client);
    bool ProcessNameAlreadyExists(ClientMessage & msg, ServerClient * client);

    bool IsClientNameExists(const std::wstring& name) const
    {
        RWLocker lk(*m_clientThreadsAccessManager);
        return std::find_if(m_clientThreads.begin(), m_clientThreads.end(),
            [&name](const ClientThreadUPtr& thr)
        {
            return !thr->completed && *thr->client.GetName() == name;
        }) != m_clientThreads.end();
    }
    void PrintClientError(const ServerClient& client, std::wstring prefix = L"") const
    {
        m_console << prefix <<
            L"\nClient " << (client.GetName() ? *client.GetName() : L"Anon"s) << L' ' << client.Id() << L" error.\n" <<
            GetErrorMsg() << L"\n";
    }
    void MakeServerMessage(ClientMessage& msg, std::wstring& str)
    {
        msg.command = ClientCommand::ServerMsg;
        msg.from = L"Server"s;
        msg.pmTo.clear();
        msg.msg = std::move(str);
        msg.timeStamp = time(nullptr);
    }

private:
    std::_Compressed_pair<WSAInit, CSOCKET> m_pair;
    std::atomic_bool m_exit;
    std::thread m_consoleInputThread;
    std::vector<std::wstring> m_sendMsgs;
    std::vector<ClientThreadUPtr> m_clientThreads;
    std::unique_ptr<RWAccessManager> m_clientThreadsAccessManager;
    Console& m_console;
    USHORT m_port;
};

bool Server::Impl::Run()
{
    if (!StartListen())
        return false;

    m_consoleInputThread = std::thread(&Impl::Input, this);

    m_exit = false;
    bool error = false;
    timeval waitTime = { 0, 0 };
    fd_set fd;

    while (!m_exit)
    {
        FD_ZERO(&fd);
        FD_SET(m_pair._Get_second().Get(), &fd);
        int selectRes = ::select(1, &fd, nullptr, nullptr, &waitTime);
        if (selectRes == 0)
        {
            std::this_thread::sleep_for(100ms);
        }
        else if (selectRes < 0)
        {
            m_exit = true;
            error = true;
            break;
        }
        else if (!AcceptClient())
        {
            m_console << L"Client accept error.\n" << GetErrorMsg() << L"\n";
        }
    }

    if (error)
        m_console.Write(L"Server shutdown, enter to continue\n");

    if (m_consoleInputThread.joinable())
        m_consoleInputThread.join();

    RWLocker lk(*m_clientThreadsAccessManager, true);
    // close client sokets
    for (auto& thr : m_clientThreads)
    {
        if (thr->client.GetSocket())
            thr->client.GetSocket()->Reset();
        if (thr->clientThread.joinable())
            thr->clientThread.join();
    }
    
    m_console.Write(L"Press any key\n"s);
    wchar_t ch;
    m_console.ReadChar(ch);
    return !error;
}

void Server::Impl::Input()
{
    std::wstring inp;
    while (!m_exit)
    {
        m_console.ReadLine(inp);
        if (inp == L"exit")
        {
            m_exit = true;
            return;
        }
    }
}

inline bool Server::Impl::StartListen() noexcept
{
    CSOCKADDR_IN saddr(AF_INET, htons(m_port), ADDR_ANY);
    if (!m_pair._Get_second().Init(PF_INET, SOCK_STREAM, IPPROTO_TCP))
        return false;

    if (::bind(m_pair._Get_second(), saddr, saddr.Size()) != 0)
        return false;

    if (::listen(m_pair._Get_second(), SOMAXCONN) != 0)
        return false;
    return true;
}
bool Server::Impl::AcceptClient()
{
    ClientThreadUPtr clThr(new (std::nothrow) ClientThread);
    if (!clThr)
    {
        ::closesocket(::accept(m_pair._Get_second(), nullptr, nullptr));
        WSASetLastError(ERROR_OUTOFMEMORY);
        return false;
    }
    clThr->client.Init(m_pair._Get_second());
    if (!clThr->client)
        return false;

    clThr->completed = false;

    if (ProcessClientConnect(&clThr->client))
        AddClient(std::move(clThr));
    return true;
}
void Server::Impl::AddClient(ClientThreadUPtr clThr)
{
    RWLocker rwlk(*m_clientThreadsAccessManager, true);

    auto ind = std::find_if(m_clientThreads.begin(), m_clientThreads.end(),
        [](const ClientThreadUPtr& cl) { return cl->completed.load(); })
        - m_clientThreads.begin();

    if (ind == m_clientThreads.size())
        m_clientThreads.resize(ind + 1);
    else if (m_clientThreads[ind]->clientThread.joinable())
        m_clientThreads[ind]->clientThread.join();

    clThr->clientThread = std::thread(&Impl::ClientFunction, this, ind);

    m_clientThreads[ind] = std::move(clThr);
}

void Server::Impl::ClientFunction(size_t ind)
{
    RWLocker rwlk(*m_clientThreadsAccessManager);
    ServerClient& client = m_clientThreads[ind]->client;
    rwlk.Unlock();

    bool error = false;
    std::vector<char> data;
    ClientMessage clMsg;

    while (!m_exit && !error)
    {
        if (ReceiveData(client, data))  // Receive data
        {
            if (data.size() == 0) // Client disconnected
                break;
            clMsg.Unserialize(data.data(), data.size());

            if (!ProcessReceivedClientData(clMsg, &client))
                error = true;
        }
        else
        {
            if(WSAGetLastError() != WSAECONNRESET)
                error = true;
            break;
        }
    }
    if (error)
        PrintClientError(client, L"Terminating client thread");

    MakeServerMessage(clMsg, *client.GetName() + L" leaves the chat."s);
    ProcessBroadcastSend(clMsg, &client);
    
    client.GetSocket()->Reset();

    rwlk.Lock(true);
    m_clientThreads[ind]->completed = true;
}
bool Server::Impl::ReceiveData(ServerClient& client, std::vector<char>& data)
{
    uint32_t received = 0;
    if (!client.RecvData(data, &received))
        return false;
    else if (received == 0)
        return true;

    m_console << L"Client " << *client.GetName() << L" " << client.Id() <<
        L" recieved " << received << L" of " << data.size() << L"\n";

    data.resize(received);
    return true;
}


bool Server::Impl::ProcessClientConnect(ServerClient* client)
{
    ClientMessage msg;
    std::vector<char> rcvData;
    if (!ReceiveData(*client, rcvData) || rcvData.size() == 0)
    {
        PrintClientError(*client);
        return false;
    }
    msg.Unserialize(rcvData.data(), rcvData.size());
    if (msg.command != ClientCommand::ClientConnect)
        return false;
    client->SetName(msg.from);

    if (!IsClientNameExists(*client->GetName()))
    {
        MakeServerMessage(msg, *client->GetName() + L" joined to the chat."s);
        return (ProcessBroadcastSend(msg, client) && ProcessClientsListRequest(msg, client));
    }
    else
    {
        msg.msg = msg.from;
        ProcessNameAlreadyExists(msg, client);
        return false;
    }
}
bool Server::Impl::ProcessReceivedClientData(ClientMessage& msg, ServerClient* client)
{
    // Process received data
    switch (msg.command)
    {
        case ClientCommand::BroadcastMessage:
            return ProcessBroadcastSend(msg, client);
        case ClientCommand::PrivateMessage:
            return ProcessPrivateSend(msg, client);
        case ClientCommand::ChangeName:
            return ProcessNameChange(msg, client);
        case ClientCommand::ListClients:
            return ProcessClientsListRequest(msg, client);
        case ClientCommand::Error:
        default:
            return false;
    }
}
bool Server::Impl::ProcessBroadcastSend(ClientMessage& msg, ServerClient* client)
{
    uint32_t size = 0;
    auto data = msg.Serialize(&size);
    if (!data)
        return false;

    RWLocker rwlk(*m_clientThreadsAccessManager);
    for (const auto& cl : m_clientThreads)
    {
        if (&cl->client != client && !cl->completed)
        {
            if (!cl->client.SendData(data.get(), size))
                PrintClientError(cl->client, L"Sending data error\n"s);
        }
    }
    return true;
}
bool Server::Impl::ProcessPrivateSend(ClientMessage& msg, ServerClient* receivedFrom)
{
    uint32_t size = 0;
    auto data= msg.Serialize(&size);
    if (!data)
        return false;
    
    RWLocker rwlk(*m_clientThreadsAccessManager);
    auto clIt = std::find_if(m_clientThreads.begin(), m_clientThreads.end(),
        [name = &msg.pmTo](const ClientThreadUPtr& cl) 
        {
            return *cl->client.GetName() == *name && !cl->completed; 
        });
    if (clIt == m_clientThreads.end())
    {
        MakeServerMessage(msg, L"There is no user with name "s + msg.pmTo);
        data = msg.Serialize(&size);
        if (!data || !receivedFrom->SendData(data.get(), size))
            return false;
    }
    else if (!(*clIt)->client.SendData(data.get(), size))
        return false;
    
    return true;
}
bool Server::Impl::ProcessNameChange(ClientMessage& msg, ServerClient* client)
{
    if (!IsClientNameExists(msg.msg))
    {
        std::wstring oldName = *client->GetName();
        client->SetName(std::move(msg.msg));
        MakeServerMessage(msg, oldName + L" changed his name to "s + *client->GetName());
        return ProcessBroadcastSend(msg);
    }
    else
        return ProcessNameAlreadyExists(msg, client);
}
bool Server::Impl::ProcessClientsListRequest(ClientMessage& msg, ServerClient* client)
{
    std::wstring list;
    RWLocker rwlk(*m_clientThreadsAccessManager);
    for (const auto& cl : m_clientThreads)
    {
        if (!cl->completed)
        {
            list += *cl->client.GetName();
            list += L'\n';
        }
    }
    rwlk.Unlock();

    if (list.empty())
        list = L"there are no active users";
    else
        list.pop_back();

    MakeServerMessage(msg, L"Current active users:\n"s + list);

    uint32_t size = 0;
    auto data = msg.Serialize(&size);
    if (!data)
        return false;

    return client->SendData(data.get(), size);
}
bool Server::Impl::ProcessNameAlreadyExists(ClientMessage& msg, ServerClient * client)
{
    MakeServerMessage(msg, L"ErrorNameAlreadyExists "s + msg.msg + L' ' + *client->GetName());
    uint32_t size;
    auto data = msg.Serialize(&size);
    if (!data)
        return false;
    return client->SendData(data.get(), size);
}



//------------------------------------------------------------------------------

Server::Server(Server&&) = default;
Server& Server::operator = (Server&&) = default;

Server::Server(uint16_t port) : m_impl(new Impl(port)) {}
Server::~Server() {}
bool Server::Run()
{
    return m_impl->Run();
}
