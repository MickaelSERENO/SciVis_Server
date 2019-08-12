#ifndef  TRIALTABLETDATA_INC
#define  TRIALTABLETDATA_INC

#include "config.h"

namespace sereno
{
#ifdef CHI2020
    /** \brief  data structure containing the data of a given tablet */
    struct TrialTabletData
    {
        uint32_t tabletID;                 /*!< The tablet ID*/
        bool     inCurrentTrial   = false; /*!< Is the tablet doing currently a trial?*/
        bool     finishTraining   = false; /*!< Is the training. */
        uint32_t currentTechnique = 0;     /*!< The current pointing interaction technique to apply.*/
        uint32_t techniqueOrder[MAX_INTERACTION_TECHNIQUE_NUMBER]; /*!< The technique ordering*/
#if TRIAL_NUMBER_STUDY_1 < TRIAL_NUMBER_STUDY_2
#define TRIAL_TABLET_DATA_MAX_POOL_SIZE (TRIAL_NUMBER_STUDY_2/2)
#else
#define TRIAL_TABLET_DATA_MAX_POOL_SIZE (TRIAL_NUMBER_STUDY_1/2)
#endif
        uint8_t  poolTargetPositionIdxStudy1[TRIAL_TABLET_DATA_MAX_POOL_SIZE]; /*!< The index of position to fetch for the next annotation's target position (study 1)*/
        uint8_t  poolTargetPositionIdxStudy2[TRIAL_TABLET_DATA_MAX_POOL_SIZE]; /*!< The index of position to fetch for the next annotation's target position (study 2)*/
    };
#endif
}

#endif
