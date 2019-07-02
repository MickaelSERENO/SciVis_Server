#include "VFVServer.h"
#include "VFVClientSocket.h"
#include "InternalData.h"
#include <signal.h>

#define NB_READ_THREAD 4

using namespace sereno;

static VFVServer server(NB_READ_THREAD, CLIENT_PORT);

void inSigInt(int sig)
{
    INFO << "Closing with SIGINT\n";
    server.cancel();
}

int main()
{
    InternalData::initSingleton();
    server.launch();

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  inSigInt);

    server.wait();
    server.closeServer();

    return 0;
}
