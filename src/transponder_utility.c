#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdbool.h>
#include "defines.h"
#include <math.h>
#include "string_array.h"
#include "qth_config.h"
#include "tle_db.h"
#include "xdg_basedirs.h"
#include "transponder_db.h"
#include <libgen.h>
#include <math.h>

void print_transponder_entry_differences(const struct sat_db_entry *old_db_entry, const struct sat_db_entry *new_db_entry)
{
	for (int i=0; i < fmax(old_db_entry->num_transponders, new_db_entry->num_transponders); i++) {
		const char *old_name = old_db_entry->transponder_name[i];
		const char *new_name = new_db_entry->transponder_name[i];

		if (strncmp(old_name, new_name, MAX_NUM_CHARS) != 0) {
			fprintf(stderr, "Names differ: `%s` -> `%s`\n", old_name, new_name);
		}

		double old_value = old_db_entry->uplink_start[i];
		double new_value = new_db_entry->uplink_start[i];
		if (old_value != new_value) fprintf(stderr, "Uplink start differs: %f -> %f\n", old_value, new_value);

		old_value = old_db_entry->uplink_end[i];
		new_value = new_db_entry->uplink_end[i];
		if (old_value != new_value) fprintf(stderr, "Uplink end differs: %f -> %f\n", old_value, new_value);

		old_value = old_db_entry->downlink_start[i];
		new_value = new_db_entry->downlink_start[i];
		if (old_value != new_value) fprintf(stderr, "Downlink start differs: %f -> %f\n", old_value, new_value);

		old_value = old_db_entry->downlink_end[i];
		new_value = new_db_entry->downlink_end[i];
		if (old_value != new_value) fprintf(stderr, "Downlink end differs: %f -> %f\n", old_value, new_value);
	}
}

int main(int argc, char **argv)
{
	string_array_t transponder_db_filenames = {0}; //TLE files to be used to update the TLE databases

	//command line options
	struct option long_options[] = {
		{"transponder-file",		required_argument,	0,	't'},
		{"help",			no_argument,		0,	'h'},
		{0, 0, 0, 0}
	};
	char short_options[] = "t:h";
	while (1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, short_options, long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 't': //transponder file
				string_array_add(&transponder_db_filenames, optarg);
				break;
			case 'h': //help
				fprintf(stderr, "Flyby transponder utility\n");
				break;
		}
	}

	//read TLE database
	struct tle_db *tle_db = tle_db_create();
	tle_db_from_search_paths(tle_db);
	if (tle_db->num_tles == 0) {
		fprintf(stderr, "No TLEs defined.\n");
	}

	//read current transponder database
	struct transponder_db *transponder_db = transponder_db_create(tle_db);
	transponder_db_from_search_paths(tle_db, transponder_db);

	for (int i=0; i < string_array_size(&transponder_db_filenames); i++) {
		const char *filename = string_array_get(&transponder_db_filenames, i);
		struct transponder_db *file_db = transponder_db_create(tle_db);
		if (transponder_db_from_file(filename, tle_db, file_db, LOCATION_TRANSIENT) != TRANSPONDER_SUCCESS) {
			fprintf(stderr, "Could not read file: %s\n", filename);
			continue;
		}

		for (int j=0; j < file_db->num_sats; j++) {
			struct sat_db_entry *new_db_entry = &(file_db->sats[j]);
			struct sat_db_entry *old_db_entry = &(transponder_db->sats[j]);
			if (!transponder_db_entry_empty(new_db_entry) && !transponder_db_entry_equal(old_db_entry, new_db_entry)) {
				if (transponder_db_entry_empty(old_db_entry)) {
					fprintf(stderr, "New entry\n\n");
				} else {
					fprintf(stderr, "Entries differ for %s:\n", tle_db->tles[j].name);
					print_transponder_entry_differences(old_db_entry, new_db_entry);
					fprintf(stderr, "Accept change? (y/n) ");
					while (true) {
						int c = getchar();
						if (c == 'y') {
							//accept change
							break;
						} else if (c == 'n') {
							//decline change
							break;
						}
					}
					fprintf(stderr, "\n");
				}
			}
		}

		transponder_db_destroy(&file_db);
	}

	//free memory
	tle_db_destroy(&tle_db);
	transponder_db_destroy(&transponder_db);
}
