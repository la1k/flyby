#include "xdg_basedirs.h"
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <predict/predict.h>
#include <math.h>
#include "qth_config.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

/**
 * Mockups of XDG functions.
 **/

char* xdg_config_home()
{
	return strdup((char*)mock());
}

char* xdg_config_dirs()
{
	return strdup((char*)mock());
}

void create_xdg_dirs()
{
}

void test_qth_readwrite()
{
	//bootstrap later equality checking: Check that the two observers are inequal now.
	predict_observer_t *observer = predict_create_observer("test", 60*M_PI/180.0, 10*M_PI/180, 100);
	predict_observer_t *read_observer = predict_create_observer("test2", 0, 0, 0);
	assert_string_not_equal(observer->name, read_observer->name);
	assert_false(observer->longitude == read_observer->longitude);
	assert_false(observer->latitude == read_observer->latitude);
	assert_false(observer->altitude == read_observer->altitude);

	char qth_file[L_tmpnam] = "/tmp/XXXXXX";
	mkstemp(qth_file);
	assert_true(strlen(qth_file) > 0);

	//write one observer to file, read back the other. Check that they are equal
	qth_to_file(qth_file, observer);
	int retval = qth_from_file(qth_file, read_observer);
	assert_int_equal(retval, 0);
	assert_string_equal(observer->name, read_observer->name);
	assert_true(observer->longitude == read_observer->longitude);
	assert_true(observer->latitude == read_observer->latitude);
	assert_true(observer->altitude == read_observer->altitude);

	predict_destroy_observer(observer);
	predict_destroy_observer(read_observer);

	unlink(qth_file);
}

void test_qth_writepath(void **params)
{
	//check that correct path is constructed
	will_return(xdg_config_home, "/tmp/");
	char* qth_path = qth_default_writepath();
	assert_non_null(qth_path);
	assert_string_equal(qth_path, "/tmp/flyby/flyby.qth");
	free(qth_path);
}

#define TESTFILE_QTH_PATH "test_data/"
void test_qth_from_search_paths(void **params)
{
	//no QTH file defined
	will_return(xdg_config_home, "/dev/NULL/");
	will_return(xdg_config_dirs, "/dev/NULL/");
	predict_observer_t *observer = predict_create_observer("test", 0, 0, 0);
	assert_int_equal(qth_from_search_paths(observer), QTH_FILE_NOTFOUND);

	//QTH file only in xdg_config_home
	will_return(xdg_config_home, TESTFILE_QTH_PATH);
	assert_int_equal(qth_from_search_paths(observer), QTH_FILE_HOME);

	//QTH file only in xdg_config_dirs
	will_return(xdg_config_home, "/dev/NULL/");
	will_return(xdg_config_dirs, TESTFILE_QTH_PATH);
	assert_int_equal(qth_from_search_paths(observer), QTH_FILE_SYSTEMWIDE);

	predict_destroy_observer(observer);
}

int main()
{
	struct CMUnitTest tests[] = {cmocka_unit_test(test_qth_readwrite),
	cmocka_unit_test(test_qth_writepath),
	cmocka_unit_test(test_qth_from_search_paths)};

	int rc = cmocka_run_group_tests(tests, NULL, NULL);
	return rc;
}
