#ifndef _COMMON_H_
#define _COMMON_H_

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#include <string>

#pragma comment(lib, "Ws2_32.lib")


constexpr uint32_t MAX_SEND_RECV_DATA_SIZE = 1024;

struct WSAInit
{
    WSAInit()
    {
        WSAData data;
        WSAStartup(MAKEWORD(2, 2), &data);
    }
    ~WSAInit()
    {
        WSACleanup();
    }
};

struct CIN_ADDR : in_addr
{
    constexpr CIN_ADDR() : in_addr{} {}
    constexpr CIN_ADDR(in_addr addr) : in_addr(addr) {}
    constexpr CIN_ADDR(ULONG addr) : in_addr{} { this->s_addr = addr; }
    constexpr CIN_ADDR(USHORT w1, USHORT w2) : in_addr{} { this->S_un.S_un_w = { w1, w2 }; }
    constexpr CIN_ADDR(UCHAR b1, UCHAR b2, UCHAR b3, UCHAR b4) : in_addr{ b1, b2, b3, b4 } {}

    constexpr ULONG Addr() const { return this->s_addr; }
    constexpr USHORT Imp() const { return this->s_imp; }
    constexpr UCHAR Network() const { return this->s_net; }
    constexpr UCHAR HostOnImp() const { return this->s_host; }
    constexpr UCHAR LogicalHost() const { return this->s_lh; }
    constexpr UCHAR ImpNo() const { return this->s_impno; }

    operator in_addr () const { return *this; }
};

struct CSOCKADDR_IN : SOCKADDR_IN
{
    constexpr CSOCKADDR_IN() : SOCKADDR_IN{} {}
    constexpr CSOCKADDR_IN(ADDRESS_FAMILY af, USHORT port, IN_ADDR addr)
        : SOCKADDR_IN{af, port, addr}  { }
    constexpr CSOCKADDR_IN(ADDRESS_FAMILY af, USHORT port, ULONG addr)
        : SOCKADDR_IN{ af, port, CIN_ADDR(addr) } { }

    constexpr CSOCKADDR_IN(ADDRESS_FAMILY af, USHORT port, USHORT w1, USHORT w2)
        : SOCKADDR_IN{ af, port, CIN_ADDR(w1, w2) } {}
    constexpr CSOCKADDR_IN(ADDRESS_FAMILY af, USHORT port, UCHAR b1, UCHAR b2, UCHAR b3, UCHAR b4)
        : SOCKADDR_IN{ af, port, CIN_ADDR(b1, b2, b3, b4) } {}

    ADDRESS_FAMILY Family() const { return this->sin_family; }
    USHORT Port() const { return this->sin_port; }
    CIN_ADDR Addr() const { return this->sin_addr; }

    operator SOCKADDR* () { return reinterpret_cast<SOCKADDR*>(this); }
    constexpr static int Size() { return sizeof(CSOCKADDR_IN); }
};

class CSOCKET
{
public:
    CSOCKET(const CSOCKET&) = delete;
    CSOCKET& operator = (const CSOCKET&) = delete;

    constexpr explicit CSOCKET(SOCKET sock = INVALID_SOCKET) noexcept : m_socket(sock) {}
    CSOCKET(CSOCKET&& sock) noexcept : m_socket(sock.Release()) {}
    CSOCKET& operator = (CSOCKET&& sock) noexcept
    {
        if (this != &sock)
            Reset(sock.Release());
    }
    CSOCKET(int af, int type, int protocol) noexcept
    {
        Init(af, type, protocol);
    }
    ~CSOCKET() noexcept
    {
        Reset();
    }

    bool Init(int af, int type, int protocol) noexcept
    {
        Reset(::socket(af, type, protocol));
        return *this;
    }
    SOCKET Get() const noexcept
    {
        return m_socket; 
    }
    void Reset(SOCKET sock = INVALID_SOCKET) noexcept
    {
        if (m_socket != INVALID_SOCKET)
            ::closesocket(m_socket);
        m_socket = sock;
    }
    SOCKET Release() noexcept
    {
        SOCKET ret = m_socket;
        m_socket = INVALID_SOCKET;
        return ret;
    }

    operator SOCKET () const noexcept
    {
        return m_socket;
    }
    explicit operator bool() const noexcept { return !!*this; }
    bool operator !() const noexcept { return m_socket == INVALID_SOCKET; }
private:
    SOCKET m_socket;
};


typedef DWORD(__stdcall *ErrorGetter)();
static inline constexpr ErrorGetter GetWSALastErrorFn() noexcept 
{
    return reinterpret_cast<ErrorGetter>(WSAGetLastError); 
}

static std::wstring GetErrorMsg(ErrorGetter getErrFn = GetWSALastErrorFn())
{
    using namespace std::literals;
    DWORD errCode = getErrFn();
    std::wstring msg = L"Error code: "s + std::to_wstring(errCode);

    LPWSTR errMsg = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS | 
        FORMAT_MESSAGE_ALLOCATE_BUFFER,
        nullptr, 
        errCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&errMsg),
        0,
        nullptr);
    if (errMsg)
    {
        msg += L"\nError message: "s + errMsg;
        LocalFree(static_cast<HLOCAL>(errMsg));
    }
    return msg;
}


#endif // !_COMMON_H_
