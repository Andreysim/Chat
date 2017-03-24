#include <wchar.h>
#include "ClientMessage.h"

#define breakable_block_begin do {
#define breakable_block_end }while(0)

constexpr uint32_t COMMAND_OFFSET = sizeof(uint64_t);
constexpr uint32_t MESSAGE_OFFSET = sizeof(uint64_t) + sizeof(uint32_t);
constexpr uint32_t MIN_MSG_SIZE = MESSAGE_OFFSET + sizeof(wchar_t) * 2;

/*
inline const wchar_t* FindCh(const wchar_t* beg, const wchar_t* end, wchar_t ch) noexcept
{
    beg = wmemchr(beg, ch, end - beg);
    return (beg == nullptr) ? end : beg;
}
*/

inline bool IsCommand(ClientCommand cmd) noexcept
{
    return (ClientCommand::Error < cmd && cmd < ClientCommand::COMMAND_COUNT);
}

ClientCommand ClientMessage::GetCommandId(const std::wstring& command) noexcept
{
    if (command == L"/pm")
        return ClientCommand::PrivateMessage;
    else if (command == L"/setname")
        return ClientCommand::ChangeName;
    else if (command == L"/listusers")
        return ClientCommand::ListClients;
    else if (command == L"/help")
        return ClientCommand::Help;
    else
        return ClientCommand::Error;
}

void ClientMessage::Unserialize(const void* data, uint32_t size) noexcept
{
    // break on error or invalid data format and set Error message type
    breakable_block_begin;

    if (size <= MIN_MSG_SIZE)
        break;

    auto pBegin = reinterpret_cast<const char*>(data);

    // Get timestamp
    timeStamp = *reinterpret_cast<const uint64_t*>(pBegin);

    //Get requested command
    command = *reinterpret_cast<const ClientCommand*>(pBegin + COMMAND_OFFSET);

    if (!IsCommand(command))
        break;

    auto pMsg = reinterpret_cast<const wchar_t*>(pBegin + MESSAGE_OFFSET);
    auto pEnd = pMsg + (size - MESSAGE_OFFSET) / sizeof(wchar_t);
    if (pMsg == pEnd || *(pEnd - 1) != L'\0')
        break;

    // Get sender name
    from.assign(pMsg);
    if (from.empty())
        break;
    
    if (command == ClientCommand::ListClients ||
        command == ClientCommand::ClientConnect) // We get all data needed 
        return;

    pMsg += from.size() + 1;
    if (pMsg == pEnd)
        break;

    if (command == ClientCommand::PrivateMessage) // need client name
    {
        pmTo.assign(pMsg);
        if (pmTo.empty())
            break;

        pMsg += pmTo.size() + 1;
        if (pMsg == pEnd)
            break;
    }

    msg.assign(pMsg, pEnd - 1);
    if (msg.empty())
        break;

    return;

    breakable_block_end;

    command = ClientCommand::Error;            
}

ClientMessage::Data ClientMessage::Serialize(uint32_t* size) noexcept
{
    Data retData;

    breakable_block_begin;

    if (!size)
        break;
    *size = 0;

    if (command == ClientCommand::Error || from.empty())
        break;

    uint32_t dataSize =
        sizeof(uint64_t) +
        sizeof(uint32_t) +
        (from.size() + 1) * sizeof(wchar_t) +
        (msg.size() + 1) * sizeof(wchar_t) +
        (pmTo.size() + 1) * sizeof(wchar_t);
    uint32_t actualSize = 0;

    retData.reset(new (std::nothrow) char[dataSize]());
    if (!retData)
        break;

    char* data = retData.get();

    // write timestamp
    *reinterpret_cast<uint64_t*>(data) = timeStamp;
    actualSize += sizeof(uint64_t);

    // write command
    *reinterpret_cast<ClientCommand*>(data + COMMAND_OFFSET) = command;
    actualSize += sizeof(uint32_t);

    auto pIt = reinterpret_cast<wchar_t*>(data + MESSAGE_OFFSET);

    // write sender
    wmemcpy(pIt, from.c_str(), from.size() + 1);
    pIt += from.size() + 1;
    actualSize += (from.size() + 1) * sizeof(wchar_t);

    // write receiver
    if (command == ClientCommand::PrivateMessage)
    {
        if (pmTo.empty())
            break;
        wmemcpy(pIt, pmTo.c_str(), pmTo.size() + 1);
        pIt += pmTo.size() + 1;
        actualSize += (pmTo.size() + 1) * sizeof(wchar_t);
    }

    if (!(command == ClientCommand::ClientConnect ||
        command == ClientCommand::ListClients))
    {
        if (msg.empty())
            break;
        // write message
        wmemcpy(pIt, msg.c_str(), msg.size() + 1);
        actualSize += (msg.size() + 1) * sizeof(wchar_t);
    }

    *size = actualSize;
    return retData;

    breakable_block_end;

    return nullptr;
}
