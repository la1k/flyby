#include <predict/predict.h>
#include "defines.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <curses.h>
#include <stdlib.h>
#include "tle_db.h"
#include "multitrack.h"

int MultiColours(double range, double elevation);


multitrack_entry_t *multitrack_create_entry(const char *name, predict_orbital_elements_t *orbital_elements)
{
	multitrack_entry_t *entry = (multitrack_entry_t*)malloc(sizeof(multitrack_entry_t));
	entry->orbital_elements = orbital_elements;
	entry->name = strdup(name);
	entry->next_aos = 0;
	entry->next_los = 0;
	entry->above_horizon = 0;
	entry->geostationary = 0;
	entry->never_visible = 0;
	entry->decayed = 0;
	return entry;
}

multitrack_listing_t* multitrack_create_listing(predict_observer_t *observer, predict_orbital_elements_t **orbital_elements, struct tle_db *tle_db)
{
	int window_height = 10;

	int num_enabled_tles = 0;
	for (int i=0; i < tle_db->num_tles; i++) {
		if (tle_db_entry_enabled(tle_db, i)) {
			num_enabled_tles++;
		}
	}

	multitrack_listing_t *listing = (multitrack_listing_t*)malloc(sizeof(multitrack_listing_t));
	listing->selected_entry_index = 0;
	listing->qth = observer;
	listing->num_entries = num_enabled_tles;
	listing->entries = (multitrack_entry_t**)malloc(sizeof(multitrack_entry_t*)*num_enabled_tles);

	int j=0;
	for (int i=0; i < tle_db->num_tles; i++) {
		if (tle_db_entry_enabled(tle_db, i)) {
			listing->entries[j] = multitrack_create_entry(tle_db->tles[i].name, orbital_elements[i]);
			j++;
		}
	}

	listing->sorted_index = (int*)calloc(tle_db->num_tles, sizeof(int));
	listing->num_above_horizon = 0;
	listing->num_below_horizon = 0;
	listing->num_decayed = 0;
	listing->num_nevervisible = 0;

	listing->top_index = 0;
	listing->bottom_index = listing->bottom_index + window_height;
	listing->num_displayed_entries = window_height;

	return listing;
}

void multitrack_update_entry(predict_observer_t *qth, multitrack_entry_t *entry, predict_julian_date_t time)
{
	entry->geostationary = false;

	struct predict_observation obs;
	struct predict_orbit orbit;
	predict_orbit(entry->orbital_elements, &orbit, time);
	predict_observe_orbit(qth, &orbit, &obs);

	//sun status
	char sunstat;
	if (!orbit.eclipsed) {
		if (obs.visible) {
			sunstat='V';
		} else {
			sunstat='D';
		}
	} else {
		sunstat='N';
	}

	//satellite approaching status
	char rangestat;
	if (fabs(obs.range_rate) < 0.1) {
		rangestat = '=';
	} else if (obs.range_rate < 0.0) {
		rangestat = '/';
	} else if (obs.range_rate > 0.0) {
		rangestat = '\\';
	}

	//set text formatting attributes according to satellite state, set AOS/LOS string
	bool can_predict = !predict_is_geostationary(entry->orbital_elements) && predict_aos_happens(entry->orbital_elements, qth->latitude) && !(orbit.decayed);
	char aos_los[MAX_NUM_CHARS] = {0};
	if (obs.elevation >= 0) {
		//different colours according to range and elevation
		entry->display_attributes = MultiColours(obs.range, obs.elevation*180/M_PI);

		if (predict_is_geostationary(entry->orbital_elements)){
			sprintf(aos_los, "*GeoS*");
			entry->geostationary = true;
		} else {
			time_t epoch = predict_from_julian(entry->next_los - time);
			strftime(aos_los, MAX_NUM_CHARS, "%M:%S", gmtime(&epoch)); //time until LOS
		}
	} else if ((obs.elevation < 0) && can_predict) {
		if ((entry->next_aos-time) < 0.00694) {
			//satellite is close, set bold
			entry->display_attributes = COLOR_PAIR(2);
			time_t epoch = predict_from_julian(entry->next_aos - time);
			strftime(aos_los, MAX_NUM_CHARS, "%M:%S", gmtime(&epoch)); //minutes and seconds left until AOS
		} else {
			//satellite is far, set normal coloring
			entry->display_attributes = COLOR_PAIR(4);
			time_t epoch = predict_from_julian(entry->next_aos);
			strftime(aos_los, MAX_NUM_CHARS, "%j.%H:%M:%S", gmtime(&epoch)); //time for AOS
		}
	} else if (!can_predict) {
		entry->display_attributes = COLOR_PAIR(3);
		sprintf(aos_los, "*GeoS-NoAOS*");
	}

	char abs_pos_string[MAX_NUM_CHARS] = {0};
	sprintf(abs_pos_string, "%3.0f  %3.0f", orbit.latitude*180.0/M_PI, orbit.longitude*180.0/M_PI);

	/* Calculate Next Event (AOS/LOS) Times */
	if (can_predict && (time > entry->next_los) && (obs.elevation > 0)) {
		entry->next_los= predict_next_los(qth, entry->orbital_elements, time);
	}

	if (can_predict && (time > entry->next_aos)) {
		if (obs.elevation < 0) {
			entry->next_aos = predict_next_aos(qth, entry->orbital_elements, time);
		}
	}

	//altitude and range in km/miles
	double disp_altitude = orbit.altitude;
	double disp_range = obs.range;

	//set string to display
	char disp_string[MAX_NUM_CHARS];
	sprintf(disp_string, " %-13.8s%5.1f  %5.1f %8s  %6.0f %6.0f %c %c %12s ", entry->name, obs.azimuth*180.0/M_PI, obs.elevation*180.0/M_PI, abs_pos_string, disp_altitude, disp_range, sunstat, rangestat, aos_los);

	//overwrite everything if orbit was decayed
	if (orbit.decayed) {
		entry->display_attributes = COLOR_PAIR(2);
		sprintf(disp_string, " %-10s   ----------------       Decayed        ---------------", entry->name);
	}

	memcpy(entry->display_string, disp_string, sizeof(char)*MAX_NUM_CHARS);

	entry->above_horizon = obs.elevation > 0;
	entry->decayed = orbit.decayed;

	entry->never_visible = !predict_aos_happens(entry->orbital_elements, qth->latitude) || (predict_is_geostationary(entry->orbital_elements) && (obs.elevation <= 0.0));
}

void multitrack_update_listing(multitrack_listing_t *listing, predict_julian_date_t time) {
	for (int i=0; i < listing->num_entries; i++) {
		multitrack_entry_t *entry = listing->entries[i];
		multitrack_update_entry(listing->qth, entry, time);
		listing->sorted_index[i] = i;
	}
}

void multitrack_sort_listing(multitrack_listing_t *listing)
{
	int num_orbits = listing->num_entries;

	//those with elevation > 0 at the top
	int above_horizon_counter = 0;
	for (int i=0; i < num_orbits; i++){
		if (listing->entries[i]->above_horizon) {
			listing->sorted_index[above_horizon_counter] = i;
			above_horizon_counter++;
		}
	}
	listing->num_above_horizon = above_horizon_counter;

	//satellites that will eventually rise above the horizon
	int below_horizon_counter = 0;
	for (int i=0; i < num_orbits; i++){
		if (!(listing->entries[i]->above_horizon) && !(listing->entries[i]->never_visible)) {
			listing->sorted_index[below_horizon_counter + above_horizon_counter] = i;
			below_horizon_counter++;
		}
	}
	listing->num_below_horizon = below_horizon_counter;

	//satellites that will never be visible, with decayed orbits last
	int nevervisible_counter = 0;
	int decayed_counter = 0;
	for (int i=0; i < num_orbits; i++){
		if (listing->entries[i]->never_visible && !(listing->entries[i]->decayed)) {
			listing->sorted_index[below_horizon_counter + above_horizon_counter + nevervisible_counter] = i;
			nevervisible_counter++;
		} else if (listing->entries[i]->decayed) {
			listing->sorted_index[num_orbits - 1 - decayed_counter] = i;
			decayed_counter++;
		}
	}
	listing->num_nevervisible = nevervisible_counter;
	listing->num_decayed = decayed_counter;

	/*
	//sort internally according to AOS/LOS
	for (int i=0; i < above_horizon_counter + below_horizon_counter; i++) {
		for (int j=0; j < above_horizon_counter + below_horizon_counter - 1; j++){
			if (aos[satindex[j]]>aos[satindex[j+1]]) {
				int x = satindex[j];
				satindex[j] = satindex[j+1];
				satindex[j+1] = x;
			}
		}
	}*/
}

void multitrack_display_entry(int row, int col, multitrack_entry_t *entry)
{
	attrset(entry->display_attributes);
	mvprintw(row, col, "%s", entry->display_string);
}

void multitrack_display_listing(multitrack_listing_t *listing)
{
	listing->entries[listing->sorted_index[listing->selected_entry_index]]->display_attributes = SELECTED_ATTRIBUTE;

	int line = 5;
	int col = 1;

	for (int i=listing->top_index; i <= listing->bottom_index; i++) {
		if ((i == listing->num_above_horizon) || (i == (listing->num_above_horizon + listing->num_below_horizon))){
			attrset(0);
			mvprintw(line++, 1, "                                                                    ");
		}
		multitrack_display_entry(line++, col, listing->entries[listing->sorted_index[i]]);
	}
}

void multitrack_handle_listing(multitrack_listing_t *listing, int input_key)
{
	switch (input_key) {
		case KEY_UP:
			listing->selected_entry_index--;
			if (listing->selected_entry_index < 0) listing->selected_entry_index = 0;
			break;

		case KEY_DOWN:
			listing->selected_entry_index++;
			if (listing->selected_entry_index >= listing->num_entries) {
				listing->selected_entry_index = listing->num_entries-1;
			}
			break;
	}

	//check for scroll event
	if (listing->selected_entry_index > listing->bottom_index) {
		listing->bottom_index++;
		listing->top_index++;
	}

	if (listing->selected_entry_index < listing->top_index) {
		listing->bottom_index--;
		listing->top_index--;
	}
}
