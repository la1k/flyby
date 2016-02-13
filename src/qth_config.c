#include "qth_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xdg_basedirs.h"
#include "defines.h"
#include "string_array.h"
#include <math.h>

enum qth_file_state qth_from_search_paths(predict_observer_t *ret_observer)
{
	//try to read QTH file from user home
	char *config_home = xdg_config_home();
	char qth_path[MAX_NUM_CHARS] = {0};
	snprintf(qth_path, MAX_NUM_CHARS, "%s%s", config_home, QTH_RELATIVE_FILE_PATH);
	int readval = qth_from_file(qth_path, ret_observer);
	free(config_home);

	if (readval != 0) {
		//try to read from system default
		char *config_dirs = xdg_config_dirs();
		string_array_t dirs = {0};
		stringsplit(config_dirs, &dirs);
		bool qth_file_found = false;

		for (int i=0; i < string_array_size(&dirs); i++) {
			snprintf(qth_path, MAX_NUM_CHARS, "%s%s", string_array_get(&dirs, i), QTH_RELATIVE_FILE_PATH);
			if (qth_from_file(qth_path, ret_observer) == 0) {
				qth_file_found = true;
				break;
			}
		}
		free(config_dirs);

		if (qth_file_found) {
			return QTH_FILE_SYSTEMWIDE;
		} else {
			return QTH_FILE_NOTFOUND;
		}
	}
	return QTH_FILE_HOME;
}

char* qth_default_writepath()
{
	create_xdg_dirs();

	char *config_home = xdg_config_home();
	char *qth_path = (char*)malloc(sizeof(char)*MAX_NUM_CHARS);
	snprintf(qth_path, MAX_NUM_CHARS, "%s%s", config_home, QTH_RELATIVE_FILE_PATH);
	free(config_home);

	return qth_path;
}

void qth_to_file(const char *qth_path, predict_observer_t *qth)
{
	FILE *fd;

	fd=fopen(qth_path,"w");

	fprintf(fd,"%s\n",qth->name);
	fprintf(fd," %g\n",qth->latitude*180.0/M_PI);
	fprintf(fd," %g\n",-qth->longitude*180.0/M_PI); //convert from N/E to N/W
	fprintf(fd," %d\n",(int)floor(qth->altitude));

	fclose(fd);
}

int qth_from_file(const char *qthfile, predict_observer_t *observer)
{
	//copied from ReadDataFiles().

	char callsign[MAX_NUM_CHARS];
	FILE *fd=fopen(qthfile,"r");
	if (fd!=NULL) {
		fgets(callsign,16,fd);
		callsign[strlen(callsign)-1]=0;

		double latitude, longitude;
		int altitude;
		fscanf(fd,"%lf", &latitude);
		fscanf(fd,"%lf", &longitude);
		fscanf(fd,"%d", &altitude);
		fclose(fd);

		strncpy(observer->name, callsign, 16);
		observer->latitude = latitude*M_PI/180.0;
		observer->longitude = -longitude*M_PI/180.0; //convert from N/W to N/E
		observer->altitude = altitude*M_PI/180.0;
	} else {
		return -1;
	}
	return 0;
}
