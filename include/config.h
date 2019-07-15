#ifndef  CONFIG_INC
#define  CONFIG_INC

//Should we log every messages?
#define VFV_LOG_DATA

//#define LOG_UPDATE_HEAD

//This defines the CHI' 20 version of this server
#define CHI2020

enum { COUNTER_BASE = __COUNTER__ };

#ifdef CHI2020
    #define CHI2020_COUNTER __COUNTER__
#endif

//Check the number of enabled configs (max : 1 config)
//A config represents basically a set of functionalities for, e.g., a user study
#if __COUNTER__ - COUNTER_BASE > 1
    #error "Too many setups have been proposed (e.g., CHI2020)."
#else
    #if __COUNTER__ - COUNTER_BASE == 0
        #define NO_PARTICULAR_SETTING
    #endif
#endif

#define UPDATE_THREAD_FRAMERATE   20
#define MAX_NB_HEADSETS           10
#define MAX_OWNER_TIME            1.e6

#ifdef CHI2020
    #define TRIAL_WAITING_TIME               (2*1e6)
    #define MAX_INTERACTION_TECHNIQUE_NUMBER 3
    #define TRIAL_NUMBER_STUDY_1             (2*8) //Two person, 8 trial per person
    #define TRIAL_NUMBER_STUDY_2             (2*8)
#endif

#endif
