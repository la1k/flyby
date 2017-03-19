#include "transponder_db.h"
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

#define TEST_DATA_DIR "test_data/"
void test_transponder_db_from_file(void **param)
{
	struct tle_db *tle_db = tle_db_create();
	struct transponder_db *transponder_db = transponder_db_create();

	//check loading from non-existing file
	assert_int_equal(transponder_db_from_file("/dev/NULL", tle_db, transponder_db, LOCATION_DATA_HOME), -1);
	assert_int_equal(transponder_db->num_sats, 0);

	//check loading from existing file when no TLE entries are defined
	assert_int_equal(transponder_db_from_file(TEST_DATA_DIR "flyby/flyby.db", tle_db, transponder_db, LOCATION_DATA_HOME), 0);
	assert_int_equal(transponder_db->num_sats, 0);
	for (int i=0; i < MAX_NUM_SATS; i++) {
		assert_true(transponder_db_entry_empty(&(transponder_db->sats[i])));
	}

	//check loading of transponder file
	tle_db_from_file(TEST_DATA_DIR "old_tles/part1.tle", tle_db);
	assert_int_equal(transponder_db_from_file(TEST_DATA_DIR "flyby/flyby.db", tle_db, transponder_db, LOCATION_DATA_HOME), 0);
	assert_int_equal(transponder_db->num_sats, tle_db->num_tles);

	//get database indices for satellites pre-defined in file
	//1: empty entry, 2: 1 transponder defined, 3: squint angle defined.
	long defined_sats[3] = {32785, 33493, 33499};
	int sat_ind[3];
	for (int i=0; i < 3; i++) {
		sat_ind[i] = tle_db_find_entry(tle_db, defined_sats[i]);
		assert_int_not_equal(sat_ind[i], -1);
		assert_int_equal(transponder_db->sats[sat_ind[i]].location, LOCATION_DATA_HOME);
	}

	//check that fields were read correctly
	assert_true(transponder_db_entry_empty(&(transponder_db->sats[sat_ind[0]])));
	assert_int_equal(transponder_db->sats[sat_ind[1]].num_transponders, 1);
	assert_string_equal(transponder_db->sats[sat_ind[1]].transponder_name[0], "test_1");
	assert_int_equal(transponder_db->sats[sat_ind[2]].num_transponders, 0);
	assert_true(transponder_db->sats[sat_ind[2]].squintflag);

	for (int i=0; i < transponder_db->num_sats; i++) {
		if ((i != sat_ind[0]) && (i != sat_ind[1]) && (i != sat_ind[2])) {
			assert_true(transponder_db_entry_empty(&(transponder_db->sats[i])));
			assert_int_equal(transponder_db->sats[i].location, 0);
		}
	}

	//check flag combination
	transponder_db_from_file(TEST_DATA_DIR "flyby/flyby.db", tle_db, transponder_db, LOCATION_DATA_DIRS);
	for (int i=0; i < 3; i++) {
		assert_true(transponder_db->sats[sat_ind[i]].location & LOCATION_DATA_HOME);
		assert_true(transponder_db->sats[sat_ind[i]].location & LOCATION_DATA_DIRS);
	}

	transponder_db_destroy(&transponder_db);
	tle_db_destroy(&tle_db);
}

void test_transponder_db_to_file(void **param)
{
	struct tle_db *tle_db = tle_db_create();
	tle_db_from_file(TEST_DATA_DIR "old_tles/part1.tle", tle_db);
	assert_true(tle_db->num_tles > 0);
	struct transponder_db *write_db = transponder_db_create();
	transponder_db_from_file("/dev/NULL", tle_db, write_db, LOCATION_DATA_HOME);
	
	//create transponder entry
	write_db->sats[0].num_transponders = 1;
	strncpy(write_db->sats[0].transponder_name[0], "test", MAX_NUM_CHARS);
	write_db->sats[0].downlink_start[0] = 1;
	write_db->sats[0].downlink_end[0] = 1;
	write_db->sats[0].uplink_start[0] = 1;
	write_db->sats[0].uplink_end[0] = 1;

	//set two first entries to be written to file
	bool should_write[MAX_NUM_SATS] = {0};
	should_write[0] = true; //non-empty entry
	should_write[1] = true; //empty entry

	//write TLE db to temporary file
	char filename[L_tmpnam] = "/tmp/XXXXXX";
	int fid = mkstemp(filename);
	assert_true(fid != -1);
	assert_true(strlen(filename) > 0);
	transponder_db_to_file("/dev/NULL", tle_db, write_db, should_write);
	transponder_db_to_file(filename, tle_db, write_db, should_write);
	transponder_db_destroy(&write_db);

	//check contents in file
	struct transponder_db *read_db = transponder_db_create();
	assert_int_equal(transponder_db_from_file(filename, tle_db, read_db, LOCATION_DATA_HOME), 0);
	assert_int_equal(read_db->sats[0].location, LOCATION_DATA_HOME);
	assert_int_equal(read_db->sats[1].location, LOCATION_DATA_HOME);
	for (int i=2; i < tle_db->num_tles; i++) {
		assert_int_equal(read_db->sats[i].location, 0);
	}
	assert_true(transponder_db_entry_empty(&(read_db->sats[1])));
	unlink(filename);
}

void test_transponder_db_write_to_default(void **param)
{
	struct tle_db *tle_db = tle_db_create();
	tle_db_from_file(TEST_DATA_DIR "newer_tles/amateur.txt", tle_db);
	assert_true(tle_db->num_tles > 0);
	struct transponder_db *write_db = transponder_db_create();
	transponder_db_from_file("/dev/NULL", tle_db, write_db, LOCATION_DATA_HOME);

	//create non-empty entries with the various location flags
	//setting only squintflag in order make entry non-empty
	int sat_ind = 0;
	write_db->sats[sat_ind].squintflag = true;
	write_db->sats[sat_ind++].location = LOCATION_NONE;
	
	write_db->sats[sat_ind].squintflag = true;
	write_db->sats[sat_ind++].location = LOCATION_TRANSIENT;
		
	write_db->sats[sat_ind].squintflag = true;
	write_db->sats[sat_ind++].location = LOCATION_DATA_HOME;
	
	write_db->sats[sat_ind].squintflag = true;
	write_db->sats[sat_ind++].location = LOCATION_DATA_DIRS;
	
	write_db->sats[sat_ind].squintflag = true;
	write_db->sats[sat_ind++].location = LOCATION_DATA_DIRS | LOCATION_DATA_HOME;
	
	write_db->sats[sat_ind].squintflag = true;
	write_db->sats[sat_ind++].location = LOCATION_DATA_DIRS | LOCATION_TRANSIENT;

	//create empty entries
	write_db->sats[sat_ind++].location = LOCATION_NONE;
	write_db->sats[sat_ind++].location = LOCATION_TRANSIENT;
	write_db->sats[sat_ind++].location = LOCATION_DATA_HOME;
	write_db->sats[sat_ind++].location = LOCATION_DATA_DIRS;
	write_db->sats[sat_ind++].location = LOCATION_DATA_DIRS | LOCATION_DATA_HOME;
	write_db->sats[sat_ind++].location = LOCATION_DATA_DIRS | LOCATION_TRANSIENT;

	assert_true(write_db->num_sats >= sat_ind);

	//create temporary directory as xdg_data_home
	char temp_dir[] = "/tmp/flybytestXXXXXX";
	mkdtemp(temp_dir);
	
	char data_home[MAX_NUM_CHARS];
	snprintf(data_home, MAX_NUM_CHARS, "%s/", temp_dir);
	will_return(xdg_data_home, data_home);

	char flyby_path[MAX_NUM_CHARS];
	snprintf(flyby_path, MAX_NUM_CHARS, "%sflyby/", data_home);
	mkdir(flyby_path, 0777);

	transponder_db_write_to_default(tle_db, write_db);
	transponder_db_destroy(&write_db);

	//read back written database
	struct transponder_db *read_db = transponder_db_create();
	char filename[MAX_NUM_CHARS];
	snprintf(filename, MAX_NUM_CHARS, "%sflyby.db", flyby_path);
	assert_int_equal(transponder_db_from_file(filename, tle_db, read_db, LOCATION_DATA_HOME), 0);

	sat_ind = 0;
	//non-empty entries
	assert_int_equal(read_db->sats[sat_ind++].location, 0);
	assert_int_equal(read_db->sats[sat_ind++].location, LOCATION_DATA_HOME);
	assert_int_equal(read_db->sats[sat_ind++].location, LOCATION_DATA_HOME);
	assert_int_equal(read_db->sats[sat_ind++].location, 0);
	assert_int_equal(read_db->sats[sat_ind++].location, LOCATION_DATA_HOME);
	assert_int_equal(read_db->sats[sat_ind++].location, LOCATION_DATA_HOME);

	//empty entries
	assert_int_equal(read_db->sats[sat_ind++].location, 0);
	assert_int_equal(read_db->sats[sat_ind++].location, LOCATION_DATA_HOME);
	assert_int_equal(read_db->sats[sat_ind++].location, 0);
	assert_int_equal(read_db->sats[sat_ind++].location, 0);
	assert_int_equal(read_db->sats[sat_ind++].location, LOCATION_DATA_HOME);
	assert_int_equal(read_db->sats[sat_ind++].location, LOCATION_DATA_HOME);

	tle_db_destroy(&tle_db);
	transponder_db_destroy(&read_db);

	unlink(filename);
	rmdir(flyby_path);
	rmdir(data_home);
}

void test_transponder_db_from_search_paths(void **param)
{
	struct tle_db *tle_db = tle_db_create();
	struct transponder_db *transponder_db = transponder_db_create();
	tle_db_from_file(TEST_DATA_DIR "old_tles/part1.tle", tle_db);
	
	//prepare database indices for satellites pre-defined in file
	long defined_sats[3] = {32785, 33493, 33499};
	int sat_ind[3];
	for (int i=0; i < 3; i++) {
		sat_ind[i] = tle_db_find_entry(tle_db, defined_sats[i]);
		assert_int_not_equal(sat_ind[i], -1);
	}

	//read transponder database from search paths
	
	//1: Transponder database defined in XDG_DATA_DIRS
	will_return(xdg_data_dirs, TEST_DATA_DIR);
	will_return(xdg_data_home, "/dev/NULL");
	transponder_db_from_search_paths(tle_db, transponder_db);
	assert_int_equal(transponder_db->sats[sat_ind[0]].location, LOCATION_NONE | LOCATION_DATA_DIRS);
	assert_int_equal(transponder_db->sats[sat_ind[1]].location, LOCATION_NONE | LOCATION_DATA_DIRS);
	assert_int_equal(transponder_db->sats[sat_ind[2]].location, LOCATION_NONE | LOCATION_DATA_DIRS);
	
	//2: Transponder database defined in XDG_DATA_HOME
	will_return(xdg_data_dirs, "/dev/NULL");
	will_return(xdg_data_home, TEST_DATA_DIR);
	transponder_db_from_search_paths(tle_db, transponder_db);
	assert_int_equal(transponder_db->sats[sat_ind[0]].location, LOCATION_NONE | LOCATION_DATA_HOME);
	assert_int_equal(transponder_db->sats[sat_ind[1]].location, LOCATION_NONE | LOCATION_DATA_HOME);
	assert_int_equal(transponder_db->sats[sat_ind[2]].location, LOCATION_NONE | LOCATION_DATA_HOME);
	
	//3: Transponder database defined in XDG_DATA_DIRS and XDG_DATA_HOME
	will_return(xdg_data_dirs, TEST_DATA_DIR);
	will_return(xdg_data_home, TEST_DATA_DIR);
	transponder_db_from_search_paths(tle_db, transponder_db);
	assert_int_equal(transponder_db->sats[sat_ind[0]].location, LOCATION_NONE | LOCATION_DATA_HOME | LOCATION_DATA_DIRS);
	assert_int_equal(transponder_db->sats[sat_ind[1]].location, LOCATION_NONE | LOCATION_DATA_HOME | LOCATION_DATA_DIRS);
	assert_int_equal(transponder_db->sats[sat_ind[2]].location, LOCATION_NONE | LOCATION_DATA_HOME | LOCATION_DATA_DIRS);

	tle_db_destroy(&tle_db);
	transponder_db_destroy(&transponder_db);
}

void test_transponder_db_entry_empty(void **param)
{
	struct sat_db_entry entry = {0};
	assert_true(transponder_db_entry_empty(&entry));

	//entry should be empty as long as no uplink or downlink are defined
	entry.num_transponders = 5;
	assert_true(transponder_db_entry_empty(&entry));

	strncpy(entry.transponder_name[0], "test", MAX_NUM_CHARS);
	assert_true(transponder_db_entry_empty(&entry));

	//test downlink configurations
	entry.downlink_start[0] = 1000;
	assert_false(transponder_db_entry_empty(&entry));

	entry.downlink_start[0] = 0.0;
	assert_true(transponder_db_entry_empty(&entry));

	entry.downlink_end[0] = 1000;
	assert_true(transponder_db_entry_empty(&entry));

	//test uplink configurations
	entry.uplink_start[0] = 1000;
	assert_false(transponder_db_entry_empty(&entry));

	entry.uplink_start[0] = 0.0;
	assert_true(transponder_db_entry_empty(&entry));

	entry.uplink_end[0] = 1000;
	assert_true(transponder_db_entry_empty(&entry));

	entry.num_transponders = 0;
	assert_true(transponder_db_entry_empty(&entry));

	//entry will be non-empty if squintflag is defined
	entry.squintflag = true;
	assert_false(transponder_db_entry_empty(&entry));
}

void test_transponder_db_entry_equal(void **param)
{
	struct sat_db_entry entry_1 = {0};
	struct sat_db_entry entry_2 = {0};

	assert_true(transponder_db_entry_equal(&entry_1, &entry_2));

	entry_1.downlink_start[0] = 1000;
	assert_true(transponder_db_entry_equal(&entry_1, &entry_2));
}

void test_transponder_db_entry_copy(void **param)
{
	struct sat_db_entry entry_1 = {0};
	struct sat_db_entry entry_2 = {0};

	entry_1.num_transponders = 5;
	strncpy(entry_1.transponder_name[3], "test", MAX_NUM_CHARS);
	entry_1.uplink_start[3] = 1000;

	assert_false(transponder_db_entry_equal(&entry_1, &entry_2));
	transponder_db_entry_copy(&entry_2, &entry_1);
	assert_true(transponder_db_entry_equal(&entry_1, &entry_2));
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

int main()
{
	struct CMUnitTest tests[] = {
		cmocka_unit_test(test_transponder_db_from_file),
		cmocka_unit_test(test_transponder_db_to_file),
		cmocka_unit_test(test_transponder_db_write_to_default),
		cmocka_unit_test(test_transponder_db_entry_empty),
		cmocka_unit_test(test_transponder_db_from_search_paths),
		cmocka_unit_test(test_transponder_db_entry_equal),
		cmocka_unit_test(test_transponder_db_entry_copy)
	};

	int rc = cmocka_run_group_tests(tests, NULL, NULL);
	return rc;
}
