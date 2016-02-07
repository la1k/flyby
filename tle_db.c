#include "tle_db.h"
#include <stdio.h>
#include <predict/predict.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include "flyby_ui.h"
#include "flyby_config.h"
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include "string_array.h"

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

/**
 * Get filename to put TLE updates in when the TLE to update are located within system-wide, non-writable locations.
 * Uses XDG_DATA_HOME/flyby/tle/tle-updatefile-[DATE]-[TIME]-[NUMBER].tle, and loops through [NUMBER] until a non-existing
 * file is found.
 **/
char *tle_db_updatefile_writepath()
{
	create_xdg_dirs();

	//create filepath basis
	char *ret_string = (char*)malloc(sizeof(char)*MAX_NUM_CHARS);
	char date_string[MAX_NUM_CHARS];
	time_t epoch = time(NULL);
	strftime(date_string, MAX_NUM_CHARS, "tle-updatefile-%F-%H%M%S-", gmtime(&epoch));
	char *data_home = xdg_data_home();

	//loop through [number] in [basename]-[number].tle until we encounter a file that does not exist yet
	int index = 0;
	while (true) {
		char temp_file[MAX_NUM_CHARS];
		snprintf(temp_file, MAX_NUM_CHARS, "%s%s%s%d.tle", data_home, TLE_RELATIVE_DIR_PATH, date_string, index);
		if (access(temp_file, F_OK) == -1) {
			strncpy(ret_string, temp_file, MAX_NUM_CHARS);
			break;
		}
		index++;
	}
	return ret_string;
}

/**
 * Write the parts of the TLE database that are read from the supplied filename to file again. Used for updating
 * the TLE files with new information, if any.
 *
 * \param tle_filename Filename to which corresponding entries are found and written
 * \param tle_db TLE database
 **/
void tle_db_update_file(const char *tle_filename, struct tle_db *tle_db)
{
	struct tle_db subset_db = {0};
	for (int i=0; i < tle_db->num_tles; i++) {
		if (strcmp(tle_db->tles[i].filename, tle_filename) == 0) {
			tle_db_add_entry(&subset_db, &(tle_db->tles[i]));
		}
	}
	tle_db_to_file(tle_filename, &subset_db);
}

void tle_db_update(const char *filename, struct tle_db *tle_db, bool *ret_was_updated, bool *ret_in_new_file)
{
	struct tle_db new_db = {0};
	int retval = tle_db_from_file(filename, &new_db);
	if (retval != 0) {
		return;
	}

	int num_tles_to_update = 0;
	int *newer_tle_indices = (int*)malloc(sizeof(int)*new_db.num_tles); //indices in new TLE db that should be used to update internal db
	int *tle_indices_to_update = (int*)malloc(sizeof(int)*new_db.num_tles); //indices in internal db that should be updated

	//find more recent entries
	for (int i=0; i < new_db.num_tles; i++) {
		int index = tle_db_find_entry(tle_db, new_db.tles[i].satellite_number);
		if (index != -1) {
			if (tle_db_entry_is_newer_than(new_db.tles[i], tle_db->tles[index])) {
				newer_tle_indices[num_tles_to_update] = i;
				tle_indices_to_update[num_tles_to_update] = index;
				num_tles_to_update++;
			}
		}
	}

	if (num_tles_to_update <= 0) {
		return;
	}

	int num_unwritable = 0;
	int *unwritable_tles = (int*)malloc(sizeof(int)*num_tles_to_update); //indices in the internal db that were updated, but cannot be written to the current file.

	//go over tles to update, collect tles belonging to one file in one update, update the file if possible, add to above array if not. Update internal db with new TLE information.
	for (int i=0; i < num_tles_to_update; i++) {
		if (newer_tle_indices[i] != -1) {
			char *tle_filename = tle_db->tles[tle_indices_to_update[i]].filename; //filename to be updated
			bool file_is_writable = access(tle_filename, W_OK) == 0;

			//find entries in tle database with corresponding filenames
			for (int j=i; j < num_tles_to_update; j++) {
				if (newer_tle_indices[j] != -1) {
					int tle_index = tle_indices_to_update[j];
					struct tle_db_entry *tle_update_entry = &(new_db.tles[newer_tle_indices[j]]);
					struct tle_db_entry *tle_entry = &(tle_db->tles[tle_index]);
					if (strcmp(tle_filename, tle_entry->filename) == 0) {
						//update tle db entry with new entry
						char keep_filename[MAX_NUM_CHARS];
						char keep_name[MAX_NUM_CHARS];
						strncpy(keep_filename, tle_entry->filename, MAX_NUM_CHARS);
						strncpy(keep_name, tle_entry->name, MAX_NUM_CHARS);

						tle_db_overwrite_entry(tle_index, tle_db, tle_update_entry);

						//keep old filename and name
						strncpy(tle_entry->filename, keep_filename, MAX_NUM_CHARS);
						strncpy(tle_entry->name, keep_name, MAX_NUM_CHARS);

						//set db indices to update to -1 in order to ignore them on the next update
						newer_tle_indices[j] = -1;

						if (ret_was_updated != NULL) {
							ret_was_updated[tle_index] = true;
						}

						if (!file_is_writable) {
							//add to list over unwritable TLE filenames
							unwritable_tles[num_unwritable] = tle_index;
							num_unwritable++;

							if (ret_in_new_file != NULL) {
								ret_in_new_file[tle_index] = true;
							}
						}
					}
				}
			}

			//write updated TLE entries to file
			if (file_is_writable) {
				tle_db_update_file(tle_filename, tle_db);
			}
		}
	}

	if ((num_unwritable > 0) && (tle_db->read_from_xdg)) {
		//write unwritable TLEs to new file
		char *new_tle_filename = tle_db_updatefile_writepath();

		struct tle_db unwritable_db = {0};
		for (int i=0; i < num_unwritable; i++) {
			tle_db_add_entry(&unwritable_db, &(tle_db->tles[unwritable_tles[i]]));
			strncpy(tle_db->tles[unwritable_tles[i]].filename, new_tle_filename, MAX_NUM_CHARS);
		}
		tle_db_to_file(new_tle_filename, &unwritable_db);

		free(new_tle_filename);
	}

	free(newer_tle_indices);
	free(tle_indices_to_update);
	free(unwritable_tles);
}

void tle_db_from_search_paths(struct tle_db *ret_tle_db)
{
	//read tles from user directory
	char *data_home = xdg_data_home();
	char home_tle_dir[MAX_NUM_CHARS] = {0};
	snprintf(home_tle_dir, MAX_NUM_CHARS, "%s%s", data_home, TLE_RELATIVE_DIR_PATH);
	tle_db_from_directory(home_tle_dir, ret_tle_db);
	free(data_home);

	char *data_dirs_str = xdg_data_dirs();
	string_array_t data_dirs = {0};
	stringsplit(data_dirs_str, &data_dirs);

	//read tles from system-wide data directories the order of precedence
	for (int i=0; i < string_array_size(&data_dirs); i++) {
		char dir[MAX_NUM_CHARS] = {0};
		snprintf(dir, MAX_NUM_CHARS, "%s%s", string_array_get(&data_dirs, i), TLE_RELATIVE_DIR_PATH);

		struct tle_db temp_db = {0};
		tle_db_from_directory(dir, &temp_db);
		tle_db_merge(&temp_db, ret_tle_db, TLE_OVERWRITE_NONE); //multiply defined TLEs in directories of less precedence are ignored
	}
	string_array_free(&data_dirs);
	free(data_dirs_str);

	ret_tle_db->read_from_xdg = true;
}

