#include <stdio.h>
#include <stdlib.h>
#include "xdg_basedirs.h"
#include <string.h>

/**
 * Functions related to xdg_basedirs.c, but cannot be placed there due to conflicts in test code when
 * mocking up the xdg basedir definitions.
 **/

char *settings_filepath(const char *settings_filename)
{
	create_xdg_dirs();
	char *config_home = xdg_config_home();

	int ret_size = strlen(config_home) + strlen(settings_filename) + 1;
	char *ret_str = (char*)malloc(sizeof(char)*ret_size);

	snprintf(ret_str, ret_size, "%s%s", config_home, settings_filename);

	free(config_home);
	return ret_str;
}
