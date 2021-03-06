#include "tle_db.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

void test_tle_db_find_entry(void **param)
{
	struct tle_db *tle_db = tle_db_create();

	int expected_num_sats = 100;
	int satnum_offset = 6;
	for (int i=0; i < expected_num_sats; i++) {
		struct tle_db_entry dummy_entry = {0};
		dummy_entry.satellite_number = i+satnum_offset;
		tle_db_add_entry(tle_db, &dummy_entry);
	}
	assert_int_equal(tle_db->num_tles, expected_num_sats);

	long new_satellite_number = 83;
	int index = 63;

	tle_db->tles[index].satellite_number = new_satellite_number;

	assert_int_equal(tle_db_find_entry(tle_db, new_satellite_number), index);

	index = 40;
	assert_int_equal(tle_db_find_entry(tle_db, index + satnum_offset), index);
	assert_int_equal(tle_db_find_entry(tle_db, 200), -1);
}

void test_tle_db_add_entry(void **param)
{
	struct tle_db_entry dummy_entry_1 = {0};
	dummy_entry_1.satellite_number = 100;
	struct tle_db_entry dummy_entry_2 = {0};
	dummy_entry_2.satellite_number = 200;

	struct tle_db *tle_db = tle_db_create();

	tle_db_add_entry(tle_db, &dummy_entry_1);
	tle_db_add_entry(tle_db, &dummy_entry_2);

	assert_int_equal(tle_db->num_tles, 2);
	assert_int_equal(tle_db->tles[0].satellite_number, 100);
	assert_int_equal(tle_db->tles[1].satellite_number, 200);

	tle_db_destroy(&tle_db);
}

#define TEST_TLE_DIR "test_data/"
void test_tle_db_from_file(void **param)
{
	struct tle_db *tle_db = tle_db_create();

	//read empty database
	int retval = tle_db_from_file("/dev/NULL", tle_db);
	assert_int_not_equal(retval, 0);
	assert_int_equal(tle_db->num_tles, 0);

	//read pre-defined database
	retval = tle_db_from_file(TEST_TLE_DIR "old_tles/part1.tle", tle_db);
	assert_int_equal(retval, 0);
	assert_int_equal(tle_db->num_tles, 9);
	assert_string_equal(tle_db->tles[0].name, "CUTE-1.7+APD II (CO-65)");
	assert_string_equal(tle_db->tles[0].line1, "1 32785U 08021C   13115.72547332  .00001052  00000-0  13319-3 0  6142");
	assert_string_equal(tle_db->tles[0].line2, "2 32785  97.7560 174.7469 0015936 118.7374  28.1173 14.83745831270098");
}

void test_tle_db_overwrite_entry(void **param)
{
	struct tle_db_entry entry_1;
	entry_1.satellite_number = 100;
	char name[] = "name";
	strcpy(entry_1.name, name);
	char line1[] = "line_1";
	strcpy(entry_1.line1, line1);
	char line2[] = "line_2";
	strcpy(entry_1.line2, line2);

	struct tle_db *tle_db = tle_db_create();

	//add 13 entries to database
	int index = 10;
	int expected_num_sats = index+3;
	for (int i=0; i < expected_num_sats; i++) {
		struct tle_db_entry dummy_entry = {0};
		dummy_entry.satellite_number = i;
		tle_db_add_entry(tle_db, &dummy_entry);
	}

	//overwrite entry at index 10
	tle_db_overwrite_entry(index, tle_db, &entry_1);
	assert_true(entry_1.satellite_number == tle_db->tles[index].satellite_number);
	assert_string_equal(tle_db->tles[index].name, name);
	assert_string_equal(tle_db->tles[index].line1, line1);
	assert_string_equal(tle_db->tles[index].line2, line2);
	assert_int_equal(tle_db->num_tles, expected_num_sats);
}

void test_tle_db_entry_is_newer_than(void **param)
{
	struct tle_db_entry old_entry;
	strcpy(old_entry.name, "BEESAT");
	old_entry.satellite_number = 35933;
	strcpy(old_entry.line1, "1 35933U 09051C   13115.83979722  .00000632  00000-0  16119-3 0  2924");
	strcpy(old_entry.line2, "2 35933  98.3513 224.0841 0005397 226.9721 279.2955 14.53892524190336");

	struct tle_db_entry new_entry;
	strcpy(new_entry.name, "BEESAT");
	new_entry.satellite_number = 35933;
	strcpy(new_entry.line1, "1 35933U 09051C   16083.86818462  .00000300  00000-0  79122-4 0  9997");
	strcpy(new_entry.line2, "2 35933  98.4372 211.0478 0005599 197.7075 162.3928 14.55908579344897");

	assert_true(tle_db_entry_is_newer_than(new_entry, old_entry));
	assert_false(tle_db_entry_is_newer_than(old_entry, new_entry));
}

void test_tle_db_from_directory(void **param)
{
	struct tle_db *tle_db = tle_db_create();

	//non-existing directory
	tle_db_from_directory("/dev/NULL", tle_db);
	assert_int_equal(tle_db->num_tles, 0);

	//correct number from homogeneous directory
	tle_db_from_directory(TEST_TLE_DIR "old_tles/", tle_db);
	assert_int_equal(tle_db->num_tles, 16);

	//check when there is no trailing '/' in directory name
	tle_db_from_file("/dev/NULL", tle_db);
	assert_int_equal(tle_db->num_tles, 0);
	tle_db_from_directory(TEST_TLE_DIR "old_tles", tle_db);
	assert_int_equal(tle_db->num_tles, 16);

	//test new/old precedence in inhomogeneous directory
	struct tle_db *old_tles = tle_db_create();
	tle_db_from_directory(TEST_TLE_DIR "old_tles/", old_tles);
	assert_int_equal(old_tles->num_tles, 16);
	struct tle_db *new_tles = tle_db_create();
	tle_db_from_directory(TEST_TLE_DIR "newer_tles/", new_tles);
	assert_int_equal(new_tles->num_tles, 63);

	tle_db_from_file("/dev/NULL", tle_db);
	tle_db_from_directory(TEST_TLE_DIR "mixture/flyby/tles/", tle_db);
	assert_true(tle_db->num_tles > 0);

	bool in_both = false;
	for (int i=0; i < tle_db->num_tles; i++) {
		long sat_num = tle_db->tles[i].satellite_number;
		int old_ind = tle_db_find_entry(old_tles, sat_num);
		int new_ind = tle_db_find_entry(new_tles, sat_num);
		assert_true((old_ind != -1) || (new_ind != -1));

		//if TLE is in both databases, the one in tle_db should be more recent than the one in old_tles
		if ((old_ind != -1) && (new_ind != -1)) {
			in_both = true;
			assert_true(tle_db_entry_is_newer_than(new_tles->tles[new_ind], old_tles->tles[old_ind])); //check that the new TLE truly is newer than the old TLE
			assert_true(tle_db_entry_is_newer_than(tle_db->tles[i], old_tles->tles[old_ind])); //check that the newest one was picked
		}
	}
	assert_true(in_both); //check that we actually encounter a TLE in both new and old databases

	tle_db_destroy(&tle_db);
}

void test_tle_db_enabled(void **param)
{
	struct tle_db *tle_db = tle_db_create();
	tle_db_from_file(TEST_TLE_DIR "old_tles/part1.tle", tle_db);
	assert_true(tle_db->num_tles > 0);
	assert_false(tle_db_entry_enabled(tle_db, 0));

	tle_db_entry_set_enabled(tle_db, 0, true);
	assert_true(tle_db_entry_enabled(tle_db, 0));

	tle_db_entry_set_enabled(tle_db, 0, false);
	assert_false(tle_db_entry_enabled(tle_db, 0));

	tle_db_entry_set_enabled(tle_db, tle_db->num_tles, true);
	assert_false(tle_db_entry_enabled(tle_db, tle_db->num_tles));

	tle_db_entry_set_enabled(tle_db, tle_db->num_tles, false);
	assert_false(tle_db_entry_enabled(tle_db, tle_db->num_tles));
}

void test_tle_db_filenames(void **param)
{
	struct tle_db *tle_db = tle_db_create();
	tle_db_from_directory(TEST_TLE_DIR "old_tles/", tle_db);
	string_array_t string_array = tle_db_filenames(tle_db);
	assert_int_equal(string_array_size(&string_array), 2);
	assert_int_not_equal(string_array_find(&string_array, TEST_TLE_DIR "old_tles/part1.tle"), -1);
	assert_int_not_equal(string_array_find(&string_array, TEST_TLE_DIR "old_tles/part2.tle"), -1);
}

void test_tle_db_to_file(void **param)
{
	char filename[L_tmpnam] = "/tmp/XXXXXX";
	int fid = mkstemp(filename);
	assert_true(fid != -1);
	assert_true(strlen(filename) > 0);

	//write and read back non-empty database
	struct tle_db *tle_db = tle_db_create();
	tle_db_from_file(TEST_TLE_DIR "old_tles/part1.tle", tle_db);
	assert_true(tle_db->num_tles > 0);
	tle_db_to_file(filename, tle_db);

	struct tle_db *tle_db_2 = tle_db_create();
	tle_db_from_file(filename, tle_db_2);

	assert_int_equal(tle_db_2->num_tles, tle_db->num_tles);

	tle_db_destroy(&tle_db);
	tle_db_destroy(&tle_db_2);

	//write and read back empty database
	tle_db = tle_db_create();
	tle_db_2 = tle_db_create();
	assert_int_equal(tle_db->num_tles, 0);
	tle_db_from_file(TEST_TLE_DIR "old_tles/part1.tle", tle_db_2);
	assert_true(tle_db_2->num_tles > 0);
	tle_db_to_file(filename, tle_db);
	tle_db_from_file(filename, tle_db_2);
	assert_int_equal(tle_db_2->num_tles, 0);

	//try to write to non-existing location
	tle_db_to_file("/dev/NULL", tle_db);

	unlink(filename);
}

void test_whitelist_from_file(void **param)
{
	struct tle_db *tle_db = tle_db_create();
	tle_db_from_file(TEST_TLE_DIR "old_tles/part1.tle", tle_db);
	struct tle_db *tle_db_copy = tle_db_create();
	memcpy(tle_db_copy, tle_db, sizeof(struct tle_db));
	assert_memory_equal((char*)tle_db_copy, (char*)tle_db, sizeof(struct tle_db));

	//read non-existing whitelist
	whitelist_from_file("/dev/NULL", tle_db);
	assert_memory_equal((char*)tle_db_copy, (char*)tle_db, sizeof(struct tle_db));
	for (int i=0; i < tle_db->num_tles; i++) {
		assert_false(tle_db_entry_enabled(tle_db, i));
	}
	long sat_1 = 32785;
	long sat_2 = 33493;
	assert_false(tle_db_entry_enabled(tle_db, tle_db_find_entry(tle_db, sat_1)));
	assert_false(tle_db_entry_enabled(tle_db, tle_db_find_entry(tle_db, sat_2)));

	//read non-empty whitelist
	whitelist_from_file(TEST_TLE_DIR "flyby/flyby.whitelist", tle_db);
	assert_true(tle_db->num_tles > 0);
	assert_true(tle_db_entry_enabled(tle_db, tle_db_find_entry(tle_db, sat_1)));
	assert_true(tle_db_entry_enabled(tle_db, tle_db_find_entry(tle_db, sat_2)));
	for (int i=0; i < tle_db->num_tles; i++) {
		long satellite_number = tle_db->tles[i].satellite_number;
		if ((satellite_number != sat_1) && (satellite_number != sat_2)) {
			assert_false(tle_db_entry_enabled(tle_db, i));
		}
	}
}

void test_whitelist_to_file(void **param)
{
	struct tle_db *tle_db = tle_db_create();
	tle_db_from_file(TEST_TLE_DIR "old_tles/part1.tle", tle_db);
	int index = 0;
	whitelist_from_file("/dev/NULL", tle_db);
	tle_db_entry_set_enabled(tle_db, index, true);

	//write whitelist to file
	char filename[L_tmpnam] = "/tmp/XXXXX";
	mkstemp(filename);
	assert_true(strlen(filename) > 0);
	whitelist_to_file(filename, tle_db);

	tle_db_destroy(&tle_db);

	//read back whitelist
	struct tle_db *tle_db_new = tle_db_create();
	tle_db_from_file(TEST_TLE_DIR "old_tles/part1.tle", tle_db_new);
	whitelist_from_file("/dev/NULL", tle_db_new);
	assert_false(tle_db_entry_enabled(tle_db_new, index));
	whitelist_from_file(filename, tle_db_new);
	assert_true(tle_db_entry_enabled(tle_db_new, index));

	//write whitelist to impossible location
	whitelist_to_file("/dev/NULL", tle_db);

	tle_db_destroy(&tle_db_new);
}

void test_tle_db_merge(void **param)
{
	struct tle_db *tle_db_1 = tle_db_create();
	struct tle_db *tle_db_2 = tle_db_create();

	//check merge behavior for two complementary databases
	tle_db_from_file(TEST_TLE_DIR "old_tles/part1.tle", tle_db_1);
	tle_db_from_file(TEST_TLE_DIR "old_tles/part2.tle", tle_db_2);
	struct tle_db *merged = tle_db_create();
	tle_db_merge(tle_db_1, merged, TLE_OVERWRITE_NONE);
	tle_db_merge(tle_db_2, merged, TLE_OVERWRITE_NONE);
	assert_int_equal(tle_db_1->num_tles + tle_db_2->num_tles, merged->num_tles);

	//check merge behavior for two completely overlapping databases
	tle_db_from_file(TEST_TLE_DIR "old_tles/part1.tle", tle_db_1);
	tle_db_from_file(TEST_TLE_DIR "old_tles/part1.tle", tle_db_2);
	tle_db_from_file("/dev/NULL", merged);
	tle_db_merge(tle_db_1, merged, TLE_OVERWRITE_NONE);
	tle_db_merge(tle_db_2, merged, TLE_OVERWRITE_NONE);
	assert_int_equal(tle_db_1->num_tles, merged->num_tles);
	assert_int_equal(tle_db_2->num_tles, merged->num_tles);

	//check merge behavior for two partially overlapping databases
	tle_db_from_directory(TEST_TLE_DIR "old_tles/", tle_db_1);
	tle_db_from_file(TEST_TLE_DIR "newer_tles/amateur.txt", tle_db_2);
	tle_db_from_file("/dev/NULL", merged);
	tle_db_merge(tle_db_1, merged, TLE_OVERWRITE_NONE);
	tle_db_merge(tle_db_2, merged, TLE_OVERWRITE_NONE);
	int expected_num_tles = 66;
	assert_int_equal(merged->num_tles, expected_num_tles);

	//check that old entries are overwritten with new when this option is chosen
	tle_db_from_file("/dev/NULL", merged);
	tle_db_merge(tle_db_1, merged, TLE_OVERWRITE_NONE);
	tle_db_merge(tle_db_2, merged, TLE_OVERWRITE_OLD);
	assert_int_equal(merged->num_tles, expected_num_tles);
	bool oldnew_triggered = false;
	for (int i=0; i < merged->num_tles; i++) {
		long satellite_number = merged->tles[i].satellite_number;
		int old_entry = tle_db_find_entry(tle_db_1, satellite_number);
		int new_entry = tle_db_find_entry(tle_db_2, satellite_number);
		if ((old_entry != -1) && (new_entry != -1)) {
			oldnew_triggered = true;
			assert_true(tle_db_entry_is_newer_than(merged->tles[i], tle_db_1->tles[old_entry]));
		}
	}
	assert_true(oldnew_triggered);

	//check the same, but with opposite order of merge
	tle_db_from_file("/dev/NULL", merged);
	tle_db_merge(tle_db_2, merged, TLE_OVERWRITE_OLD);
	tle_db_merge(tle_db_1, merged, TLE_OVERWRITE_NONE);
	assert_int_equal(merged->num_tles, expected_num_tles);
	oldnew_triggered = false;
	for (int i=0; i < merged->num_tles; i++) {
		long satellite_number = merged->tles[i].satellite_number;
		int old_entry = tle_db_find_entry(tle_db_1, satellite_number);
		int new_entry = tle_db_find_entry(tle_db_2, satellite_number);
		if ((old_entry != -1) && (new_entry != -1)) {
			oldnew_triggered = true;
			assert_true(tle_db_entry_is_newer_than(merged->tles[i], tle_db_1->tles[old_entry]));
		}
	}
	assert_true(oldnew_triggered);

	tle_db_destroy(&tle_db_1);
	tle_db_destroy(&tle_db_2);
	tle_db_destroy(&merged);
}

char *xdg_data_dirs()
{
	return strdup((char*)mock());
}

char *xdg_data_home()
{
	return strdup((char*)mock());
}

void create_xdg_dirs()
{
}

char *xdg_config_home()
{
	return strdup((char*)mock());
}

void test_tle_db_from_search_paths(void **param)
{
	//test that search path gives same database as the direct path
	struct tle_db *tle_db_direct = tle_db_create();
	tle_db_from_directory(TEST_TLE_DIR "old_tles/flyby/tles/", tle_db_direct);

	will_return(xdg_data_dirs, "/dev/NULL");
	will_return(xdg_data_home, TEST_TLE_DIR "old_tles/");
	struct tle_db *tle_db_searchpaths = tle_db_create();
	tle_db_from_search_paths(tle_db_searchpaths);
	assert_true(tle_db_searchpaths->read_from_xdg);

	assert_int_equal(tle_db_direct->num_tles, tle_db_searchpaths->num_tles);

	//check that xdg_config_home takes precedence over xdg_data_home regardless of how old the TLEs are
	struct tle_db *tle_db_old = tle_db_create();
	tle_db_from_directory(TEST_TLE_DIR "old_tles/flyby/tles/", tle_db_old);
	struct tle_db *tle_db_new = tle_db_create();
	tle_db_from_directory(TEST_TLE_DIR "newer_tles/flyby/tles/", tle_db_new);

	will_return(xdg_data_dirs, TEST_TLE_DIR "newer_tles/");
	will_return(xdg_data_home, TEST_TLE_DIR "old_tles/");
	tle_db_from_file("/dev/NULL", tle_db_searchpaths);
	tle_db_from_search_paths(tle_db_searchpaths);

	bool in_both = false;
	for (int i=0; i < tle_db_searchpaths->num_tles; i++) {
		long sat_num = tle_db_searchpaths->tles[i].satellite_number;
		int old_ind = tle_db_find_entry(tle_db_old, sat_num);
		int new_ind = tle_db_find_entry(tle_db_new, sat_num);
		assert_true((old_ind != -1) || (new_ind != -1));
		if ((old_ind != -1) && (new_ind != -1)) {
			in_both = true;
			assert_false(tle_db_entry_is_newer_than(tle_db_searchpaths->tles[i], tle_db_new->tles[new_ind]));
		}
	}
	assert_true(in_both);
}

void test_whitelist_from_search_paths(void **param)
{
	struct tle_db *tle_db = tle_db_create();
	tle_db_from_file(TEST_TLE_DIR "old_tles/part1.tle", tle_db);
	struct tle_db *tle_db_copy = tle_db_create();
	memcpy(tle_db_copy, tle_db, sizeof(struct tle_db));

	//read non-existing whitelist
	will_return(xdg_config_home, "/dev/NULL");
	whitelist_from_search_paths(tle_db);
	for (int i=0; i < tle_db->num_tles; i++) {
		assert_false(tle_db_entry_enabled(tle_db, i));
	}
	long sat_1 = 32785;
	long sat_2 = 33493;
	assert_false(tle_db_entry_enabled(tle_db, tle_db_find_entry(tle_db, sat_1)));
	assert_false(tle_db_entry_enabled(tle_db, tle_db_find_entry(tle_db, sat_2)));

	//read non-empty whitelist
	will_return(xdg_config_home, TEST_TLE_DIR);
	whitelist_from_search_paths(tle_db);
	assert_true(tle_db->num_tles > 0);
	assert_true(tle_db_entry_enabled(tle_db, tle_db_find_entry(tle_db, sat_1)));
	assert_true(tle_db_entry_enabled(tle_db, tle_db_find_entry(tle_db, sat_2)));
	for (int i=0; i < tle_db->num_tles; i++) {
		long satellite_number = tle_db->tles[i].satellite_number;
		if ((satellite_number != sat_1) && (satellite_number != sat_2)) {
			assert_false(tle_db_entry_enabled(tle_db, i));
		}
	}
}

void set_filenames(struct tle_db *tle_db, const char *filename)
{
	for (int i=0; i < tle_db->num_tles; i++) {
		strcpy(tle_db->tles[i].filename, filename);
	}
}

void test_tle_db_update(void **param)
{
	struct tle_db *new_tles = tle_db_create();
	tle_db_from_file(TEST_TLE_DIR "newer_tles/amateur.txt", new_tles);

	struct tle_db *orig_db = tle_db_create();
	tle_db_from_directory(TEST_TLE_DIR "old_tles/", orig_db);
	struct tle_db *tle_db = tle_db_create();
	tle_db_from_directory(TEST_TLE_DIR "old_tles/", tle_db);
	set_filenames(tle_db, "/dev/NULL");
	assert_false(tle_db->read_from_xdg);
	int num_tles = tle_db->num_tles;

	//test with no new TLEs in file
	int *update_status = (int*)calloc(tle_db->num_tles, sizeof(int));
	tle_db_update(TEST_TLE_DIR "old_tles/part1.txt", tle_db, update_status);
	for (int i=0; i < tle_db->num_tles; i++) {
		assert_false(update_status[i] & TLE_IN_NEW_FILE);
		assert_false(update_status[i] & TLE_FILE_UPDATED);
		assert_false(update_status[i] & TLE_DB_UPDATED);
		update_status[i] = 0;
	}

	//test with new TLEs in updatefile

	//non-writable file locations, not read from XDG
	tle_db_from_file("/dev/NULL", tle_db);
	tle_db_from_directory(TEST_TLE_DIR "old_tles/", tle_db);
	set_filenames(tle_db, "/dev/NULL");
	tle_db_update(TEST_TLE_DIR "newer_tles/amateur.txt", tle_db, update_status);
	bool updated = false;
	for (int i=0; i < tle_db->num_tles; i++) {
		assert_false(update_status[i] & TLE_IN_NEW_FILE);
		assert_false(update_status[i] & TLE_FILE_UPDATED);

		if (update_status[i] & TLE_DB_UPDATED) {
			updated = true;
			assert_true(tle_db_entry_is_newer_than(tle_db->tles[i], orig_db->tles[i]));
		} else {
			assert_int_equal(tle_db_find_entry(new_tles, tle_db->tles[i].satellite_number), -1);
		}

		update_status[i] = 0;
	}
	assert_true(updated);
	assert_int_equal(num_tles, tle_db->num_tles);

	//writable location for all, not read from XDG
	tle_db_from_file("/dev/NULL", tle_db);
	tle_db_from_directory(TEST_TLE_DIR "old_tles/", tle_db);
	char temp_dir[] = "/tmp/flybytestXXXXXX";
	mkdtemp(temp_dir);
	char dirpath[MAX_NUM_CHARS];
	snprintf(dirpath, MAX_NUM_CHARS, "%s/", temp_dir);

	char filename[MAX_NUM_CHARS];
	snprintf(filename, MAX_NUM_CHARS, "%s/test.tle", temp_dir);
	int fid = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0777);
	close(fid);
	set_filenames(tle_db, filename);
	tle_db_update(TEST_TLE_DIR "newer_tles/amateur.txt", tle_db, update_status);
	updated = false;
	for (int i=0; i < tle_db->num_tles; i++) {
		assert_false(update_status[i] & TLE_IN_NEW_FILE); //all should be in the original file
		if (update_status[i] & TLE_DB_UPDATED) {
			updated = true;
			assert_true(update_status[i] & TLE_FILE_UPDATED);
		}
		update_status[i] = 0;
	}
	assert_true(updated);
	unlink(filename);

	//unwriteable location for all, writeable location for XDG_DATA_HOME
	tle_db_from_file("/dev/NULL", tle_db);
	tle_db_from_directory(TEST_TLE_DIR "old_tles/", tle_db);
	set_filenames(tle_db, "/dev/NULL");
	updated = false;

	char tle_path[MAX_NUM_CHARS];
	snprintf(tle_path, MAX_NUM_CHARS, "%s/flyby/", temp_dir);
	mkdir(tle_path, 0777);
	snprintf(tle_path, MAX_NUM_CHARS, "%s/flyby/tles/", temp_dir);
	mkdir(tle_path, 0777);
	will_return(xdg_data_home, dirpath);
	tle_db->read_from_xdg = true;
	tle_db_update(TEST_TLE_DIR "newer_tles/amateur.txt", tle_db, update_status);
	char updatefile[MAX_NUM_CHARS];
	bool updatefile_set = false;
	for (int i=0; i < tle_db->num_tles; i++) {
		if (update_status[i] & TLE_DB_UPDATED) {
			updated = true;
			assert_true(update_status[i] & TLE_IN_NEW_FILE);
			assert_false(update_status[i] & TLE_FILE_UPDATED);

			if (!updatefile_set) {
				updatefile_set = true;
				strncpy(updatefile, tle_db->tles[i].filename, MAX_NUM_CHARS);
			}
		}
		update_status[i] = 0;
	}
	assert_true(updated);
	unlink(updatefile);

	//unwritable location for all, non-writable location for XDG_DATA_HOME
	tle_db_from_file("/dev/NULL", tle_db);
	tle_db_from_directory(TEST_TLE_DIR "old_tles/", tle_db);
	tle_db->read_from_xdg = true;
	will_return(xdg_data_home, "/dev/NULL");
	set_filenames(tle_db, "/dev/NULL");
	updated = false;
	tle_db_update(TEST_TLE_DIR "newer_tles/amateur.txt", tle_db, update_status);
	for (int i=0; i < tle_db->num_tles; i++) {
		assert_false(update_status[i] & TLE_IN_NEW_FILE);
		assert_false(update_status[i] & TLE_FILE_UPDATED);
		if (update_status[i] & TLE_DB_UPDATED) {
			updated = true;
		}
		update_status[i] = 0;
	}
	assert_true(updated);

	//cleanup folders
	rmdir(tle_path);
	snprintf(tle_path, MAX_NUM_CHARS, "%s/flyby", temp_dir);
	rmdir(tle_path);
	rmdir(temp_dir);
	free(update_status);
}

int main()
{
	struct CMUnitTest tests[] = {cmocka_unit_test(test_tle_db_add_entry),
	cmocka_unit_test(test_tle_db_find_entry),
	cmocka_unit_test(test_tle_db_from_file),
	cmocka_unit_test(test_tle_db_overwrite_entry),
	cmocka_unit_test(test_tle_db_entry_is_newer_than),
	cmocka_unit_test(test_tle_db_from_directory),
	cmocka_unit_test(test_tle_db_filenames),
	cmocka_unit_test(test_whitelist_from_file),
	cmocka_unit_test(test_tle_db_to_file),
	cmocka_unit_test(test_tle_db_merge),
	cmocka_unit_test(test_whitelist_from_search_paths),
	cmocka_unit_test(test_tle_db_update),
	cmocka_unit_test(test_tle_db_from_search_paths),
	cmocka_unit_test(test_tle_db_enabled)
	};

	int rc = cmocka_run_group_tests(tests, NULL, NULL);
	return rc;
}
