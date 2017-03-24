#include "Client.h"
#include <iostream>

int main()
{
    int ret = 1;
    try
    {
        Client client;
        ret = !client.Run();
    }
    catch (std::exception& exc)
    {
        std::cout << exc.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Unknown exception" << std::endl;
    }
    return ret;
}

