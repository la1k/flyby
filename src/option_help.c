#include <string.h>
#include <stdbool.h>
#include "option_help.h"
#include "defines.h"
#include <stdio.h>
#include <stdlib.h>

bool has_short_option(const char *short_options, struct option long_option)
{
	const char *ptr = strchr(short_options, long_option.val);
	if (ptr == NULL) {
		return false;
	}
	return true;
}

void fill_spaces(char *string, int length, int num_spaces)
{
	if (num_spaces > length) {
		num_spaces = length-1;
	}

	for (int i=0; i < num_spaces; i++) {
		string[i] = ' ';
	}
	string[num_spaces] = '\0';
}

struct option *extended_to_longopts(struct option_extended *options)
{
	size_t num_options = 0;
	while (true) {
		if (options[num_options].option.name == NULL) {
			break;
		}
		num_options++;
	}

	struct option *ret_options = (struct option*)malloc(sizeof(struct option)*num_options);
	for (int i=0; i < num_options; i++) {
		ret_options[i] = options[i].option;
	}
	return ret_options;
}

void getopt_long_show_help(const char *usage_instructions, struct option_extended long_options[], const char *short_options)
{
	//find maximum length of long options
	int max_length = 0;
	int index = 0;
	while (true) {
		const char *name = long_options[index].option.name;
		if (name == 0) {
			break;
		}

		int length = strlen(name);
		if (length > max_length) {
			max_length = length;
		}
		index++;
	}
	max_length += 6; //extra signs that are displayed after the long option in --help
	max_length += 8;

	//display initial description
	index = 0;
	printf("%s\n\n", usage_instructions);
	while (true) {
		char option_print_full[MAX_NUM_CHARS] = {0};
		fill_spaces(option_print_full, MAX_NUM_CHARS, MAX_NUM_CHARS);
		char *option_print = option_print_full;

		if (long_options[index].option.name == 0) {
			break;
		}

		//display short option
		if (has_short_option(short_options, long_options[index].option)) {
			option_print += snprintf(option_print, MAX_NUM_CHARS, " -%c,", long_options[index].option.val);
		} else {
			option_print += snprintf(option_print, MAX_NUM_CHARS, "    ");
		}

		//display long option
		option_print += snprintf(option_print, MAX_NUM_CHARS, "--%s", long_options[index].option.name);

		//display argument
		switch (long_options[index].option.has_arg) {
			case required_argument:
				option_print += snprintf(option_print, MAX_NUM_CHARS, "=%s", long_options[index].argument);
			break;

			case optional_argument:
				option_print += snprintf(option_print, MAX_NUM_CHARS, "=%s (optional)", long_options[index].argument);
			break;
		}
		*option_print = ' ';

		option_print = option_print_full + max_length;
		if (long_options[index].description != NULL) {
			snprintf(option_print, MAX_NUM_CHARS, " %s", long_options[index].description);
		} else {
			snprintf(option_print, MAX_NUM_CHARS, " Documentation missing.");
		}
		printf("%s\n", option_print_full);
		index++;
	}
}
