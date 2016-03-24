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

#define DEFAULT_XDG_DATA_DIRS "/usr/local/share/:/usr/share"
#define DEFAULT_XDG_CONFIG_DIRS "/etc/xdg/"
#define DEFAULT_XDG_DATA_HOME ".local/share/"
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

	//return $HOME/.local/share if not
	setenv("XDG_CONFIG_HOME", "", 1);
	setenv("HOME", ".", 1);
	assert_string_equal(xdg_config_home(), "./" DEFAULT_XDG_CONFIG_HOME);
	unsetenv("XDG_CONFIG_HOME");
	assert_string_equal(xdg_config_home(), "./" DEFAULT_XDG_CONFIG_HOME);
}

int main()
{
	//XDG_DATA_DIRS
	//XDG_DATA_HOME
	//XDG_CONFIG_DIRS
	//XDG_CONFIG_HOME


	struct CMUnitTest tests[] = {cmocka_unit_test(test_directory_exists),
		cmocka_unit_test(test_xdg_data_dirs),
		cmocka_unit_test(test_xdg_config_dirs),
		cmocka_unit_test(test_xdg_config_home),
		cmocka_unit_test(test_xdg_data_home)};
	int rc = cmocka_run_group_tests(tests, NULL, NULL);
	return rc;
	return 0;
}
