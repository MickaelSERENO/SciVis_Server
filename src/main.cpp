#include "Server.h"
#include "VFVClientSocket.h"
#include "InternalData.h"

int main()
{
    InternalData::initSingleton();

    Server<VFVClientSocket> server(4, 8000);
    server.launch(4, 8000);
    server.wait();

    return 0;
}
