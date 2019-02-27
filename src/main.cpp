#include "VFVServer.h"
#include "VFVClientSocket.h"
#include "InternalData.h"

#define NB_READ_THREAD 4

using namespace sereno;

int main()
{
    InternalData::initSingleton();

    VFVServer server(NB_READ_THREAD, CLIENT_PORT);
    server.launch();
    server.wait();

    return 0;
}
