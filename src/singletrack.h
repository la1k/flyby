#ifndef SINGLETRACK_H_DEFINED
#define SINGLETRACK_H_DEFINED

#include "hamlib.h"
#include <predict/predict.h>
#include "tle_db.h"
#include "transponder_db.h"

/* This function tracks a single satellite in real-time
 * until 'Q' or ESC is pressed.
 *
 * \param orbit_ind Which orbit is first displayed on screen (can be changed within SingleTrack using left/right buttons)
 * \param qth Point of observation
 * \param transponder_db Transponder database
 * \param tle_db TLE database
 * \param rotctld rotctld connection instance
 * \param downlink_info rigctld connection instance for downlink
 * \param uplink_info rigctld connection instance for uplink
 **/
void singletrack(int orbit_ind, predict_observer_t *qth, struct transponder_db *transponder_db, struct tle_db *tle_db, rotctld_info_t *rotctld, rigctld_info_t *downlink_info, rigctld_info_t *uplink_info);

#endif
