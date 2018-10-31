#ifndef  COLORMODE_INC
#define  COLORMODE_INC

/* \brief Corresponds to the color mode available on this program */
enum ColorMode
{
    RAINBOW          = 0, /*!< Rainbow colormode*/
    GRAYSCALE        = 1, /*!< Greyscale colormode*/
    WARM_COLD_CIELAB = 2, /*!< Red to blue (white in the middle) colormode. Based on CIELAB*/
    WARM_COLD_CIELUV = 3, /*!< Red to blue (white in the middle) colormode. Based on CIELUV*/
    WARM_COLD_MSH    = 4  /*!< Red to blue (white in the middle) colormode. Based on MSH*/
};

#endif
