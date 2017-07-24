#include "xdg_basedirs.h"
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

bool directory_exists(const char *dirpath)
{
	struct stat s;
	int err = stat(dirpath, &s);
	if ((err == -1) && (errno == ENOENT)) {
		return false;
	}
	return true;
}

void test_directory_exists(void **param)
{
	assert_true(directory_exists("."));
	assert_false(directory_exists("/dev/NULL"));
}

#define DEFAULT_XDG_DATA_DIRS "/usr/local/share/:/usr/share/"
#define DEFAULT_XDG_CONFIG_DIRS "/etc/xdg/"
#define DEFAULT_XDG_DATA_HOME_BASE ".local/"
#define DEFAULT_XDG_DATA_HOME DEFAULT_XDG_DATA_HOME_BASE "share/"
#define DEFAULT_XDG_CONFIG_HOME ".config/"
#define TMP_DIR "/tmp/"

void test_xdg_data_dirs(void **param)
{
	//return $XDG_DATA_DIRS if defined
	setenv("XDG_DATA_DIRS", TMP_DIR, 1);
	assert_string_equal(xdg_data_dirs(), TMP_DIR);

	//return /usr/local/share:/usr/share if empty or not defined
	setenv("XDG_DATA_DIRS", "", 1);
	assert_string_equal(xdg_data_dirs(), DEFAULT_XDG_DATA_DIRS);
	unsetenv("XDG_DATA_DIRS");
	assert_string_equal(xdg_data_dirs(), DEFAULT_XDG_DATA_DIRS);
}

void test_xdg_config_dirs(void **param)
{
	//return XDG_CONFIG_DIRS if defined
	setenv("XDG_CONFIG_DIRS", "/tmp/", 1);
	assert_string_equal(xdg_config_dirs(), "/tmp/");

	setenv("XDG_CONFIG_DIRS", "/tmp", 1);
	assert_string_equal(xdg_config_dirs(), "/tmp/");

	//return /etc/xdg/ if empty or not defined
	setenv("XDG_CONFIG_DIRS", "", 1);
	assert_string_equal(xdg_config_dirs(), DEFAULT_XDG_CONFIG_DIRS);
	unsetenv("XDG_CONFIG_DIRS");
	assert_string_equal(xdg_config_dirs(), DEFAULT_XDG_CONFIG_DIRS);
}

void test_xdg_data_home(void **param)
{
	//return XDG_DATA_HOME if defined
	setenv("XDG_DATA_HOME", "/tmp/", 1);
	assert_string_equal(xdg_data_home(), "/tmp/");

	setenv("XDG_DATA_HOME", "/tmp", 1);
	assert_string_equal(xdg_data_home(), "/tmp/");

	//return $HOME/.local/share if not
	setenv("XDG_DATA_HOME", "", 1);
	setenv("HOME", ".", 1);
	assert_string_equal(xdg_data_home(), "./" DEFAULT_XDG_DATA_HOME);
	unsetenv("XDG_DATA_HOME");
	assert_string_equal(xdg_data_home(), "./" DEFAULT_XDG_DATA_HOME);
}

void test_xdg_config_home(void **param)
{
	//return XDG_CONFIG_HOME if defined
	setenv("XDG_CONFIG_HOME", "/tmp/", 1);
	assert_string_equal(xdg_config_home(), "/tmp/");

	setenv("XDG_CONFIG_HOME", "/tmp", 1);
	assert_string_equal(xdg_config_home(), "/tmp/");

	//return $HOME/.local/share if not
	setenv("XDG_CONFIG_HOME", "", 1);
	setenv("HOME", ".", 1);
	assert_string_equal(xdg_config_home(), "./" DEFAULT_XDG_CONFIG_HOME);
	unsetenv("XDG_CONFIG_HOME");
	assert_string_equal(xdg_config_home(), "./" DEFAULT_XDG_CONFIG_HOME);
}

/**
 * Add extra tailing directory to a path string.
 *
 * \param basepath Base path
 * \param newdir Tailing directory
 * \return basepath/newdir
 **/
char *add_to_path(const char *basepath, const char *newdir)
{
	int length = strlen(basepath) + strlen(newdir) + 1;
	char *ret_string = malloc(length*sizeof(char));
	snprintf(ret_string, length, "%s/%s", basepath, newdir);
	return ret_string;
}

/**
 * Check that full flyby config/data directories (XDG_DATA_HOME/flyby/tles,
 * XDG_CONFIG_HOME/flyby) can be created given the current definitions of
 * XDG_DATA_HOME and XDG_CONFIG_HOME and remove the directories again. Called
 * from the test_create_xdg_dirs_when_*-functions.
 **/
void test_create_xdg_dirs()
{
	//construct expected paths
	char *xdg_data_home_basepath = xdg_data_home();
	char *xdg_config_home_basepath = xdg_config_home();
	char *flyby_config_dir = add_to_path(xdg_config_home_basepath, FLYBY_RELATIVE_ROOT_PATH);
	char *flyby_data_dir = add_to_path(xdg_data_home_basepath, FLYBY_RELATIVE_ROOT_PATH);
	char *flyby_tle_dir = add_to_path(xdg_data_home_basepath, TLE_RELATIVE_DIR_PATH);

	//check that paths do not exist yet
	assert_false(directory_exists(flyby_config_dir));
	assert_false(directory_exists(flyby_data_dir));
	assert_false(directory_exists(flyby_tle_dir));

	create_xdg_dirs();

	//check that paths have been created
	assert_true(directory_exists(flyby_config_dir));
	assert_true(directory_exists(flyby_data_dir));
	assert_true(directory_exists(flyby_tle_dir));

	//cleanup
	rmdir(flyby_tle_dir);
	rmdir(flyby_data_dir);
	rmdir(flyby_config_dir);
	
	assert_false(directory_exists(flyby_config_dir));
	assert_false(directory_exists(flyby_data_dir));
	assert_false(directory_exists(flyby_tle_dir));

	free(flyby_config_dir);
	free(flyby_data_dir);
	free(flyby_tle_dir);

	free(xdg_data_home_basepath);
	free(xdg_config_home_basepath);
}

void test_create_xdg_dirs_when_xdg_directories_are_welldefined(void **param)
{
	//create temporary directory for XDG_DATA_HOME and XDG_CONFIG_HOME
	char temp_xdg_data_home[] = "/tmp/flybyXXXXXXXX";
	mkdtemp(temp_xdg_data_home);
	setenv("XDG_DATA_HOME", temp_xdg_data_home, 1);
	assert_true(directory_exists(temp_xdg_data_home));

	char temp_xdg_config_home[] = "/tmp/flybyXXXXXXXX";
	mkdtemp(temp_xdg_config_home);
	setenv("XDG_CONFIG_HOME", temp_xdg_config_home, 1);
	assert_true(directory_exists(temp_xdg_config_home));

	//check that directories can be created correctly
	test_create_xdg_dirs();

	//cleanup
	rmdir(temp_xdg_data_home);
	rmdir(temp_xdg_config_home);

	assert_false(directory_exists(temp_xdg_data_home));
	assert_false(directory_exists(temp_xdg_config_home));
}

void test_create_xdg_dirs_when_dotlocal_and_dotconfig_have_not_been_created(void **param)
{
	//set temporary directory as HOME in order to ensure that .local and .config do not exist
	char *previous_home_dir = strdup(getenv("HOME"));
	char temp_home_dir[] = "/tmp/flybyXXXXXXXX";
	mkdtemp(temp_home_dir);
	setenv("HOME", temp_home_dir, 1);

	//ensure XDG_*_HOME are empty
	setenv("XDG_DATA_HOME", "", 1);
	setenv("XDG_CONFIG_HOME", "", 1);
	assert_true(directory_exists(temp_home_dir));

	char *data_home_path = xdg_data_home();
	char *config_home_path = xdg_config_home();
	assert_false(directory_exists(data_home_path));
	assert_false(directory_exists(config_home_path));

	//ensure that XDG_DATA_HOME and XDG_CONFIG_HOME are as expected
	char *expected_data_home_path = add_to_path(temp_home_dir, DEFAULT_XDG_DATA_HOME "/");
	char *expected_config_home_path = add_to_path(temp_home_dir, DEFAULT_XDG_CONFIG_HOME "/");
	assert_string_equal(data_home_path, expected_data_home_path);
	assert_string_equal(config_home_path, expected_config_home_path);

	free(expected_data_home_path);
	free(expected_config_home_path);

	//test directory creation
	test_create_xdg_dirs();

	//revert HOME variable
	setenv("HOME", previous_home_dir, 1);
	free(previous_home_dir);

	//clean up directories
	char *data_home_base_path = add_to_path(temp_home_dir, DEFAULT_XDG_DATA_HOME_BASE); //corresponds to (...)/.local
	rmdir(data_home_path);
	rmdir(data_home_base_path);
	rmdir(config_home_path);
	rmdir(temp_home_dir);
	assert_false(directory_exists(temp_home_dir));

	free(data_home_base_path);
	free(data_home_path);
	free(config_home_path);
}

int main()
{
	struct CMUnitTest tests[] = {cmocka_unit_test(test_directory_exists),
		cmocka_unit_test(test_xdg_data_dirs),
		cmocka_unit_test(test_xdg_config_dirs),
		cmocka_unit_test(test_xdg_config_home),
		cmocka_unit_test(test_xdg_data_home),
		cmocka_unit_test(test_create_xdg_dirs_when_xdg_directories_are_welldefined),
		cmocka_unit_test(test_create_xdg_dirs_when_dotlocal_and_dotconfig_have_not_been_created)
	};

	int rc = cmocka_run_group_tests(tests, NULL, NULL);
	return rc;
}
