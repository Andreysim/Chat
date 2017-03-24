#include "ClientBase.h"
#include <algorithm>

bool ClientBase::SendData(const void* data, uint32_t size) const noexcept
{
    int result = 0;
    // send sended data size
    result = ::send(m_socket, reinterpret_cast<const char*>(&size), sizeof(uint32_t), 0);
    if (result == SOCKET_ERROR)
        return false;

    // send data
    uint32_t nSends = (size + MAX_SEND_RECV_DATA_SIZE - 1) / MAX_SEND_RECV_DATA_SIZE;
    const char* dataToSend = static_cast<const char*>(data);
    for (uint32_t i = 0; i < nSends; ++i)
    {
        result = ::send(
            m_socket, 
            dataToSend,
            (std::min)(size, MAX_SEND_RECV_DATA_SIZE),
            0);
        if (result == SOCKET_ERROR)
            return false;
        dataToSend += result;
        size -= result;
    }
    return size == 0;
}

bool ClientBase::RecvData(std::vector<char>& data, uint32_t* recved) const noexcept
{
    data.clear();
    if (recved)
        *recved = 0;

    int result;
    uint32_t recvSize = 0;
    result = ::recv(m_socket, reinterpret_cast<char*>(&recvSize), sizeof(uint32_t), 0);
    if (result == SOCKET_ERROR)
        return false;
    else if (result == 0)
        return true;

    try { data.resize(recvSize); }
    catch (std::exception&) { WSASetLastError(ERROR_OUTOFMEMORY); return false; }

    uint32_t nRecvs = (recvSize + MAX_SEND_RECV_DATA_SIZE - 1) / MAX_SEND_RECV_DATA_SIZE;
    char* dataToRecv = data.data();
    for(uint32_t i = 0; i < nRecvs; ++i)
    {
        result = ::recv(
            m_socket,
            dataToRecv,
            (std::min)(recvSize, MAX_SEND_RECV_DATA_SIZE),
            0);
        if (result == SOCKET_ERROR)
            break;
        dataToRecv += result;
        recvSize -= result;
    }
    if(result == SOCKET_ERROR) 
        data.clear();
    else if (recved)
        *recved = dataToRecv - data.data();

    return recvSize == 0;
}