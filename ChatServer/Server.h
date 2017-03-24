#ifndef _SERVER_H_
#define _SERVER_H_

#include <memory>

#ifndef DEF_SERV_PORT 
#define DEF_SERV_PORT 51488
#endif

class Server
{
public:
    Server(const Server&) = delete;
    Server& operator = (const Server&) = delete;

    Server(Server&&);
    Server& operator = (Server&&);

    Server(uint16_t port = DEF_SERV_PORT);
    ~Server();

    bool Run();
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;   
};

#endif // !_SERVER_H_
