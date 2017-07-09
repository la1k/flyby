#ifndef FLYBY_DEFINES_H_DEFINED
#define FLYBY_DEFINES_H_DEFINED

#define MAX_NUM_CHARS		1024
#define MAX_NUM_TRANSPONDERS	10

//Height of window on bottom of multitrack defining the main menu options
#define MAIN_MENU_OPTS_WIN_HEIGHT 3

#define EARTH_RADIUS_KM		6.378137E3		/* WGS 84 Earth radius km */
#define HALF_DELAY_TIME	5
#define	KM_TO_MI		0.621371		/* km to miles */


//inactive/deselected color style for settings field
#define FIELDSTYLE_INACTIVE COLOR_PAIR(1)|A_UNDERLINE

//active/selected color style for settings field
#define FIELDSTYLE_ACTIVE COLOR_PAIR(5)

#define FIELDSTYLE_DESCRIPTION COLOR_PAIR(4)|A_BOLD 

#endif
