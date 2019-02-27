#include "VFVServer.h"
#include "VFVClientSocket.h"
#include "InternalData.h"

using namespace sereno;

int main()
{
    InternalData::initSingleton();

    VFVServer server(4, 8000);
    server.launch();
    server.wait();

    return 0;
}
