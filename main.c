#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdbool.h>

#define MAX_NUM_CHARS 1024


//values for command line options without shorthand
#define FLYBY_OPT_ROTCTL_PORT 201
#define FLYBY_OPT_UPLINK_PORT 202
#define FLYBY_OPT_UPLINK_VFO 203
#define FLYBY_OPT_DOWNLINK_PORT 204
#define FLYBY_OPT_DOWNLINK_VFO 205

/**
 * Returns true if specified option's value is a char within the short options char array (and has thus a short-hand version of the long option)
 *
 * \param short_options Array over short option chars
 * \param long_option Option struct to check
 * \returns True if option has a short option, false otherwise
 **/
bool has_short_option(const char *short_options, struct option long_option) {
	const char *ptr = strchr(short_options, long_option.val);
	if (ptr == NULL) {
		return false;
	}
	return true;
}

/**
 * Print flyby program usage to stdout. Option descriptions are set here.
 *
 * \param name Name of program, use argv[0]
 * \param long_options List of long options used in getopts_long
 * \param short_options List of short options used in getopts_long
 **/
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
				printf("=FILE\t\tupdate TLE database with TLE file FILE");
				break;
			case 't':
				printf("=FILE\t\t\tuse FILE as TLE database file. Overrides user and system TLE database files");
				break;
			case 'q':
				printf("=FILE\t\t\tuse FILE as QTH config file. Overrides existing QTH config file");
				break;
			case 'a':
				printf("=SERVER_HOST\t\tconnect to a rotctl server with hostname SERVER_HOST and enable antenna tracking");
				break;
			case FLYBY_OPT_ROTCTL_PORT:
				printf("=SERVER_PORT\t\tspecify rotctl server port");
				break;
			case 'H':
				printf("=HORIZON\t\t\tspecify horizon threshold for when %s will start tracking an orbit", name);
				break;
			case 'U':
				printf("=SERVER_HOST\tconnect to specified rigctl server for uplink frequency steering");
				break;
			case FLYBY_OPT_UPLINK_PORT:
				printf("=SERVER_PORT\tspecify rigctl uplink port");
				break;
			case FLYBY_OPT_UPLINK_VFO:
				printf("=VFO_NAME\tspecify rigctl uplink VFO");
				break;
			case 'D':
				printf("=SERVER_HOST\tconnect to specified rigctl server for downlink frequency steering");
				break;
			case FLYBY_OPT_DOWNLINK_PORT:
				printf("=SERVER_PORT\tspecify rigctl downlink port");
				break;
			case FLYBY_OPT_DOWNLINK_VFO:
				printf("=VFO_NAME\tspecify rigctl downlink VFO");
				break;
			case 'h':
				printf("\t\t\t\tShow help");
				break;
		}
		index++;
		printf("\n");
	}
}

#define ROTCTL_DEFAULT_HOST "localhost"
#define ROTCTL_DEFAULT_PORT "4533\0\0"
#define RIGCTL_UPLINK_DEFAULT_HOST "localhost"
#define RIGCTL_UPLINK_DEFAULT_PORT "4532\0\0"
#define RIGCTL_DOWNLINK_DEFAULT_HOST "localhost"
#define RIGCTL_DOWNLINK_DEFAULT_PORT "4532\0\0"

int main (int argc, char **argv)
{
	//rotctl options
	bool use_rotctl = false;
	char rotctl_host[MAX_NUM_CHARS] = ROTCTL_DEFAULT_HOST;
	char rotctl_port[MAX_NUM_CHARS] = ROTCTL_DEFAULT_PORT;
	bool rotctl_once_per_second = false;
	double tracking_horizon = 0;

	//rigctl uplink options
	bool use_rigctl_uplink = false;
	char rigctl_uplink_host[MAX_NUM_CHARS] = RIGCTL_UPLINK_DEFAULT_HOST;
	char rigctl_uplink_port[MAX_NUM_CHARS] = RIGCTL_UPLINK_DEFAULT_PORT;
	char rigctl_uplink_vfo[MAX_NUM_CHARS] = {0};
	
	//rigctl downlink options
	bool use_rigctl_downlink = false;
	char rigctl_downlink_host[MAX_NUM_CHARS] = RIGCTL_DOWNLINK_DEFAULT_HOST;
	char rigctl_downlink_port[MAX_NUM_CHARS] = RIGCTL_DOWNLINK_DEFAULT_HOST;
	char rigctl_downlink_vfo[MAX_NUM_CHARS] = {0};

	//files
	char qth_config_filename[MAX_NUM_CHARS] = {0};
	char **tle_filenames = NULL;
	char **tle_update_filenames = NULL;


	//command line options
	struct option long_options[] = {
		{"update-tle-db",		required_argument,	0,	'u'},
		{"tle-file",			required_argument,	0,	't'},
		{"qth-file",			required_argument,	0,	'q'},
		{"rotctl",			required_argument,	0,	'a'},
		{"rotctl-port",			required_argument,	0,	FLYBY_OPT_ROTCTL_PORT},
		{"horizon",			required_argument,	0,	'H'},
		{"rigctl-uplink",		required_argument,	0,	'U'},
		{"rigctl-uplink-port",		required_argument,	0,	FLYBY_OPT_UPLINK_PORT},
		{"rigctl-uplink-vfo",		required_argument,	0,	FLYBY_OPT_UPLINK_VFO},
		{"rigctl-downlink",		required_argument,	0,	'D'},
		{"rigctl-downlink-port",	required_argument,	0,	FLYBY_OPT_DOWNLINK_PORT},
		{"rigctl-downlink-vfo",		required_argument,	0,	FLYBY_OPT_DOWNLINK_VFO},
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
				break;
			case 't': //tlefile
				break;
			case 'q': //qth
				strncpy(qth_config_filename, optarg, MAX_NUM_CHARS);
				break;
			case 'a': //rotctl
				use_rotctl = true;
				strncpy(rotctl_host, optarg, MAX_NUM_CHARS);
				break;
			case FLYBY_OPT_ROTCTL_PORT: //rotctl port
				strncpy(rotctl_port, optarg, MAX_NUM_CHARS);
				break;
			case 'H': //horizon
				tracking_horizon = strtod(optarg, NULL);
				break;
			case 'U': //uplink
				use_rigctl_uplink = true;
				strncpy(rigctl_uplink_host, optarg, MAX_NUM_CHARS);
				break;
			case FLYBY_OPT_UPLINK_PORT: //uplink port
				strncpy(rigctl_uplink_port, optarg, MAX_NUM_CHARS);
				break;
			case FLYBY_OPT_UPLINK_VFO: //uplink vfo
				strncpy(rigctl_uplink_vfo, optarg, MAX_NUM_CHARS);
				break;
			case 'D': //downlink
				use_rigctl_downlink = true;
				strncpy(rigctl_downlink_host, optarg, MAX_NUM_CHARS);
				break;
			case FLYBY_OPT_DOWNLINK_PORT: //downlink port
				strncpy(rigctl_downlink_port, optarg, MAX_NUM_CHARS);
				break;
			case FLYBY_OPT_DOWNLINK_VFO: //downlink vfo
				strncpy(rigctl_downlink_vfo, optarg, MAX_NUM_CHARS);
				break;
			case 'h': //help
				show_help(argv[0], long_options, short_options);
				break;
		}
	}
}
