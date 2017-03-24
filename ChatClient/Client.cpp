#include "Client.h"
#include "../ChatServer/Common.h"
#include "../ChatServer/ClientBase.h"
#include "../ChatServer/ClientMessage.h"
#include "../ChatServer/Console.h"
#include <thread>
#include <algorithm>
#include <sstream>
#include <atomic>

using namespace std::literals;

#define breakable_block_begin do {
#define breakable_block_end }while(0)

class Client::Impl : public ClientBase
{
public:
    Impl() : m_console(Console::GetInstance()), m_exit(false)
    {
        WSAData wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (!m_console.IsMultiThreaded())
            m_console.SetMultiThreaded(true);
    }
    ~Impl() { WSACleanup(); }

    bool Run();
private:
    bool InitClient();
    bool ReceiveThread();
    bool ClientRoutine();
    bool ParseInputLine(ClientMessage& msg, const std::wstring& str);

    std::wstring GetTimeStr(uint64_t timestmp) const
    {
        std::wstring resStr;
        tm time = { -1 };
        ::localtime_s(&time, reinterpret_cast<const time_t*>(&timestmp));
        if (time.tm_sec != -1)
        {
            resStr.resize(32);
            resStr.resize(::wcsftime(&resStr[0], 32, L"[%H:%M:%S] ", &time));
        }
        if (resStr.empty())
            resStr = L"[Error time] ";
        return resStr;
    }

    void PrintInputMessage(const ClientMessage& msg, const std::wstring& str) const;
    void PrintReceivedMessage(const ClientMessage& msg) const;
    void PrintSockError() const
    {
        PrintError(L"Error\n"s + GetErrorMsg() + L"\n"s);
    }
    void PrintError(const std::wstring& errStr) const
    {
        m_console.Write(errStr, Console::Red);
    }
private:
    std::thread m_recvThread;
    Console& m_console;
    std::atomic<bool> m_exit;
};

bool Client::Impl::Run()
{
    m_console.SetTextColor(Console::White);

    bool error = false;

    if (!InitClient())
        return false;

    breakable_block_begin;
    
    if (::connect(m_socket, m_addr, m_addr.Size()) != 0)
    {
        error = true;
        break;
    }

    m_console.SetTextColor(Console::Green);

    m_recvThread = std::thread(&Impl::ReceiveThread, this);

    if (!ClientRoutine())
        error = true;

    m_exit = true;

    breakable_block_end;

    if (error)
        PrintSockError();

    m_socket.Reset();

    if (m_recvThread.joinable())
        m_recvThread.join();

    m_console.SetTextColor(Console::White);
    m_console.Write(L"Press any key"s);
    wchar_t ch;
    m_console.ReadChar(ch);
    return true;
}

bool Client::Impl::InitClient()
{
    m_console.Write(L"Welcome to the chat\n");

    // Request client name
    do
    {
        m_console.Write(L"Enter your name: ");
        m_console.ReadLine(m_name);
        if (std::all_of(m_name.begin(), m_name.end(), ::isalnum))
            break;
        PrintError(L"Invalid name. Name can consist only of letters and nubers\n");
    } while (1);

    // Request server address
    std::string tmpStr;
    do
    {
        m_console.Write(L"Enter server ip address: ");
        if (!m_console.ReadLine(tmpStr))
            return false;

        m_addr.sin_addr.s_addr = ::inet_addr(tmpStr.c_str());
        if (m_addr.Addr().Addr() != INADDR_NONE)
            break;

        PrintError(L"Incorrect address\n");
    } while (1);

    // Request server port
    do
    {
        m_console.Write(L"Enter server port: ");
        if (!m_console.ReadLine(tmpStr))
            return false;

        try
        {
            m_addr.sin_port = ::htons(static_cast<uint16_t>(std::stol(tmpStr)));
            break;
        }
        catch (std::exception&)
        {
            PrintError(L"Invalid port\n");
        }
    } while (1);

    m_addr.sin_family = AF_INET;

    if (!m_socket.Init(PF_INET, SOCK_STREAM, IPPROTO_TCP))
    {
        PrintSockError();
        return false;
    }
    return true;
}

bool Client::Impl::ReceiveThread()
{
    ClientMessage msg;
    std::vector<char> data;
    uint32_t recved;
    bool error = false;

    while (!m_exit)
    {
        if (!RecvData(data, &recved))
        {
            auto err = WSAGetLastError();
            if(err == WSAECONNRESET)
                m_console.Write(L"Server shutdown\n", Console::White);
            else if(err != WSAECONNABORTED)
                error = true;              
            break;
        }
        if (recved == 0)
        {
            m_console.Write(L"You was disconnected\n", Console::White);
            break;
        }
        msg.Unserialize(data.data(), recved);

        if (msg.command == ClientCommand::ServerMsg && msg.msg.compare(0, 22, L"ErrorNameAlreadyExists") == 0)
        {
            std::wistringstream iss(msg.msg);
            std::wstring tmpStr;
            iss >> tmpStr >> tmpStr;
            msg.msg = L"User with name '"s + tmpStr + L"' already exists";
            iss >> tmpStr;
            m_name = std::move(tmpStr);
            if (m_name.empty())
                error = true;
        }
        PrintReceivedMessage(msg);     
    }

    if (error)
        PrintSockError();

    m_exit = true;

    return !error;
}
bool Client::Impl::ClientRoutine()
{
    std::wstring inpStr;
    ClientMessage msg;
    uint32_t dataSize = 0;
    ClientMessage::Data data;

    // Send to server connection request
    msg.from = m_name;
    msg.command = ClientCommand::ClientConnect;
    msg.timeStamp = ::time(nullptr);
    data = msg.Serialize(&dataSize);
    if (!data || !SendData(data.get(), dataSize))
        return false;

    while (!m_exit)
    {
        m_console.ReadLine(inpStr);
        if (m_exit)
            break;
        if (inpStr == L"/exit")
        {
            m_exit = true;
            break;
        }
        if (!ParseInputLine(msg, inpStr))
            continue;
        PrintInputMessage(msg, inpStr);
        if (msg.command == ClientCommand::Help)
            continue;
        msg.from = m_name;
        data = msg.Serialize(&dataSize);
        if (!data)
            PrintError(L"Serialization failed\n");
        else if (!SendData(data.get(), dataSize))
        {
            PrintError(L"Message was not sended\n");
            PrintSockError();
            return false;
        }
        if (msg.command == ClientCommand::ChangeName)
        {
            m_name = msg.msg;
            msg.from = std::move(msg.msg);
        }
    };
    return true;
}
bool Client::Impl::ParseInputLine(ClientMessage& msg, const std::wstring& str)
{
    if (str.empty())
        return false;

    msg.pmTo.clear();
    msg.msg.clear();

    std::wistringstream iss(str);

    msg.command = ClientCommand::BroadcastMessage;

    if (str[0] == L'/')
    {
        std::wstring cmdStr;
        iss >> cmdStr;
        msg.command = ClientMessage::GetCommandId(cmdStr);
        if (msg.command == ClientCommand::Error)
        {
            PrintError(L"Invalid command "s + cmdStr + L"\n"s);
            return false;
        }
    }

    msg.timeStamp = ::time(nullptr);

    if (msg.command == ClientCommand::Help)
    {
        msg.msg = L"Available commands:\n"
            L"/pm (user name)- private message\n"
            L"/setname (new name) - change name\n"
            L"/listusers - show current active users\n"
            L"/exit - exit program";
        return true;
    }

    if (msg.command == ClientCommand::PrivateMessage)
    {
        iss >> msg.pmTo;
        if (msg.pmTo.empty())
        {
            PrintError(L"No client name was specified for private message\n");
            return false;
        }
    }

    if (msg.command == ClientCommand::ListClients)
        return true;

    if (msg.command == ClientCommand::ChangeName)
    {
        iss >> msg.msg;
        if (msg.msg.empty())
        {
            PrintError(L"Can't change name - no name specified\n");
            return false;
        }
    }
    else
    {
        if(msg.command != ClientCommand::BroadcastMessage)
            iss.ignore(); //skip space
        std::getline(iss, msg.msg);
        if (msg.msg.empty())
            return false;
    }
    return true;
}

void Client::Impl::PrintInputMessage(const ClientMessage & msg, const std::wstring& inpStr) const
{
    std::wstring str = GetTimeStr(msg.timeStamp);
    Console::Color color = Console::Green;

    if (msg.command == ClientCommand::PrivateMessage)
    {
        str += L"You to "s + msg.pmTo + L": "s + msg.msg + L'\n';
        color = Console::Magenta;
    }
    else if (msg.command == ClientCommand::BroadcastMessage)
    {
        str += L"You: "s + msg.msg + L'\n';
    }
    else if (msg.command == ClientCommand::Help)
    {
        color = Console::Cyan;
        str = msg.msg + L'\n';
    }
    else
        str.clear();

    m_console.LockWrite();

    uint32_t conSize = m_console.GetConsoleSize();
    uint16_t width = conSize & 0xFFFF;
    uint32_t nErase = ((inpStr.size() + width - 1) / width) * width;
    m_console.EraseChars(nErase);

    if (!str.empty())
        m_console.Write(str, color);

    m_console.UnlockWrite();
}

void Client::Impl::PrintReceivedMessage(const ClientMessage & msg) const
{
    if (msg.command == ClientCommand::Error)
        return;

    std::wstring resStr;

    resStr = GetTimeStr(msg.timeStamp);

    Console::Color color = Console::White;
    switch (msg.command)
    {
        case ClientCommand::ServerMsg:
            color = Console::Cyan;
            resStr += msg.from + L": "s + msg.msg;
            break;
        case ClientCommand::BroadcastMessage:
            color = Console::Yellow;
            resStr += msg.from + L": "s + msg.msg;
            break;
        case ClientCommand::PrivateMessage:
            color = Console::Magenta;
            resStr += L"From "s + msg.from + L": "s + msg.msg;
            break;
        default:
            resStr.clear();
            break;
    }

    if (!resStr.empty())
    {
        resStr += L'\n';
        m_console.Write(resStr, color);
    }
}


//------------------------------------------------------------------------------

Client::Client(Client&&) = default;
Client& Client::operator = (Client&&) = default;

Client::Client() : m_impl(new Impl) {}
Client::~Client() {}

bool Client::Run()
{
    return m_impl->Run();
}