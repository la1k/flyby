#ifndef HAMLIB_SETTINGS_H_DEFINED
#define HAMLIB_SETTINGS_H_DEFINED

#include "hamlib.h"
#include <form.h>

/**
 * Used for deciding whether the background should be cleared or not before
 * displaying the hamlib status window.
 **/
enum hamlib_status_background_clearing {
	HAMLIB_STATUS_CLEAR_BACKGROUND,
	HAMLIB_STATUS_KEEP_BACKGROUND
};

/**
 * Show hamlib status window.
 *
 * \param rotctld Rotctld connection instance
 * \param downlink Downlink rigctld connection instance
 * \param uplink Uplink rigctld connection instance
 * \param clear Whether background should be partially cleared or not before displaying the window
 **/
void hamlib_status(rotctld_info_t *rotctld, rigctld_info_t *downlink, rigctld_info_t *uplink, enum hamlib_status_background_clearing clear);

#endif
