#ifndef HAMLIB_SETTINGS_H_DEFINED
#define HAMLIB_SETTINGS_H_DEFINED

/**
 * Show hamlib status window.
 *
 * \param rotctld Rotctld connection instance
 * \param downlink Downlink rigctld connection instance
 * \param uplink Uplink rigctld connection instance
 **/
void hamlib_status(rotctld_info_t *rotctld, rigctld_info_t *downlink, rigctld_info_t *uplink);

#endif
