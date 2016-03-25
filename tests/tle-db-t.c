#include "tle_db.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

void test_tle_db_find_entry(void **param)
{
	struct tle_db *tle_db = tle_db_create();
	tle_db->num_tles = 100;
	tle_db->tles[50].satellite_number = 100;

	assert_int_equal(tle_db_find_entry(tle_db, 100), 50);
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

void test_tle_db_create(void **param)
{
	struct tle_db *tle_db_1 = tle_db_create();
	struct tle_db *tle_db_2 = tle_db_create();

	assert_int_equal(tle_db_1->num_tles, 0);
	assert_int_equal(tle_db_2->num_tles, 0);
	assert_memory_equal((char*)tle_db_1, (char*)tle_db_2, sizeof(struct tle_db));

	tle_db_destroy(&tle_db_1);
	tle_db_destroy(&tle_db_2);
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
	int index = 10;
	tle_db->num_tles = index+1;
	tle_db_overwrite_entry(index, tle_db, &entry_1);

	assert_true(entry_1.satellite_number == tle_db->tles[index].satellite_number);
	assert_string_equal(tle_db->tles[index].name, name);
	assert_string_equal(tle_db->tles[index].line1, line1);
	assert_string_equal(tle_db->tles[index].line2, line2);
	assert_int_equal(tle_db->num_tles, index+1);
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

	tle_db_from_directory("/dev/NULL", tle_db);
	assert_int_equal(tle_db->num_tles, 0);

	tle_db_from_directory(TEST_TLE_DIR "old_tles/", tle_db);
	assert_int_equal(tle_db->num_tles, 16);

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
	for (int i=0; i < merged->num_tles; i++) {
		long satellite_number = merged->tles[i].satellite_number;
		int corr_entry = tle_db_find_entry(tle_db_1, satellite_number);
		if (corr_entry != -1) {
			if (!tle_db_entry_is_newer_than(merged->tles[i], tle_db_1->tles[corr_entry])) {
				assert_string_equal(merged->tles[i].line1, tle_db_1->tles[corr_entry].line1);
				assert_string_equal(merged->tles[i].line2, tle_db_1->tles[corr_entry].line2);
			}
		}
	}

	//check the same, but with opposite order of merge
	tle_db_from_file("/dev/NULL", merged);
	tle_db_merge(tle_db_2, merged, TLE_OVERWRITE_OLD);
	tle_db_merge(tle_db_1, merged, TLE_OVERWRITE_NONE);
	assert_int_equal(merged->num_tles, expected_num_tles);
	for (int i=0; i < merged->num_tles; i++) {
		long satellite_number = merged->tles[i].satellite_number;
		int corr_entry = tle_db_find_entry(tle_db_1, satellite_number);
		if (corr_entry != -1) {
			if (!tle_db_entry_is_newer_than(merged->tles[i], tle_db_1->tles[corr_entry])) {
				assert_string_equal(merged->tles[i].line1, tle_db_1->tles[corr_entry].line1);
				assert_string_equal(merged->tles[i].line2, tle_db_1->tles[corr_entry].line2);
			}
		}
	}

	tle_db_destroy(&tle_db_1);
	tle_db_destroy(&tle_db_2);
	tle_db_destroy(&merged);
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
	cmocka_unit_test(test_tle_db_create),
	cmocka_unit_test(test_whitelist_from_file),
	cmocka_unit_test(test_tle_db_to_file),
	cmocka_unit_test(test_tle_db_merge),
	cmocka_unit_test(test_whitelist_to_file),
	cmocka_unit_test(test_tle_db_enabled)
	};


	int rc = cmocka_run_group_tests(tests, NULL, NULL);
	return rc;
}
