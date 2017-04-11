#include "tle_db.h"
#include <stdio.h>
#include <predict/predict.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include "ui.h"
#include "xdg_basedirs.h"
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include "string_array.h"
#include <ctype.h>

struct tle_db *tle_db_create()
{
	struct tle_db *tle_db = (struct tle_db*) malloc(sizeof(struct tle_db));
	memset((void*)tle_db, 0, sizeof(struct tle_db));
	return tle_db;
}

void tle_db_destroy(struct tle_db **tle_db)
{
	if ((*tle_db)->tles != NULL) {
		free((*tle_db)->tles);
	}
	free(*tle_db);
	*tle_db = NULL;
}

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

				if ((merge_opt == TLE_OVERWRITE_OLD) && tle_db_entry_is_newer_than(new_db->tles[i], main_db->tles[j])) {
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

int tle_db_find_entry(const struct tle_db *tle_db, long satellite_number)
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

	char *dirpath_ext = NULL;
	if (dirpath[strlen(dirpath)-1] != '/') {
		dirpath_ext = (char*)malloc(sizeof(char)*(strlen(dirpath)+2));
		strcpy(dirpath_ext, dirpath);
		dirpath_ext[strlen(dirpath)] = '/';
		dirpath_ext[strlen(dirpath_ext)] = '\0';
	} else {
		dirpath_ext = strdup(dirpath);
	}

	d = opendir(dirpath_ext);
	if (d) {
		while ((file = readdir(d)) != NULL) {
			if (file->d_type == DT_REG) {
				int pathsize = strlen(file->d_name) + strlen(dirpath_ext) + 1;
				char *full_path = (char*)malloc(sizeof(char)*pathsize);
				snprintf(full_path, pathsize, "%s%s", dirpath_ext, file->d_name);

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
	free(dirpath_ext);
}

/* This function scans line 1 and line 2 of a NASA 2-Line element
 * set and returns a 1 if the element set appears to be valid or
 * a 0 if it does not.  If the data survives this torture test,
 * it's a pretty safe bet we're looking at a valid 2-line
 * element set and not just some random text that might pass
 * as orbital data based on a simple checksum calculation alone.
 *
 * \param line1 Line 1 of TLE
 * \param line2 Line 2 of TLE
 * \return 1 if valid, 0 if not
 **/
char KepCheck(const char *line1, const char *line2)
{
	int x;
	unsigned sum1, sum2;

	unsigned char val[256];

	/* Set up translation table for computing TLE checksums */

	for (x=0; x<=255; val[x]=0, x++);
	for (x='0'; x<='9'; val[x]=x-'0', x++);

	val['-']=1;

	/* Compute checksum for each line */

	for (x=0, sum1=0, sum2=0; x<=67; sum1+=val[(int)line1[x]], sum2+=val[(int)line2[x]], x++);

	/* Perform a "torture test" on the data */

	x=(val[(int)line1[68]]^(sum1%10)) | (val[(int)line2[68]]^(sum2%10)) |
	  (line1[0]^'1')  | (line1[1]^' ')  | (line1[7]^'U')  |
	  (line1[8]^' ')  | (line1[17]^' ') | (line1[23]^'.') |
	  (line1[32]^' ') | (line1[34]^'.') | (line1[43]^' ') |
	  (line1[52]^' ') | (line1[61]^' ') | (line1[62]^'0') |
	  (line1[63]^' ') | (line2[0]^'2')  | (line2[1]^' ')  |
	  (line2[7]^' ')  | (line2[11]^'.') | (line2[16]^' ') |
	  (line2[20]^'.') | (line2[25]^' ') | (line2[33]^' ') |
	  (line2[37]^'.') | (line2[42]^' ') | (line2[46]^'.') |
	  (line2[51]^' ') | (line2[54]^'.') | (line1[2]^line2[2]) |
	  (line1[3]^line2[3]) | (line1[4]^line2[4]) |
	  (line1[5]^line2[5]) | (line1[6]^line2[6]) |
	  (isdigit(line1[68]) ? 0 : 1) | (isdigit(line2[68]) ? 0 : 1) |
	  (isdigit(line1[18]) ? 0 : 1) | (isdigit(line1[19]) ? 0 : 1) |
	  (isdigit(line2[31]) ? 0 : 1) | (isdigit(line2[32]) ? 0 : 1);

	return (x ? 0 : 1);
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

int tle_db_to_file(const char *filename, struct tle_db *tle_db)
{
	int x;
	FILE *fd;

	/* Save orbital data to tlefile */
	fd=fopen(filename,"w");
	if (fd != NULL) {
		for (x=0; x<tle_db->num_tles; x++) {
			fprintf(fd,"%s\n", tle_db->tles[x].name);
			fprintf(fd,"%s\n", tle_db->tles[x].line1);
			fprintf(fd,"%s\n", tle_db->tles[x].line2);
		}

		fclose(fd);
		return 0;
	} else {
		return -1;
	}
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
	struct tle_db *subset_db = tle_db_create();
	for (int i=0; i < tle_db->num_tles; i++) {
		if (strcmp(tle_db->tles[i].filename, tle_filename) == 0) {
			tle_db_add_entry(subset_db, &(tle_db->tles[i]));
		}
	}
	tle_db_to_file(tle_filename, subset_db);
	tle_db_destroy(&subset_db);
}

void tle_db_update(const char *filename, struct tle_db *tle_db, int *update_status)
{
	struct tle_db *new_db = tle_db_create();
	int retval = tle_db_from_file(filename, new_db);
	if (retval != 0) {
		tle_db_destroy(&new_db);
		return;
	}

	if (update_status != NULL) {
		for (int i=0; i < tle_db->num_tles; i++) {
			update_status[i] = 0;
		}
	}

	int num_tles_to_update = 0;
	int *newer_tle_indices = (int*)malloc(sizeof(int)*new_db->num_tles); //indices in new TLE db that should be used to update internal db
	int *tle_indices_to_update = (int*)malloc(sizeof(int)*new_db->num_tles); //indices in internal db that should be updated

	//find more recent entries
	for (int i=0; i < new_db->num_tles; i++) {
		int index = tle_db_find_entry(tle_db, new_db->tles[i].satellite_number);
		if (index != -1) {
			if (tle_db_entry_is_newer_than(new_db->tles[i], tle_db->tles[index])) {
				newer_tle_indices[num_tles_to_update] = i;
				tle_indices_to_update[num_tles_to_update] = index;
				num_tles_to_update++;
			}
		}
	}

	if (num_tles_to_update <= 0) {
		tle_db_destroy(&new_db);
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
					struct tle_db_entry *tle_update_entry = &(new_db->tles[newer_tle_indices[j]]);
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

						if (update_status != NULL) {
							update_status[tle_index] |= TLE_DB_UPDATED;
							if (file_is_writable) {
								update_status[tle_index] |= TLE_FILE_UPDATED;
							}
						}

						if (!file_is_writable) {
							//add to list over unwritable TLE filenames
							unwritable_tles[num_unwritable] = tle_index;
							num_unwritable++;
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
			int tle_ind = unwritable_tles[i];
			tle_db_add_entry(&unwritable_db, &(tle_db->tles[tle_ind]));
			strncpy(tle_db->tles[tle_ind].filename, new_tle_filename, MAX_NUM_CHARS);
		}
		int retval = tle_db_to_file(new_tle_filename, &unwritable_db);
		if ((update_status != NULL) && (retval != -1)) {
			for (int i=0; i < num_unwritable; i++) {
				int tle_ind = unwritable_tles[i];
				update_status[tle_ind] |= TLE_IN_NEW_FILE;
			}
		}
		free(new_tle_filename);
	}

	free(newer_tle_indices);
	free(tle_indices_to_update);
	free(unwritable_tles);
	tle_db_destroy(&new_db);
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

		struct tle_db *temp_db = tle_db_create();
		tle_db_from_directory(dir, temp_db);
		tle_db_merge(temp_db, ret_tle_db, TLE_OVERWRITE_NONE); //multiply defined TLEs in directories of less precedence are ignored
		tle_db_destroy(&temp_db);
	}
	string_array_free(&data_dirs);
	free(data_dirs_str);

	ret_tle_db->read_from_xdg = true;
}

void tle_db_entry_set_enabled(struct tle_db *db, int tle_index, bool enabled)
{
	if ((tle_index < db->num_tles) && (tle_index >= 0)) {
		db->tles[tle_index].enabled = enabled;
	}
}

bool tle_db_entry_enabled(const struct tle_db *db, int tle_index)
{
	if ((tle_index < db->num_tles) && (tle_index >= 0)) {
		return db->tles[tle_index].enabled;
	}
	return false;
}

predict_orbital_elements_t *tle_db_entry_to_orbital_elements(const struct tle_db *db, int tle_index)
{
	if ((tle_index < db->num_tles) && (tle_index >= 0)) {
		const struct tle_db_entry *entry = &(db->tles[tle_index]);
		char *tle[2] = {(char*)(entry->line1), (char*)(entry->line2)};
		return predict_parse_tle(tle);
	} else {
		return NULL;
	}
}

const char *tle_db_entry_name(const struct tle_db *db, int tle_index)
{
	if ((tle_index < db->num_tles) && (tle_index >= 0)) {
		return db->tles[tle_index].name;
	}
	return NULL;
}

void whitelist_from_file(const char *file, struct tle_db *db)
{
	for (int i=0; i < db->num_tles; i++) {
		tle_db_entry_set_enabled(db, i, false);
	}

	FILE *fd = fopen(file, "r");
	char temp_str[MAX_NUM_CHARS] = {0};
	if (fd != NULL) {
		while (feof(fd) == 0) {
			fgets(temp_str, MAX_NUM_CHARS, fd);
			long satellite_number = strtol(temp_str, NULL, 10);
			tle_db_entry_set_enabled(db, tle_db_find_entry(db, satellite_number), true);
		}
		fclose(fd);
	}
}

string_array_t tle_db_filenames(const struct tle_db *db)
{
	string_array_t returned_list = {0};
	for (int i=0; i < db->num_tles; i++) {
		if (string_array_find(&returned_list, db->tles[i].filename) == -1) {
			string_array_add(&returned_list, db->tles[i].filename);
		}
	}
	return returned_list;
}

void whitelist_from_search_paths(struct tle_db *db)
{
	//try to read QTH file from user home
	char *config_home = xdg_config_home();
	char whitelist_path[MAX_NUM_CHARS] = {0};
	snprintf(whitelist_path, MAX_NUM_CHARS, "%s%s", config_home, WHITELIST_RELATIVE_FILE_PATH);
	free(config_home);

	whitelist_from_file(whitelist_path, db);
}

void whitelist_to_file(const char *filename, struct tle_db *db)
{
	FILE* fd = fopen(filename, "w");
	if (fd != NULL) {
		for (int i=0; i < db->num_tles; i++) {
			if (tle_db_entry_enabled(db, i)) {
				fprintf(fd, "%ld\n", db->tles[i].satellite_number);
			}
		}
		fclose(fd);
	}
}

void whitelist_write_to_default(struct tle_db *db)
{
	//get writepath
	create_xdg_dirs();
	char *config_home = xdg_config_home();
	char writepath[MAX_NUM_CHARS] = {0};
	snprintf(writepath, MAX_NUM_CHARS, "%s%s", config_home, WHITELIST_RELATIVE_FILE_PATH);
	free(config_home);

	//write whitelist to writepath
	whitelist_to_file(writepath, db);
}

