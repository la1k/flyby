#include <predict/predict.h>

/**
 * Convenience enum so that screens for predicting moon and sun can be unified to one function.
 **/
enum celestial_object{PREDICT_SUN, PREDICT_MOON};

/* This function predicts satellite passes.
 *
 * \param name Name of satellite
 * \param orbital_elements Orbital elements of satellite
 * \param qth QTH at which satellite is to be observed
 * \param mode 'p' for all passes, 'v' for visible passes only
 **/
void satellite_pass_display_schedule(const char *name, predict_orbital_elements_t *orbital_elements, predict_observer_t *qth, char mode);

/**
 * Display solar illumination predictions.
 *
 * \param name Name of satellite
 * \param orbital_elements Orbital elements for satellite
 **/
void solar_illumination_display_predictions(const char *name, predict_orbital_elements_t *orbital_elements);

/**
 * Predict passes of sun and moon, similar to Predict().
 *
 * \param object Sun or moon
 * \param qth Point of observation
**/
void sun_moon_pass_display_schedule(enum celestial_object object, predict_observer_t *qth);
