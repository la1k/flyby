#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>


#define OPT_ROTCTL_UPDATE_INTERVAL 200
#define OPT_ROTCTL_PORT 201
#define OPT_UPLINK_PORT 202
#define OPT_UPLINK_VFO 203
#define OPT_DOWNLINK_PORT 204
#define OPT_DOWNLINK_VFO 205
#define OPT_LONGITUDE 206
#define OPT_LATITUDE 207

void show_help(const char *name, struct option long_options[], const char *short_options)
{
	printf("\nUsage:\n");
	printf("%s [options]\n\n", name);
	printf("Options:\n");
	int index = 0;
	while (true) {
		if (long_options[index].name == 0) {
			break;
		}

		//display short option
		if (short_options[index] != ':') {
			printf(" -%c,", short_options[index]);
		} else {
			printf("    ");
		}
		
		//display long option
		printf("--%s", long_options[index].name);
		switch (long_options[index].val) {
			case 'u':
				printf("=FILE\t\tupdate TLE database with TLE file FILE");
				break;
			case 't':
				printf("=FILE\t\t\tuse FILE as TLE file");
				break;
			case 'q':
				printf("=FILE\t\t\tuse FILE as QTH file");
				break;
			case 'a':
				printf("=SERVER_HOST\t\tconnect to a rotctl server and enable antenna tracking");
				break;
			case OPT_ROTCTL_UPDATE_INTERVAL:
				printf("=SECONDS\tsend azimuth/elevation to rotctl at specified interval SECONDS instead of when they change");
				break;
			case OPT_ROTCTL_PORT:
				printf("=SERVER_PORT\t\tspecify rotctl server port");
				break;
			case 'H':
				printf("=HORIZON\t\t\tspecify horizon threshold for when flyby will start tracking an orbit");
				break;
			case 'U':
				printf("=SERVER_HOST\t\tconnect to specified rigctl server for uplink frequency steering");
				break;
			case OPT_UPLINK_PORT:
				printf("=SERVER_PORT\t\tspecify rigctl uplink port");
				break;
			case OPT_UPLINK_VFO:
				printf("=VFO_NAME\t\tspecify rigctl uplink VFO");
				break;
			case 'D':
				printf("=SERVER_HOST\t\tconnect to specified rigctl server for downlink frequency steering");
				break;
			case OPT_DOWNLINK_PORT:
				printf("=SERVER_PORT\t\tspecify rigctl downlink port");
				break;
			case OPT_DOWNLINK_VFO:
				printf("=VFO_NAME\t\tspecify rigctl downlink VFO");
				break;
			case OPT_LONGITUDE:
				printf("=EAST/WEST\t\tspecify longitude display convention. Defaults to EAST");

				break;
			case OPT_LATITUDE:
				printf("=NORTH/SOUTH\t\tspecify latitude display convention. Defaults to NORTH");
				break;
			case 'h':
				printf("\t\t\t\tShow help");
				break;
		}
		index++;
		printf("\n");
	}
}

int main (int argc, char **argv)
{
	static int verbose_flag = 0;
	int c;
	static struct option long_options[] = {
		{"update-tle-db",		required_argument,	0,	'u'},
		{"tle-file",			required_argument,	0,	't'},
		{"qth-file",			required_argument,	0,	'q'},
		{"rotctl",			required_argument,	0,	'a'},
		{"rotctl-update-interval",	required_argument,	0,	OPT_ROTCTL_UPDATE_INTERVAL},
		{"rotctl-port",			required_argument,	0,	OPT_ROTCTL_PORT},
		{"horizon",			required_argument,	0,	'H'},
		{"uplink",			required_argument,	0,	'U'},
		{"uplink-port",			required_argument,	0,	OPT_UPLINK_PORT},
		{"uplink-vfo",			required_argument,	0,	OPT_UPLINK_VFO},
		{"downlink",			required_argument,	0,	'D'},
		{"downlink-port",		required_argument,	0,	OPT_DOWNLINK_PORT},
		{"downlink-vfo",		required_argument,	0,	OPT_DOWNLINK_VFO},
		{"longitude",			required_argument,	0,	OPT_LONGITUDE},
		{"latitude",			required_argument,	0,	OPT_LATITUDE},
		{"help",			no_argument,		0,	'h'},
		{0, 0, 0, 0}
	};
	char short_options[] = "utqa::HU::D::::h";

	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, short_options, long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 'u': //updatefile
				break;
			case 't': //tlefile
				break;
			case 'q': //qth
				break;
			case 'a': //rotctl
				break;
			case OPT_ROTCTL_UPDATE_INTERVAL: //rotctl update interval
				break;
			case OPT_ROTCTL_PORT: //rotctl port
				break;
			case 'H': //horizon
				break;
			case 'U': //uplink
				break;
			case OPT_UPLINK_PORT: //uplink port
				break;
			case OPT_UPLINK_VFO: //uplink vfo
				break;
			case 'D': //downlink
				break;
			case OPT_DOWNLINK_PORT: //downlink port
				break;
			case OPT_DOWNLINK_VFO: //downlink vfo
				break;
			case OPT_LONGITUDE: //longitude
				break;
			case OPT_LATITUDE: //latitude
				break;
			case 'h': //help
				show_help(argv[0], long_options, short_options);
				break;
		}
	}
}
