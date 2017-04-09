#ifndef LOCATOR_H_DEFINED
#define LOCATOR_H_DEFINED

/**
 * Convert maidenhead grid locator to WGS84 coordinates.
 *
 * \param locator Locator string
 * \param ret_longitude Returned longitude
 * \param ret_latitude Returned latitude
 **/
void maidenhead_to_latlon(const char *locator, double *ret_longitude, double *ret_latitude);

/**
 * Convert WGS84 coordinates (N/E) to maidenhead locator.
 *
 * \param mLtd Degrees latitude north
 * \param mLng Degrees longitude east
 * \param mStr Output locator string
 **/
void latlon_to_maidenhead(double mLtd, double mLng, char* mStr);

#endif
