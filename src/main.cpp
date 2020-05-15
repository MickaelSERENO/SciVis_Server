#include "VFVServer.h"
#include "VFVClientSocket.h"
#include "LocationServer.h"
#include "InternalData.h"
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctime>
#include <cstdlib>

#define NB_READ_THREAD 4

using namespace sereno;

static VFVServer* serverPtr = NULL;
static LocationServer* locationServerPtr = NULL;

void inSigInt(int sig)
{
    if(serverPtr)
    {
        INFO << "Closing with SIGINT\n";
        locationServerPtr->cancel();
        serverPtr->cancel();
    }
}

int main()
{
    srand(time(NULL));

    VFVServer* server = new VFVServer(NB_READ_THREAD, CLIENT_PORT);
    serverPtr = server;

    LocationServer* locationServer = new LocationServer(NB_READ_THREAD, LOCATION_PORT, server);
    locationServerPtr = locationServer;

    InternalData::initSingleton();
    server->launch();
    locationServer->launch();

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  inSigInt);

    locationServer->wait();
    locationServer->closeServer();

    server->wait();
    server->closeServer();

    delete server;

    //Copy the log file in case of "issue" from the investigators (always ;) )
    pid_t pid;
    pid = fork();

    if(pid == 0)
    {
        char logFile[1024];
        strcpy(logFile, "log.json.old");
        execl("/bin/cp", "/bin/cp", "log.json", logFile, NULL);
    }
    else
    {
        int childExitStatus;
        waitpid(pid, &childExitStatus, 0);
    }

    return 0;
}
