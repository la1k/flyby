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
#include "transponder_db.h"

//longopt value identificators for command line options without shorthand
#define FLYBY_OPT_ROTCTLD_PORT 201
#define FLYBY_OPT_UPLINK_PORT 202
#define FLYBY_OPT_UPLINK_VFO 203
#define FLYBY_OPT_DOWNLINK_PORT 204
#define FLYBY_OPT_DOWNLINK_VFO 205
#define FLYBY_OPT_ROTCTLD_ONCE_PER_SECOND 206

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
	bool rotctld_once_per_second = false;
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
	string_array_t tle_update_filenames = {0}; //TLE files to be used to update the TLE databases
	string_array_t tle_cmd_filenames = {0}; //TLE files supplied on the command line

	char qth_filename[MAX_NUM_CHARS] = {0};
	bool qth_cmd_filename_set = false;

	//command line options
	struct option long_options[] = {
		{"update-tle-db",		required_argument,	0,	'u'},
		{"tle-file",			required_argument,	0,	't'},
		{"qth-file",			required_argument,	0,	'q'},
		{"rotctld-host",			required_argument,	0,	'a'},
		{"rotctld-port",		required_argument,	0,	FLYBY_OPT_ROTCTLD_PORT},
		{"rotctld-horizon",		required_argument,	0,	'H'},
		{"rotctld-once-per-second",	no_argument,		0,	FLYBY_OPT_ROTCTLD_ONCE_PER_SECOND},
		{"rigctld-uplink-host",		required_argument,	0,	'U'},
		{"rigctld-uplink-port",		required_argument,	0,	FLYBY_OPT_UPLINK_PORT},
		{"rigctld-uplink-vfo",		required_argument,	0,	FLYBY_OPT_UPLINK_VFO},
		{"rigctld-downlink-host",		required_argument,	0,	'D'},
		{"rigctld-downlink-port",	required_argument,	0,	FLYBY_OPT_DOWNLINK_PORT},
		{"rigctld-downlink-vfo",	required_argument,	0,	FLYBY_OPT_DOWNLINK_VFO},
		{"help",			no_argument,		0,	'h'},
		{0, 0, 0, 0}
	};
	char short_options[] = "u:t:q:a:H:U:D:h";
	while (1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, short_options, long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 'u': //updatefile
				string_array_add(&tle_update_filenames, optarg);
				break;
			case 't': //tlefile
				string_array_add(&tle_cmd_filenames, optarg);
				break;
			case 'q': //qth
				strncpy(qth_filename, optarg, MAX_NUM_CHARS);
				qth_cmd_filename_set = true;
				break;
			case 'a': //rotctl
				use_rotctl = true;
				strncpy(rotctld_host, optarg, MAX_NUM_CHARS);
				break;
			case FLYBY_OPT_ROTCTLD_PORT: //rotctl port
				strncpy(rotctld_port, optarg, MAX_NUM_CHARS);
				break;
			case 'H': //horizon
				tracking_horizon = strtod(optarg, NULL);
				break;
			case FLYBY_OPT_ROTCTLD_ONCE_PER_SECOND: //once per second-option
				rotctld_once_per_second = true;
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
				show_help(argv[0], long_options, short_options);
				return 0;
				break;
		}
	}

	//read TLE database
	struct tle_db tle_db = {0};
	int num_cmd_tle_files = string_array_size(&tle_cmd_filenames);
	if (num_cmd_tle_files > 0) {
		//TLEs are read from files specified on the command line
		for (int i=0; i < num_cmd_tle_files; i++) {
			struct tle_db temp_db = {0};
			int retval = tle_db_from_file(string_array_get(&tle_cmd_filenames, i), &temp_db);
			if (retval != -1) {
				tle_db_merge(&temp_db, &tle_db, TLE_OVERWRITE_OLD);
			} else {
				fprintf(stderr, "TLE file %s could not be loaded, exiting.\n", string_array_get(&tle_cmd_filenames, i));
				return 1;
			}
		}
	} else {
		//TLEs are read from XDG dirs
		tle_db_from_search_paths(&tle_db);
	}

	//use tle update files to update the TLE database, if present
	int num_update_files = string_array_size(&tle_update_filenames);
	if (num_update_files > 0) {
		for (int i=0; i < num_update_files; i++) {
			printf("Updating TLE database using %s:\n\n", string_array_get(&tle_update_filenames, i));
			AutoUpdate(string_array_get(&tle_update_filenames, i), &tle_db, NULL);
			printf("\n");
		}
		string_array_free(&tle_update_filenames);
		return 0;
	}

	//connect to rotctld
	rotctld_info_t rotctld = {0};
	if (use_rotctl) {
		rotctld_connect(rotctld_host, rotctld_port, rotctld_once_per_second, tracking_horizon, &rotctld);
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

	struct transponder_db transponder_db = {0};
	transponder_db_from_search_paths(&tle_db, &transponder_db);

	RunFlybyUI(is_new_user, qth_filename, observer, &tle_db, &transponder_db, &rotctld, &downlink, &uplink);

	//disconnect from rigctl and rotctl
	rigctld_disconnect(&downlink);
	rigctld_disconnect(&uplink);
	rotctld_disconnect(&rotctld);

	//free memory
	predict_destroy_observer(observer);
}

/**
 * Returns true if specified option's value is a char within the short options char array (and has thus a short-hand version of the long option)
 *
 * \param short_options Array over short option chars
 * \param long_option Option struct to check
 * \returns True if option has a short option, false otherwise
 **/
bool has_short_option(const char *short_options, struct option long_option)
{
	const char *ptr = strchr(short_options, long_option.val);
	if (ptr == NULL) {
		return false;
	}
	return true;
}

void show_help(const char *name, struct option long_options[], const char *short_options)
{
	//display initial description
	printf("\nUsage:\n");
	printf("%s [options]\n\n", name);
	printf("Options:\n");

	int index = 0;
	while (true) {
		if (long_options[index].name == 0) {
			break;
		}

		//display short option
		if (has_short_option(short_options, long_options[index])) {
			printf(" -%c,", long_options[index].val);
		} else {
			printf("    ");
		}
		
		//display long option
		printf("--%s", long_options[index].name);

		//display usage information
		switch (long_options[index].val) {
			case 'u':
				printf("=FILE\t\tupdate TLE database with TLE file FILE. Multiple files can be specified using the same option multiple times (e.g. -u file1 -u file2 ...). %s will exit afterwards", name);
				break;
			case 't':
				printf("=FILE\t\t\tuse FILE as TLE database file. Overrides user and system TLE database files. Multiple files can be specified using this option multiple times (e.g. -t file1 -t file2 ...).");
				break;
			case 'q':
				printf("=FILE\t\t\tuse FILE as QTH config file. Overrides existing QTH config file");
				break;
			case 'a':
				printf("=SERVER_HOST\t\tconnect to a rotctld server with hostname SERVER_HOST and enable antenna tracking");
				break;
			case FLYBY_OPT_ROTCTLD_PORT:
				printf("=SERVER_PORT\t\tspecify rotctld server port");
				break;
			case 'H':
				printf("=HORIZON\t\tspecify elevation threshold for when %s will start tracking an orbit", name);
				break;
			case FLYBY_OPT_ROTCTLD_ONCE_PER_SECOND:
				printf("\t\tSend updates to rotctld once per second instead of when (azimuth,elevation) changes");
				break;
			case 'U':
				printf("=SERVER_HOST\tconnect to specified rigctld server for uplink frequency steering");
				break;
			case FLYBY_OPT_UPLINK_PORT:
				printf("=SERVER_PORT\tspecify rigctld uplink port");
				break;
			case FLYBY_OPT_UPLINK_VFO:
				printf("=VFO_NAME\tspecify rigctld uplink VFO");
				break;
			case 'D':
				printf("=SERVER_HOST\tconnect to specified rigctld server for downlink frequency steering");
				break;
			case FLYBY_OPT_DOWNLINK_PORT:
				printf("=SERVER_PORT\tspecify rigctld downlink port");
				break;
			case FLYBY_OPT_DOWNLINK_VFO:
				printf("=VFO_NAME\tspecify rigctld downlink VFO");
				break;
			case 'h':
				printf("\t\t\t\tShow help");
				break;
			default:
				printf("DOCUMENTATION MISSING\n");
				break;
		}
		index++;
		printf("\n");
	}
}
