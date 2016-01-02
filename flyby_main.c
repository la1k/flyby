#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdbool.h>
#include "flyby_defines.h"
#include "flyby_hamlib.h"

//longopt value identificators for command line options without shorthand
#define FLYBY_OPT_ROTCTLD_PORT 201
#define FLYBY_OPT_UPLINK_PORT 202
#define FLYBY_OPT_UPLINK_VFO 203
#define FLYBY_OPT_DOWNLINK_PORT 204
#define FLYBY_OPT_DOWNLINK_VFO 205

/**
 * Print flyby program usage to stdout.
 *
 * \param program_name Name of program, use argv[0]
 * \param long_options List of long options used in getopts_long
 * \param short_options List of short options used in getopts_long
 **/
void show_help(const char *program_name, struct option long_options[], const char *short_options);

/**
 * Dynamic size string array for the situations where we don't know in advance the number of filenames.
 * Used for accumulating TLE filenames. Exchange by std::vector if we start using C++ instead. :^)
 **/
typedef struct {
	int available_size; //available size within string array
	int num_strings; //current number of strings
	char **strings; //strings
} string_array_t;

/**
 * Add string to string array. Reallocates available space to twice the size when current available size is exceeded.
 *
 * \param string_array String array
 * \param string String to add
 * \return 0 on success, -1 on failure
 **/
int string_array_add(string_array_t *string_array, const char *string);

/**
 * Get string at specified index from string array.
 *
 * \param string_array String array
 * \param index Index at which we want to extract a string
 * \return String at specified index
 **/
const char* string_array_get(string_array_t *string_array, int index);

/**
 * Get string array size.
 *
 * \param string_array String array
 * \return Size
 **/
int string_array_size(string_array_t *string_array);

/**
 * Free memory allocated in string array.
 *
 * \param string_array String array to free
 **/
void string_array_free(string_array_t *string_array);

//int flyby_read_tle_files(string_array_t tle_files, struct tle_db *ret_db);
//int flyby_read_qth_file(const char *qth_file, predict_observer_t *ret_observer);
//int flyby_read_transponder_db(const char *db_file, struct transponder_db *ret_db);

int main (int argc, char **argv)
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
	char rigctld_downlink_port[MAX_NUM_CHARS] = RIGCTLD_DOWNLINK_DEFAULT_HOST;
	char rigctld_downlink_vfo[MAX_NUM_CHARS] = {0};

	//config files
	char qth_filename[MAX_NUM_CHARS] = {0};
	char db_filename[MAX_NUM_CHARS] = {0};
	char tle_filename[MAX_NUM_CHARS] = {0};

	//read config filenames
	//TODO: To be replaced with config paths from XDG-standard, issue #1.
	char *env = getenv("HOME");
	snprintf(qth_filename, MAX_NUM_CHARS, "%s/.flyby/flyby.qth", env);
	snprintf(db_filename, MAX_NUM_CHARS, "%s/.flyby/flyby.db", env);
	snprintf(tle_filename, MAX_NUM_CHARS, "%s/.flyby/flyby.tle", env);

	string_array_t tle_update_filenames = {0}; //TLE files to be used to update the TLE databases

	//command line options
	struct option long_options[] = {
		{"update-tle-db",		required_argument,	0,	'u'},
		{"tle-file",			required_argument,	0,	't'},
		{"qth-file",			required_argument,	0,	'q'},
		{"rotctl",			required_argument,	0,	'a'},
		{"rotctld-port",		required_argument,	0,	FLYBY_OPT_ROTCTLD_PORT},
		{"horizon",			required_argument,	0,	'H'},
		{"rigctld-uplink",		required_argument,	0,	'U'},
		{"rigctld-uplink-port",		required_argument,	0,	FLYBY_OPT_UPLINK_PORT},
		{"rigctld-uplink-vfo",		required_argument,	0,	FLYBY_OPT_UPLINK_VFO},
		{"rigctld-downlink",		required_argument,	0,	'D'},
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
				strncpy(tle_filename, optarg, MAX_NUM_CHARS);
				break;
			case 'q': //qth
				strncpy(qth_filename, optarg, MAX_NUM_CHARS);
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
				break;
		}
	}

	//use tle update files to update the TLE database
	int num_update_files = string_array_size(&tle_update_filenames);
	if (num_update_files > 0) {
		for (int i=0; i < num_update_files; i++) {
			printf("Updating TLE database using %s.\n", string_array_get(&tle_update_filenames, i));
			//FIXME: AutoUpdate(something something, string_array_get(&tle_update_filenames, i));
		}
		string_array_free(&tle_update_filenames);
		return 0;
	}

	//connect to rotctld
	rotctld_info_t rotctld = {0};
	if (use_rotctl) {
		rotctld_connect(rotctld_host, rotctld_port, &rotctld);
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


	//disconnect from rigctl and rotctl
	rigctld_disconnect(&downlink);
	rigctld_disconnect(&uplink);
	rotctld_disconnect(&rotctld);
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
				printf("=FILE\t\t\tuse FILE as TLE database file. Overrides user and system TLE database files. Only a single file is supported.");
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
				printf("=HORIZON\t\t\tspecify elevation threshold for when %s will start tracking an orbit", name);
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
		}
		index++;
		printf("\n");
	}
}

int string_array_add(string_array_t *string_array, const char *string)
{
	//initialize
	if (string_array->available_size < 1) {
		string_array->strings = (char**)malloc(sizeof(char*));
		string_array->available_size = 1;
		string_array->num_strings = 0;
	}

	//extend size to twice the current size
	if (string_array->num_strings+1 > string_array->available_size) {
		char **temp = realloc(string_array->strings, sizeof(char*)*string_array->available_size*2);
		if (temp == NULL) {
			return -1;
		}
		string_array->available_size = string_array->available_size*2;
		string_array->strings = temp;
	}

	//copy string
	string_array->strings[string_array->num_strings] = (char*)malloc(sizeof(char)*MAX_NUM_CHARS);
	strncpy(string_array->strings[string_array->num_strings], string, MAX_NUM_CHARS);
	string_array->num_strings++;
	return 0;
}

const char* string_array_get(string_array_t *string_array, int index)
{
	if (index < string_array->num_strings) {
		return string_array->strings[index];
	}
	return NULL;
}

void string_array_free(string_array_t *string_array)
{
	for (int i=0; i < string_array->num_strings; i++) {
		free(string_array->strings[i]);
	}
	free(string_array->strings);
	string_array->num_strings = 0;
	string_array->available_size = 0;
	string_array->strings = NULL;
}

int string_array_size(string_array_t *string_array)
{
	return string_array->num_strings;
}
