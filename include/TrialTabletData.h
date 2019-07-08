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
    };
#endif
}

#endif
