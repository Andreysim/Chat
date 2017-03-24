#include "Server.h"
#include <iostream>


int main()
{
    int ret = 1;
    try
    {
        Server serv;
        ret = !serv.Run();
        return ret;
    }
    catch (std::exception& exc)
    {
        std::cerr << exc.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unknown exception" << std::endl;
    }
    return ret;
}

