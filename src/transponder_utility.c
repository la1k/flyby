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

int main(int argc, char **argv)
{
	string_array_t transponder_db_filenames = {0}; //TLE files to be used to update the TLE databases

	//command line options
	struct option long_options[] = {
		{"help",			no_argument,		0,	'h'},
		{0, 0, 0, 0}
	};
	char short_options[] = "h";
	while (1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, short_options, long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 'u': //updatefile
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

	//read current transponder database
	struct transponder_db *transponder_db = transponder_db_create(tle_db);
	transponder_db_from_search_paths(tle_db, transponder_db);

	for (int i=0; i < string_array_size(&transponder_db_filenames); i++) {
		struct transponder_db *file_db = transponder_db_create(tle_db);

		transponder_db_destroy(file_db);
	}

	//free memory
	tle_db_destroy(&tle_db);
	transponder_db_destroy(&transponder_db);
}
