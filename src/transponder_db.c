#include "transponder_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xdg_basedirs.h"
#include "string_array.h"

int transponder_db_from_file(const char *dbfile, const struct tle_db *tle_db, struct transponder_db *ret_db)
{
	//copied from ReadDataFiles().

	/* Load satellite database file */
	ret_db->num_sats = tle_db->num_tles;
	FILE *fd=fopen(dbfile,"r");
	long catnum;
	char line1[80];
	int y = 0, match = 0, transponders = 0, entry = 0;
	if (fd!=NULL) {
		fgets(line1,40,fd);

		while (strncmp(line1,"end",3)!=0 && line1[0]!='\n' && feof(fd)==0) {
			/* The first line is the satellite
			   name which is ignored here. */

			fgets(line1,40,fd);
			sscanf(line1,"%ld",&catnum);

			/* Search for match */

			for (y=0, match=0; y<tle_db->num_tles && match==0; y++) {
				if (catnum==tle_db->tles[y].satellite_number)
					match=1;
			}

			if (match) {
				transponders=0;
				entry=0;
				y--;
			}

			fgets(line1,40,fd);

			if (match) {
				if (strncmp(line1,"No",2)!=0) {
					sscanf(line1,"%lf, %lf",&(ret_db->sats[y].alat), &(ret_db->sats[y].alon));
					ret_db->sats[y].squintflag=1;
				}

				else
					ret_db->sats[y].squintflag=0;
			}

			fgets(line1,80,fd);

			while (strncmp(line1,"end",3)!=0 && line1[0]!='\n' && feof(fd)==0) {
				if (entry<MAX_NUM_TRANSPONDERS) {
					if (match) {
						if (strncmp(line1,"No",2)!=0) {
							line1[strlen(line1)-1]=0;
							strcpy(ret_db->sats[y].transponder_name[entry],line1);
						} else
							ret_db->sats[y].transponder_name[entry][0]=0;
					}

					fgets(line1,40,fd);

					if (match)
						sscanf(line1,"%lf, %lf", &(ret_db->sats[y].uplink_start[entry]), &(ret_db->sats[y].uplink_end[entry]));

					fgets(line1,40,fd);

					if (match)
						sscanf(line1,"%lf, %lf", &(ret_db->sats[y].downlink_start[entry]), &(ret_db->sats[y].downlink_end[entry]));

					fgets(line1,40,fd); //FIXME: Unused information: weekly schedule for transponder. See issue #29.
					fgets(line1,40,fd); //Unused information: orbital schedule for transponder.

					if (match) {
						if (ret_db->sats[y].uplink_start[entry]!=0.0 || ret_db->sats[y].downlink_start[entry]!=0.0)
							transponders++;

						entry++;
					}
				}
				fgets(line1,80,fd);
			}
			fgets(line1,80,fd);

			if (match) {
				ret_db->sats[y].num_transponders=transponders;
				ret_db->loaded = true;
			}

			entry=0;
			transponders=0;
		}

		fclose(fd);
	} else {
		return -1;
	}
	return 0;
}

void transponder_db_from_search_paths(const struct tle_db *tle_db, struct transponder_db *transponder_db)
{
	string_array_t data_dirs = {0};
	char *data_home = xdg_data_home();
	string_array_add(&data_dirs, data_home);
	free(data_home);

	char *data_dirs_str = xdg_data_dirs();
	stringsplit(data_dirs_str, &data_dirs);
	free(data_dirs_str);

	//read transponder databases from system-wide data directories in opposide order of precedence, and then the home directory
	for (int i=string_array_size(&data_dirs)-1; i >= 0; i--) {
		char db_path[MAX_NUM_CHARS] = {0};
		snprintf(db_path, MAX_NUM_CHARS, "%s%s", string_array_get(&data_dirs, i), DB_RELATIVE_FILE_PATH);

		//will overwrite existing entries at their correct positions automatically, and ignore everything else
		transponder_db_from_file(db_path, tle_db, transponder_db);
	}
	string_array_free(&data_dirs);
}

void transponder_db_to_file(const char *filename, struct tle_db *tle_db, struct transponder_db *transponder_db)
{
	FILE *fd;
	fd = fopen(filename,"w");
	for (int i=0; i < transponder_db->num_sats; i++) {
		struct sat_db_entry *entry = &(transponder_db->sats[i]);

		//write if sat db entry is non-empty
		if ((entry->num_transponders > 0) || (entry->squintflag)) {
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
				fprintf(fd, "%s\n", entry->transponder_name[j]);
				fprintf(fd, "%f, %f\n", entry->uplink_start[j], entry->uplink_end[j]);
				fprintf(fd, "%f, %f\n", entry->downlink_start[j], entry->downlink_end[j]);
				fprintf(fd, "No weekly schedule\n"); //FIXME: See issue #29.
				fprintf(fd, "No orbital schedule\n");
			}
			fprintf(fd, "end\n");
		}
	}
	fclose(fd);
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
	transponder_db_to_file(writepath, tle_db, transponder_db);
}
