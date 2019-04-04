#include "VFVServer.h"
#include "VFVClientSocket.h"
#include "InternalData.h"
#include <signal.h>

#define NB_READ_THREAD 4

using namespace sereno;

int main()
{
    signal(SIGPIPE, SIG_IGN);
    InternalData::initSingleton();

    VFVServer server(NB_READ_THREAD, CLIENT_PORT);
    server.launch();
    server.wait();

    return 0;
}
