#ifndef _CLIENT_MESSAGE_H_
#define _CLIENT_MESSAGE_H_

#include <cinttypes>
#include <string>
#include <memory>


enum class ClientCommand : uint32_t
{
    Error,
    BroadcastMessage,
    PrivateMessage,
    ChangeName,
    ListClients,
    ClientConnect,
    ServerMsg,
    Help,
    COMMAND_COUNT,
};

enum class MsgSerializeError
{

};

class ClientMessage
{
public:
    typedef std::unique_ptr<char[]> Data;

    static ClientCommand GetCommandId(const std::wstring& command) noexcept;
    void Unserialize(const void* data, uint32_t size) noexcept;
    Data Serialize(uint32_t* size) noexcept; // returned data need to be released with delete[]

    std::wstring msg;
    std::wstring from;
    std::wstring pmTo;
    uint64_t timeStamp = 0;
    ClientCommand command = ClientCommand::Error;
};

#endif // !_CLIENT_MESSAGE_H_

