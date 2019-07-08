#ifndef  CONFIG_INC
#define  CONFIG_INC

//Should we log every messages?
#define VFV_LOG_DATA

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

#define UPDATE_THREAD_FRAMERATE   10
#define MAX_NB_HEADSETS           10
#define MAX_OWNER_TIME            1.e6

#ifdef CHI2020
    #define SLEEP_NEXTTRIAL_MIN_TIME (3*1.e6)
    #define SLEEP_NEXTTRIAL_MAX_TIME (1*1.e6)
    #define MAX_INTERACTION_TECHNIQUE_NUMBER 4
#endif

#endif
