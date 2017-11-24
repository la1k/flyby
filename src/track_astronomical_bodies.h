#ifndef TRACK_ASTRONOMICAL_BODIES_H_DEFINED
#define TRACK_ASTRONOMICAL_BODIES_H_DEFINED

#include "hamlib.h"

/**
 * Display UI for tracking various astronomical bodies through rotctld.
 *
 * Singletrack generalized to planets and other objects.
 *
 * \param qth Ground station coordinates
 * \param rotctld Rotctld connection instance
 **/
void track_astronomical_body(predict_observer_t *qth, rotctld_info_t *rotctld);

/**
 * Type of astronomical body.
 *
 * Note: For convenience in creating the astronomical body displayers in
 * track_astronomical_body(...), this enum is looped through from 0 to
 * NUM_ASTRONOMICAL_BODIES. Will break if any of the enums inside are redefined
 * to constants other than the defaults.
 *
 * New astronomical bodies are added by adding a new enum here, updating NUM_ASTRONOMICAL_BODIES and
 * updating astronomical_body_to_name(...) and observe_astronomical_body(...) in track_astronomical_bodies.c
 * accordingly. Might also have to modify the layout at some point.
 **/
enum astronomical_body {
	///Sun
	PREDICT_SUN,
	///Moon
	PREDICT_MOON,
};

///Number of astronomical objects defined above.
#define NUM_ASTRONOMICAL_BODIES 2

/**
 * Calculate observation-dependent properties for astronomical body.
 *
 * \param type Type of astronomical body
 * \param qth Ground station
 * \param day Time
 * \param observation Returned properties
 **/
void observe_astronomical_body(enum astronomical_body type, predict_observer_t *qth, predict_julian_date_t day, struct predict_observation *observation);

#endif
