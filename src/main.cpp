#include "VFVServer.h"
#include "VFVClientSocket.h"
#include "InternalData.h"
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctime>
#include <cstdlib>

#define NB_READ_THREAD 4

using namespace sereno;

static VFVServer* serverPtr = NULL;

void inSigInt(int sig)
{
    if(serverPtr)
    {
        INFO << "Closing with SIGINT\n";
        serverPtr->cancel();
    }
}

int main()
{
    srand(time(NULL));

    VFVServer server(NB_READ_THREAD, CLIENT_PORT);
    serverPtr = &server;

    InternalData::initSingleton();
    server.launch();

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  inSigInt);

    server.wait();
    server.closeServer();

    //Copy the log file in case of "issue" from the investigators (always ;) )
    pid_t pid;
    pid = fork();

    if(pid == 0)
    {
        char logFile[1024];
#ifdef CHI2020
        sprintf(logFile, "pair%d.json", server.getPairID());
#else
        strcpy(logFile, "log.json.old");
#endif
        execl("/bin/cp", "/bin/cp", "log.json", logFile, NULL);
    }
    else
    {
        int childExitStatus;
        waitpid(pid, &childExitStatus, 0);
    }


    return 0;
}
