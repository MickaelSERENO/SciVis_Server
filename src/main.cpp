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

#define HOLOLENS_ID 0
#define TABLET_ID   1

#define UPDATE_VRPN_FRAMERATE 30

using namespace sereno;

//All the "connection" pointers
static VFVServer* serverPtr = NULL;
static LocationServer* vuforiaLocationServerPtr = NULL;
static std::thread* viconServerPtr = NULL;

/** The location mode defined by an environment variable. See TRACKING_VICON and TRACKING_VUFORIA */
static int locationMode = -1;

/** Should the app closes? Used by external threads */
static bool closeApp = false;

/* \brief  Function handling the signal "SIGINT"
 * \param sig the signal received (should always be SIGINT) */
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
        //DO NOT CANCEL OTHER SERVER BECAUSE SIGNAL ARE PARALLELIZED. If "ServerPtr" closes, everything else should close as well.
    }
}

/* \brief  The VRPN callback function regarding trackers
 * \param userData the user data
 * \param t information regarding trackers */
void VRPN_CALLBACK trackerVRPNCallback(void* userData, const vrpn_TRACKERCB t)
{
    uint32_t deviceID = *(uint32_t*)userData;
//    INFO << "DeviceID: " << deviceID << ' '
//         << "pos: " << t.pos[0]  << ',' << t.pos[1]  << ',' << t.pos[2]  << ' '
//         << "rot: " << t.quat[0] << ',' << t.quat[1] << ',' << t.quat[2] << ',' << t.quat[3] << std::endl;

    //Push the position and rotation
    if(deviceID == TABLET_ID)
        serverPtr->pushTabletVRPNPosition (glm::vec3(t.pos[0], t.pos[1], t.pos[2]), Quaternionf(t.quat[0], t.quat[1], t.quat[2], t.quat[3]), 0);
    else
        serverPtr->pushHeadsetVRPNPosition(glm::vec3(t.pos[0], t.pos[1], t.pos[2]), Quaternionf(t.quat[0], t.quat[1], t.quat[2], t.quat[3]), 0);
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

    //Initialize VRPN tracking mode (VICON)
    else if(locationMode == TRACKING_VICON)
    {
        viconServerPtr = new std::thread([&]()
        {
            //Device IDs
            uint32_t holoLensID = HOLOLENS_ID;
            uint32_t tabletID   = TABLET_ID;

            //HoloLens
            vrpn_Tracker_Remote* vrpnTrackerHoloLens = new vrpn_Tracker_Remote("Hololens2@192.168.2.3");
            vrpnTrackerHoloLens->register_change_handler(&holoLensID, trackerVRPNCallback);

            //Multi touch tablet
            vrpn_Tracker_Remote* vrpnTrackerTablet = new vrpn_Tracker_Remote("GalaxyTabS4@192.168.2.3");
            vrpnTrackerTablet->register_change_handler(&tabletID, trackerVRPNCallback);

            vrpn_Tracker_Remote* trackers[] = {vrpnTrackerHoloLens, vrpnTrackerTablet};

            while(!closeApp)
            {
                struct timespec beg;
                struct timespec end;
                clock_gettime(CLOCK_REALTIME, &beg);

                uint32_t startTime = beg.tv_nsec*1.e-3 + end.tv_sec*1.e6;

                //Enter the VRPN main loop
                for(vrpn_Tracker_Remote* it : trackers)
                {
                    it->mainloop();
                    it->mainloop();
                }
                clock_gettime(CLOCK_REALTIME, &end);
                uint32_t endTime = beg.tv_nsec*1.e-3 + end.tv_sec*1.e6;

                usleep(std::max(0.0, 1.e6/UPDATE_VRPN_FRAMERATE - endTime + startTime));

                //Commit all the positions if the VRPN connection works
                for(vrpn_Tracker_Remote* it : trackers)
                    if(it->connectionPtr() != NULL && it->connectionPtr()->doing_okay() && it->connectionPtr()->connected())
                    {
                        serverPtr->commitAllVRPNPositions();
                    }
            }

            //Close every connections
            for(vrpn_Tracker_Remote* it : trackers)
                delete it;
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
    //For that, fork the application to run the "cp" command (more reliable)
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
