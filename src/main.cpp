#include "VFVServer.h"
#include "VFVClientSocket.h"
#include "LocationServer.h"
#include "InternalData.h"
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctime>
#include <cstdlib>
#include <vrpn_Tracker.h>

#define TRACKING_VUFORIA 1
#define TRACKING_VICON   2
#define NB_READ_THREAD   4

using namespace sereno;

static VFVServer* serverPtr = NULL;
static LocationServer* vuforiaLocationServerPtr = NULL;
static std::thread* viconServerPtr = NULL;

/** The location mode defined by an environment variable. See TRACKING_VICON and TRACKING_VUFORIA */
static int locationMode = -1;

static bool closeApp = false;

void inSigInt(int sig)
{
    if(!closeApp)
    {
        closeApp = true;
        if(serverPtr)
        {
            INFO << "Closing with SIGINT\n";
            serverPtr->cancel();
        }

        //DO NOT CANCEL OTHER SERVER BECAUSE SIGNAL ARE PARALLELIZED
    }
}

void VRPN_CALLBACK trackerVRPNCallback(void* userData, const vrpn_TRACKERCB t)
{
    INFO << "Tracker '" << t.sensor << "' : " << t.pos[0] << "," <<  t.pos[1] << "," << t.pos[2] << std::endl;
}

int main(int argc, char** argv)
{
    //Read application arguments
    for(int i = 1; i < argc; i++)
    {
        if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
        {
            std::cout << "Application permitting to launch the server for the SciVis_HoloLens project.\n" << std::endl
                      << "Help command" << std::endl
                      << "[LD_LIBRARY_PATH=$HOME/.local] [TRACKING_MODE=mode] ./VFVServer" << std::endl
                      << "LD_LIBRARY_PATH: tells where are your built-in libraries (UNIX environment variable)" << std::endl
                      << "TRACKING_MODE  : application-defined environment variable. It defines the tracking mode to use. By default, no tracking is performed. Set at " << TRACKING_VUFORIA << " to use the VUFORIA tracking (debug mode only) or set it at " << TRACKING_VICON << " to use the VICON system." << std::endl; 
        }
    }

    //Init built-in variables
    srand(time(NULL));
    if(getenv("TRACKING_MODE"))
        locationMode = std::atoi(getenv("TRACKING_MODE"));

    InternalData::initSingleton();

    //Launch the main server
    VFVServer* server = new VFVServer(NB_READ_THREAD, CLIENT_PORT);
    serverPtr = server;
    server->launch();

    //Initialize vuforia tracking mode if needed
    if(locationMode == TRACKING_VUFORIA)
    {
        vuforiaLocationServerPtr = new LocationServer(1, LOCATION_PORT, server);
        vuforiaLocationServerPtr->launch();
    }

    else if(locationMode == TRACKING_VICON)
    {
        viconServerPtr = new std::thread([&]()
        {
            vrpn_Tracker_Remote* vrpnTracker = new vrpn_Tracker_Remote( "192.168.2.3");
            vrpnTracker->register_change_handler(0, trackerVRPNCallback);
            while(!closeApp)
            {
                vrpnTracker->mainloop();
            }
            delete vrpnTracker;
        });
    }

    //Listen to the signal "SIGINT" and cancel the signal "SIGPIPE" (which is due to socket errors)
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  inSigInt);

    //Wait for the server first. If it closes, everything else should close as well
    server->wait();
    server->closeServer();

    //Delete vuforia tracking if needed
    if(locationMode == TRACKING_VUFORIA)
    {
        vuforiaLocationServerPtr->closeServer();
        delete vuforiaLocationServerPtr;
    }

    //Delete data regarding vrpn vicon tracking
    else if(locationMode == TRACKING_VICON)
    {
        if(viconServerPtr != NULL)
        {
            if(viconServerPtr->joinable())
                viconServerPtr->join();
            delete viconServerPtr;
        }
    }

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
