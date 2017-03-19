#include "transponder_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xdg_basedirs.h"
#include "string_array.h"

struct transponder_db *transponder_db_create()
{
	struct transponder_db *transponder_db = (struct transponder_db*) malloc(sizeof(struct transponder_db));
	memset((void*)transponder_db, 0, sizeof(struct transponder_db));
	return transponder_db;
}

void transponder_db_destroy(struct transponder_db **transponder_db)
{
	free(*transponder_db);
	*transponder_db = NULL;
}

int transponder_db_from_file(const char *dbfile, const struct tle_db *tle_db, struct transponder_db *ret_db, enum sat_db_location location_info)
{
	ret_db->num_sats = tle_db->num_tles;
	FILE *fd = fopen(dbfile,"r");
	if (fd == NULL) {
		return -1;
	}

	char templine[MAX_NUM_CHARS];

	//NOTE: The database file format is the one used in Predict, with
	//redundant fields like orbital schedule. Kept for legacy reasons, but
	//might change at some point in the future when we find new fields we
	//want to define, and have no reason to retain backwards-compatibility
	//with Predict.

	while (!feof(fd)) {
		long satellite_number;
		struct sat_db_entry new_entry = {0};

		//satellite name. Ignored, present in database for readability reasons
		fgets(templine, MAX_NUM_CHARS, fd);
		if (strncmp(templine, "end", 3) == 0) {
			break;
		}

		//satellite category number
		fgets(templine, MAX_NUM_CHARS, fd);
		sscanf(templine,"%ld",&satellite_number);

		//attitude longitude and attitude latitude, for squint angle calculation
		fgets(templine, MAX_NUM_CHARS, fd);
		if (strncmp(templine,"No",2)!=0) {
			sscanf(templine,"%lf, %lf",&(new_entry.alat), &(new_entry.alon));
			new_entry.squintflag=1;
		} else {
			new_entry.squintflag=0;
		}

		//get transponders
		int transponder_index = 0;
		while (!feof(fd)) {
			fgets(templine, MAX_NUM_CHARS, fd);
			if (strncmp(templine, "end", 3) == 0) {
				//end transponder entries, move to next satellite
				break;
			}

			//transponder name
			templine[strlen(templine)-1] = '\0'; //remove newline
			strncpy(new_entry.transponder_name[transponder_index], templine, MAX_NUM_CHARS);

			//uplink frequencies
			fgets(templine, MAX_NUM_CHARS, fd);
			sscanf(templine,"%lf, %lf", &(new_entry.uplink_start[transponder_index]), &(new_entry.uplink_end[transponder_index]));

			//downlink frequencies
			fgets(templine, MAX_NUM_CHARS, fd);
			sscanf(templine,"%lf, %lf", &(new_entry.downlink_start[transponder_index]), &(new_entry.downlink_end[transponder_index]));

			//unused information: weekly schedule for transponder. See issue #29.
			fgets(templine, MAX_NUM_CHARS, fd);

			//unused information: orbital schedule for transponder. See issue #29.
			fgets(templine, MAX_NUM_CHARS, fd);

			//check whether transponder is well-defined
			if ((new_entry.uplink_start[transponder_index]!=0.0 || new_entry.downlink_start[transponder_index]!=0.0) && (transponder_index < MAX_NUM_TRANSPONDERS)) {
				transponder_index++;
			}
		}
		new_entry.num_transponders = transponder_index;

		//add to transponder database only when we can find corresponding entry in TLE database
		int tle_index = tle_db_find_entry(tle_db, satellite_number);
		bool in_tle_database = tle_index != -1;
		if (in_tle_database) {
			int new_location = ret_db->sats[tle_index].location | location_info; //ensure correct flag combination for entry location
			transponder_db_entry_copy(&(ret_db->sats[tle_index]), &new_entry);
			ret_db->sats[tle_index].location = new_location;
			ret_db->loaded = true;
		}
	}

	fclose(fd);
	return 0;
}

bool transponder_db_entry_empty(const struct sat_db_entry *entry)
{
	//check if downlink/uplinks are well-defined
	int num_defined_entries = 0;
	for (int i=0; i < entry->num_transponders; i++) {
		if ((entry->downlink_start[i] != 0.0) || (entry->uplink_start[i] != 0.0)) {
			num_defined_entries++;
		}
	}
	return ((num_defined_entries == 0) && !(entry->squintflag));
}

void transponder_db_from_search_paths(const struct tle_db *tle_db, struct transponder_db *transponder_db)
{
	string_array_t data_dirs = {0};
	char *data_home = xdg_data_home();

	char *data_dirs_str = xdg_data_dirs();
	stringsplit(data_dirs_str, &data_dirs);
	free(data_dirs_str);

	//initialize database
	for (int i=0; i < MAX_NUM_SATS; i++) {
		transponder_db->sats[i].squintflag = false;
		transponder_db->sats[i].num_transponders = 0;
		transponder_db->sats[i].location = LOCATION_NONE;
	}

	//read transponder databases from system-wide data directories in opposide order of precedence
	for (int i=string_array_size(&data_dirs)-1; i >= 0; i--) {
		char db_path[MAX_NUM_CHARS] = {0};
		snprintf(db_path, MAX_NUM_CHARS, "%s%s", string_array_get(&data_dirs, i), DB_RELATIVE_FILE_PATH);
		transponder_db_from_file(db_path, tle_db, transponder_db, LOCATION_DATA_DIRS);
	}
	string_array_free(&data_dirs);

	//read from user home directory
	char db_path[MAX_NUM_CHARS] = {0};
	snprintf(db_path, MAX_NUM_CHARS, "%s%s", data_home, DB_RELATIVE_FILE_PATH);
	transponder_db_from_file(db_path, tle_db, transponder_db, LOCATION_DATA_HOME);
	free(data_home);
}

void transponder_db_to_file(const char *filename, struct tle_db *tle_db, struct transponder_db *transponder_db, bool *should_write)
{
	FILE *fd;
	fd = fopen(filename,"w");
	if (fd != NULL) {
		for (int i=0; i < transponder_db->num_sats; i++) {
			if (should_write[i]) {
				struct sat_db_entry *entry = &(transponder_db->sats[i]);
				fprintf(fd, "%s\n", tle_db->tles[i].name);
				fprintf(fd, "%ld\n", tle_db->tles[i].satellite_number);

				//squint properties
				if (entry->squintflag) {
					fprintf(fd, "%f, %f\n", entry->alat, entry->alon);
				} else {
					fprintf(fd, "No alat, alon\n");
				}

				//transponders
				for (int j=0; j < entry->num_transponders; j++) {
					if ((entry->uplink_start[j] != 0.0) || (entry->downlink_start[j] != 0.0)) {
						fprintf(fd, "%s\n", entry->transponder_name[j]);
						fprintf(fd, "%f, %f\n", entry->uplink_start[j], entry->uplink_end[j]);
						fprintf(fd, "%f, %f\n", entry->downlink_start[j], entry->downlink_end[j]);
						fprintf(fd, "No weekly schedule\n"); //FIXME: See issue #29.
						fprintf(fd, "No orbital schedule\n");
					}
				}
				fprintf(fd, "end\n");
			}
		}
		fclose(fd);
	}
}

void transponder_db_write_to_default(struct tle_db *tle_db, struct transponder_db *transponder_db)
{
	//get writepath
	create_xdg_dirs();
	char *data_home = xdg_data_home();
	char writepath[MAX_NUM_CHARS] = {0};
	snprintf(writepath, MAX_NUM_CHARS, "%s%s", data_home, DB_RELATIVE_FILE_PATH);
	free(data_home);

	//write database to file
	bool *should_write = (bool*)calloc(transponder_db->num_sats, sizeof(bool));
	for (int i=0; i < transponder_db->num_sats; i++) {
		struct sat_db_entry *entry = &(transponder_db->sats[i]);

		//write to user database if the entry was originally loaded from XDG_DATA_HOME, or has been marked as being edited
		if ((entry->location & LOCATION_DATA_HOME) || (entry->location & LOCATION_TRANSIENT)) {
			should_write[i] = true;
		}

		//do not write to user database if it's only defined LOCATION_DATA_HOME and has become empty
		if (((entry->location & LOCATION_DATA_HOME) || (entry->location & LOCATION_NONE)) && !(entry->location & LOCATION_DATA_DIRS) && transponder_db_entry_empty(entry)) {
			should_write[i] = false;
		}
	}
	transponder_db_to_file(writepath, tle_db, transponder_db, should_write);
	free(should_write);
}

bool transponder_db_entry_equal(struct sat_db_entry *entry_1, struct sat_db_entry *entry_2)
{
	if ((entry_1->squintflag != entry_2->squintflag) ||
		(entry_1->alat != entry_2->alat) ||
		(entry_1->alon != entry_2->alon) ||
		(entry_1->num_transponders != entry_2->num_transponders)) {
		return false;
	}

	for (int i=0; i < entry_1->num_transponders; i++) {
		if ((strncmp(entry_1->transponder_name[i], entry_2->transponder_name[i], MAX_NUM_CHARS) != 0) ||
			(entry_1->uplink_start[i] != entry_2->uplink_start[i]) ||
			(entry_1->uplink_end[i] != entry_2->uplink_end[i]) ||
			(entry_1->downlink_start[i] != entry_2->downlink_start[i]) ||
			(entry_1->downlink_end[i] != entry_2->downlink_end[i])) {
			return false;
		}
	}
	return true;
}

void transponder_db_entry_copy(struct sat_db_entry *destination, struct sat_db_entry *source)
{
	destination->squintflag = source->squintflag;
	destination->alat = source->alat;
	destination->alon = source->alon;
	destination->num_transponders = source->num_transponders;
	for (int i=0; i < source->num_transponders; i++) {
		strncpy(destination->transponder_name[i], source->transponder_name[i], MAX_NUM_CHARS);
	}
	memcpy(destination->uplink_start, source->uplink_start, MAX_NUM_TRANSPONDERS*sizeof(double));
	memcpy(destination->uplink_end, source->uplink_end, MAX_NUM_TRANSPONDERS*sizeof(double));
	memcpy(destination->downlink_start, source->downlink_start, MAX_NUM_TRANSPONDERS*sizeof(double));
	memcpy(destination->downlink_end, source->downlink_end, MAX_NUM_TRANSPONDERS*sizeof(double));
	destination->location = source->location;
}
