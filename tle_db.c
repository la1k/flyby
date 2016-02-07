#include "tle_db.h"
#include <stdio.h>
#include <predict/predict.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include "flyby_ui.h"

/**
 * Check if tle_1 is more recent than tle_2.
 *
 * \param tle_1 First NORAD TLE
 * \param tle_2 Second NORAD TLE
 * \return True if tle_1 is newer than tle_2, false otherwise
 **/
bool tle_is_newer_than(char *tle_1[2], char *tle_2[2])
{
	predict_orbital_elements_t *orbele_1 = predict_parse_tle(tle_1);
	predict_orbital_elements_t *orbele_2 = predict_parse_tle(tle_2);

	double epoch_1 = orbele_1->epoch_year*1000.0 + orbele_1->epoch_day;
	double epoch_2 = orbele_2->epoch_year*1000.0 + orbele_2->epoch_day;

	predict_destroy_orbital_elements(orbele_1);
	predict_destroy_orbital_elements(orbele_2);

	return epoch_1 > epoch_2;
}

bool tle_db_entry_is_newer_than(struct tle_db_entry tle_entry_1, struct tle_db_entry tle_entry_2)
{
	char *tle_1[2] = {tle_entry_1.line1, tle_entry_1.line2};
	char *tle_2[2] = {tle_entry_2.line1, tle_entry_2.line2};
	return tle_is_newer_than(tle_1, tle_2);
}

void tle_db_overwrite_entry(int entry_index, struct tle_db *tle_db, const struct tle_db_entry *new_entry)
{
	if (entry_index < tle_db->num_tles) {
		tle_db->tles[entry_index].satellite_number = new_entry->satellite_number;
		strncpy(tle_db->tles[entry_index].name, new_entry->name, MAX_NUM_CHARS);
		strncpy(tle_db->tles[entry_index].line1, new_entry->line1, MAX_NUM_CHARS);
		strncpy(tle_db->tles[entry_index].line2, new_entry->line2, MAX_NUM_CHARS);
		strncpy(tle_db->tles[entry_index].filename, new_entry->filename, MAX_NUM_CHARS);
	}
}

void tle_db_add_entry(struct tle_db *tle_db, const struct tle_db_entry *entry)
{
	if (tle_db->num_tles+1 < MAX_NUM_SATS) {
		tle_db->num_tles++;
		tle_db_overwrite_entry(tle_db->num_tles-1, tle_db, entry);
	}
}

void tle_db_merge(struct tle_db *new_db, struct tle_db *main_db, enum tle_merge_behavior merge_opt)
{
	for (int i=0; i < new_db->num_tles; i++) {
		bool tle_exists = false;
		for (int j=0; j < main_db->num_tles; j++) {
			//check whether TLE already exists in the database
			if (new_db->tles[i].satellite_number == main_db->tles[j].satellite_number) {
				tle_exists = true;

				if ((merge_opt == TLE_OVERWRITE_OLD) && tle_db_entry_is_newer_than(new_db->tles[i], main_db->tles[i])) {
					tle_db_overwrite_entry(j, main_db, &(new_db->tles[i]));
				}
			}
		}

		//append TLE entry to main TLE database
		if (!tle_exists) {
			tle_db_add_entry(main_db, &(new_db->tles[i]));
		}
	}
}

int tle_db_find_entry(struct tle_db *tle_db, long satellite_number)
{
	for (int i=0; i < tle_db->num_tles; i++) {
		if (tle_db->tles[i].satellite_number == satellite_number) {
			return i;
		}
	}
	return -1;
}

void tle_db_from_directory(const char *dirpath, struct tle_db *ret_tle_db)
{
	DIR *d;
	struct dirent *file;
	d = opendir(dirpath);
	if (d) {
		while ((file = readdir(d)) != NULL) {
			if (file->d_type == DT_REG) {
				int pathsize = strlen(file->d_name) + strlen(dirpath) + 1;
				char *full_path = (char*)malloc(sizeof(char)*pathsize);
				snprintf(full_path, pathsize, "%s%s", dirpath, file->d_name);

				//read into empty TLE db
				struct tle_db temp_db = {0};
				tle_db_from_file(full_path, &temp_db);
				free(full_path);

				//merge with existing TLE db
				tle_db_merge(&temp_db, ret_tle_db, TLE_OVERWRITE_OLD); //overwrite only entries with older epochs
			}
		}
		closedir(d);
	}
}

int tle_db_from_file(const char *tle_file, struct tle_db *ret_db)
{
	//copied from ReadDataFiles().

	ret_db->num_tles = 0;
	int y = 0;
	char name[80], line1[80], line2[80];

	FILE *fd=fopen(tle_file,"r");
	if (fd!=NULL) {
		while (feof(fd)==0) {
			/* Initialize variables */

			name[0]=0;
			line1[0]=0;
			line2[0]=0;

			/* Read element set */

			fgets(name,75,fd);
			fgets(line1,75,fd);
			fgets(line2,75,fd);

			if (KepCheck(line1,line2) && (feof(fd)==0)) {
				/* We found a valid TLE! */

				/* Some TLE sources left justify the sat
				   name in a 24-byte field that is padded
				   with blanks.  The following lines cut
				   out the blanks as well as the line feed
				   character read by the fgets() function. */

				y=strlen(name);

				while (name[y]==32 || name[y]==0 || name[y]==10 || name[y]==13 || y==0) {
					name[y]=0;
					y--;
				}

				/* Copy TLE data into the sat data structure */

				struct tle_db_entry entry = {0};

				strncpy(entry.name,name,24);
				strncpy(entry.line1,line1,69);
				strncpy(entry.line2,line2,69);

				/* Get satellite number, so that the satellite database can be parsed. */

				char *tle[2] = {entry.line1, entry.line2};
				predict_orbital_elements_t *temp_elements = predict_parse_tle(tle);
				entry.satellite_number = temp_elements->satellite_number;
				predict_destroy_orbital_elements(temp_elements);

				strncpy(entry.filename, tle_file, MAX_NUM_CHARS);

				tle_db_add_entry(ret_db, &entry);
			}
		}

		fclose(fd);
	} else {
		return -1;
	}
	return 0;
}

void tle_db_to_file(const char *filename, struct tle_db *tle_db)
{
	int x;
	FILE *fd;

	/* Save orbital data to tlefile */

	fd=fopen(filename,"w");

	for (x=0; x<tle_db->num_tles; x++) {
		fprintf(fd,"%s\n", tle_db->tles[x].name);
		fprintf(fd,"%s\n", tle_db->tles[x].line1);
		fprintf(fd,"%s\n", tle_db->tles[x].line2);
	}

	fclose(fd);
}

