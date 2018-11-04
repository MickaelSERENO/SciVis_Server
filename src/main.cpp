#include "VFVServer.h"
#include "VFVClientSocket.h"
#include "InternalData.h"

int main()
{
    InternalData::initSingleton();

    VFVServer server(4, 8000);
    server.launch();
    server.wait();

    return 0;
}
