#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <memory>

class Client
{
public:
    Client(const Client&) = delete;
    Client& operator = (const Client&) = delete;

    Client(Client&&);
    Client& operator = (Client&&);

    Client();
    ~Client();

    bool Run();
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // !_CLIENT_H_

