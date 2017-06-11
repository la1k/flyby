#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdbool.h>
#include "defines.h"
#include "hamlib.h"
#include <predict/predict.h>
#include <math.h>
#include "ui.h"
#include "string_array.h"
#include "qth_config.h"
#include "tle_db.h"
#include "xdg_basedirs.h"
#include "transponder_db.h"
#include "option_help.h"
#include <libgen.h>

//longopt value identificators for command line options without shorthand
#define FLYBY_OPT_ROTCTLD_PORT 201
#define FLYBY_OPT_UPLINK_PORT 202
#define FLYBY_OPT_UPLINK_VFO 203
#define FLYBY_OPT_DOWNLINK_PORT 204
#define FLYBY_OPT_DOWNLINK_VFO 205
#define FLYBY_OPT_ROTCTLD_UPDATE_INTERVAL 206
#define FLYBY_OPT_ADD_TLE 207

/**
 * Print flyby program usage to stdout.
 *
 * \param program_name Name of program, use argv[0]
 * \param long_options List of long options used in getopts_long
 * \param short_options List of short options used in getopts_long
 **/
void show_help(const char *program_name, struct option long_options[], const char *short_options);

int main(int argc, char **argv)
{
	//rotctl options
	bool use_rotctl = false;
	char rotctld_host[MAX_NUM_CHARS] = ROTCTLD_DEFAULT_HOST;
	char rotctld_port[MAX_NUM_CHARS] = ROTCTLD_DEFAULT_PORT;
	int rotctld_update_interval = 0;
	double tracking_horizon = 0;

	//rigctl uplink options
	bool use_rigctld_uplink = false;
	char rigctld_uplink_host[MAX_NUM_CHARS] = RIGCTLD_UPLINK_DEFAULT_HOST;
	char rigctld_uplink_port[MAX_NUM_CHARS] = RIGCTLD_UPLINK_DEFAULT_PORT;
	char rigctld_uplink_vfo[MAX_NUM_CHARS] = {0};

	//rigctl downlink options
	bool use_rigctld_downlink = false;
	char rigctld_downlink_host[MAX_NUM_CHARS] = RIGCTLD_DOWNLINK_DEFAULT_HOST;
	char rigctld_downlink_port[MAX_NUM_CHARS] = RIGCTLD_DOWNLINK_DEFAULT_PORT;
	char rigctld_downlink_vfo[MAX_NUM_CHARS] = {0};

	//config files
	string_array_t tle_add_filenames = {0}; //TLE files to be added to TLE database
	string_array_t tle_update_filenames = {0}; //TLE files to be used to update the TLE databases
	string_array_t tle_cmd_filenames = {0}; //TLE files supplied on the command line

	char qth_filename[MAX_NUM_CHARS] = {0};
	bool qth_cmd_filename_set = false;

	//command line options
	struct option_extended options[] = {
		{{"add-tle-file",		required_argument,	0,	FLYBY_OPT_ADD_TLE},
			"FILE", "Add TLE file to flyby's TLE database. The base filename of the input file will be used for the internal file, so any existing file with this filename will be overwritten."},
		{{"update-tle-db",		required_argument,	0,	'u'},
			"FILE", "Update TLE database with TLE file FILE. Multiple files can be specified using the same option multiple times (e.g. -u file1 -u file2 ...). Flyby will exit afterwards. Any new TLEs in the file will be ignored."},
		{{"tle-file",			required_argument,	0,	't'},
			"FILE", "Use FILE as TLE database file. Overrides user and system TLE database files. Multiple files can be specified using this option multiple times (e.g. -t file1 -t file2 ...)."},
		{{"qth-file",			required_argument,	0,	'q'},
			"FILE", "Use FILE as QTH config file. Overrides existing QTH config file."},
		{{"rotctld-host",		required_argument,	0,	'A'},
			"HOST", "Connect to a rotctld server with hostname HOST and enable antenna tracking."},
		{{"rotctld-port",		required_argument,	0,	FLYBY_OPT_ROTCTLD_PORT},
			"PORT", "Specify rotctld server port."},
		{{"rotctld-horizon",		required_argument,	0,	'H'},
			"HORIZON", "Specify elevation threshold for when flyby will start tracking an orbit."},
		{{"rotctld-update-interval",	required_argument,	0,	FLYBY_OPT_ROTCTLD_UPDATE_INTERVAL},
			"SECS", "Send updates to rotctld other SECS seconds instead of when (azimuth,elevation) changes."},
		{{"rigctld-uplink-host",		required_argument,	0,	'U'},
			"HOST", "Connect to specified rigctld server for uplink frequency steering."},
		{{"rigctld-uplink-port",		required_argument,	0,	FLYBY_OPT_UPLINK_PORT},
			"PORT", "Specify rigctld uplink port."},
		{{"rigctld-uplink-vfo",		required_argument,	0,	FLYBY_OPT_UPLINK_VFO},
			"VFO_NAME", "Specify rigctld uplink VFO."},
		{{"rigctld-downlink-host",	required_argument,	0,	'D'},
			"HOST", "Connect to specified rigctld server for downlink frequency steering."},
		{{"rigctld-downlink-port",	required_argument,	0,	FLYBY_OPT_DOWNLINK_PORT},
			"PORT", "Specify rigctld downlink port."},
		{{"rigctld-downlink-vfo",	required_argument,	0,	FLYBY_OPT_DOWNLINK_VFO},
			"VFO_NAME", "Specify rigctld downlink VFO."},
		{{"help",			no_argument,		0,	'h'},
			NULL, "Show help."},
		{{0, 0, 0, 0}, NULL, NULL}
	};
	char usage_instructions[MAX_NUM_CHARS];
	snprintf(usage_instructions, MAX_NUM_CHARS, "Flyby satellite tracking program\nUsage:\n%s [options]", argv[0]);

	struct option *long_options = extended_to_longopts(options);
	char short_options[] = "u:t:q:A:H:U:D:h";
	while (1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, short_options, long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 'u': //updatefile
				string_array_add(&tle_update_filenames, optarg);
				break;
			case FLYBY_OPT_ADD_TLE: //add TLE file
				string_array_add(&tle_add_filenames, optarg);
				break;
			case 't': //tlefile
				string_array_add(&tle_cmd_filenames, optarg);
				break;
			case 'q': //qth
				strncpy(qth_filename, optarg, MAX_NUM_CHARS);
				qth_cmd_filename_set = true;
				break;
			case 'A': //rotctl
				use_rotctl = true;
				strncpy(rotctld_host, optarg, MAX_NUM_CHARS);
				break;
			case FLYBY_OPT_ROTCTLD_PORT: //rotctl port
				strncpy(rotctld_port, optarg, MAX_NUM_CHARS);
				break;
			case 'H': //horizon
				tracking_horizon = strtod(optarg, NULL);
				break;
			case FLYBY_OPT_ROTCTLD_UPDATE_INTERVAL: //once per second-option
				rotctld_update_interval = strtod(optarg, NULL);
				break;
			case 'U': //uplink
				use_rigctld_uplink = true;
				strncpy(rigctld_uplink_host, optarg, MAX_NUM_CHARS);
				break;
			case FLYBY_OPT_UPLINK_PORT: //uplink port
				strncpy(rigctld_uplink_port, optarg, MAX_NUM_CHARS);
				break;
			case FLYBY_OPT_UPLINK_VFO: //uplink vfo
				strncpy(rigctld_uplink_vfo, optarg, MAX_NUM_CHARS);
				break;
			case 'D': //downlink
				use_rigctld_downlink = true;
				strncpy(rigctld_downlink_host, optarg, MAX_NUM_CHARS);
				break;
			case FLYBY_OPT_DOWNLINK_PORT: //downlink port
				strncpy(rigctld_downlink_port, optarg, MAX_NUM_CHARS);
				break;
			case FLYBY_OPT_DOWNLINK_VFO: //downlink vfo
				strncpy(rigctld_downlink_vfo, optarg, MAX_NUM_CHARS);
				break;
			case 'h': //help
				getopt_long_show_help(usage_instructions, options, short_options);
				return 0;
				break;
		}
	}

	//add TLE files to XDG data home and exit
	int num_tle_add_files = string_array_size(&tle_add_filenames);
	if (num_tle_add_files > 0) {
		create_xdg_dirs();
		char *xdg_home = xdg_data_home();
		for (int i=0; i < num_tle_add_files; i++) {
			struct tle_db *temp_db = tle_db_create();
			char *orig_filename = strdup(string_array_get(&tle_add_filenames, i));
			int retval = tle_db_from_file(orig_filename, temp_db);
			if (retval != -1) {
				char *base_filename = basename(orig_filename);
				char out_filename[MAX_NUM_CHARS];
				snprintf(out_filename, MAX_NUM_CHARS, "%s%s%s", xdg_home, TLE_RELATIVE_DIR_PATH, base_filename);
				tle_db_to_file(out_filename, temp_db);
				fprintf(stderr, "Copy `%s` to `%s` (%ld TLEs)\n", orig_filename, out_filename, temp_db->num_tles);
			} else {
				fprintf(stderr, "TLE file `%s` could not be loaded.\n", orig_filename);
			}
			tle_db_destroy(&temp_db);
			free(orig_filename);
		}
		free(xdg_home);
		return 0;
	}

	//read TLE database
	struct tle_db *tle_db = tle_db_create();
	int num_cmd_tle_files = string_array_size(&tle_cmd_filenames);
	if (num_cmd_tle_files > 0) {
		//TLEs are read from files specified on the command line
		for (int i=0; i < num_cmd_tle_files; i++) {
			struct tle_db *temp_db = tle_db_create();
			int retval = tle_db_from_file(string_array_get(&tle_cmd_filenames, i), temp_db);
			if (retval != -1) {
				tle_db_merge(temp_db, tle_db, TLE_OVERWRITE_OLD);
			} else {
				fprintf(stderr, "TLE file %s could not be loaded, exiting.\n", string_array_get(&tle_cmd_filenames, i));
				return 1;
			}
			tle_db_destroy(&temp_db);
		}
	} else {
		//TLEs are read from XDG dirs
		tle_db_from_search_paths(tle_db);
	}

	whitelist_from_search_paths(tle_db);

	//use tle update files to update the TLE database, if present
	int num_update_files = string_array_size(&tle_update_filenames);
	if (num_update_files > 0) {
		for (int i=0; i < num_update_files; i++) {
			printf("Updating TLE database using %s:\n\n", string_array_get(&tle_update_filenames, i));
			update_tle_database(string_array_get(&tle_update_filenames, i), tle_db);
			printf("\n");
		}
		string_array_free(&tle_update_filenames);
		return 0;
	}

	//connect to rotctld
	rotctld_info_t rotctld = {0};
	if (use_rotctl) {
		rotctld_connect(rotctld_host, rotctld_port, &rotctld);
		rotctld_set_update_interval(&rotctld, rotctld_update_interval);
		rotctld_set_tracking_horizon(&rotctld, tracking_horizon);
	}

	//check rigctld input arguments
	if (use_rigctld_uplink && use_rigctld_downlink) {
		if ((strncmp(rigctld_uplink_host, rigctld_downlink_host, MAX_NUM_CHARS) == 0) &&
		   (strncmp(rigctld_uplink_port, rigctld_downlink_port, MAX_NUM_CHARS) == 0) &&
		   ((strncmp(rigctld_uplink_vfo, rigctld_downlink_vfo, MAX_NUM_CHARS) == 0) ||
		   (strlen(rigctld_uplink_vfo) == 0) || strlen(rigctld_downlink_vfo) == 0)) {
			fprintf(stderr, "VFO names must be specified when the same rigctld instance is used for both uplink and downlink.\n"
					"Downlink input:\t%s:%s, VFO=\"%s\"\nUplink input:\t%s:%s, VFO=\"%s\"\n",
					rigctld_downlink_host, rigctld_downlink_port, rigctld_downlink_vfo, rigctld_uplink_host, rigctld_uplink_port, rigctld_uplink_vfo);
			return 1;
		}
	}

	//connect to rigctld
	rigctld_info_t uplink = {0};
	if (use_rigctld_uplink) {
		rigctld_connect(rigctld_uplink_host, rigctld_uplink_port, rigctld_uplink_vfo, &uplink);
	}
	rigctld_info_t downlink = {0};
	if (use_rigctld_downlink) {
		rigctld_connect(rigctld_downlink_host, rigctld_downlink_port, rigctld_downlink_vfo, &downlink);
	}

	//read flyby config files
	predict_observer_t *observer = predict_create_observer("", 0, 0, 0);
	bool is_new_user = false;

	if (qth_cmd_filename_set) {
		int retval = qth_from_file(qth_filename, observer);
		if (retval != 0) {
			fprintf(stderr, "QTH file %s could not be loaded.\n", qth_filename);
			return 1;
		}
	} else {
		is_new_user = qth_from_search_paths(observer) != QTH_FILE_HOME;
		char *temp = qth_default_writepath();
		strncpy(qth_filename, temp, MAX_NUM_CHARS);
		free(temp);
	}

	struct transponder_db *transponder_db = transponder_db_create(tle_db);
	transponder_db_from_search_paths(tle_db, transponder_db);

	run_flyby_curses_ui(is_new_user, qth_filename, observer, tle_db, transponder_db, &rotctld, &downlink, &uplink);

	//disconnect from rigctl and rotctl
	rigctld_disconnect(&downlink);
	rigctld_disconnect(&uplink);
	rotctld_disconnect(&rotctld);

	//free memory
	predict_destroy_observer(observer);
	tle_db_destroy(&tle_db);
	transponder_db_destroy(&transponder_db);
	free(long_options);
}
