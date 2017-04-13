#ifndef OPTION_HELP_H_DEFINED

#include <getopt.h>

struct option_extended
{
	struct option option;
	const char *argument;
	const char *description;

};

struct option *extended_to_longopts(struct option_extended *options);

/**
 * Returns true if specified option's value is a char within the short options char array (and has thus a short-hand version of the long option)
 *
 * \param short_options Array over short option chars
 * \param long_option Option struct to check
 * \returns True if option has a short option, false otherwise
 **/
bool has_short_option(const char *short_options, struct option long_option);

/**
 * Print program usage instructions to stdout.
 *
 * \param General usage instructions
 * \param long_options List of long options used in getopts_long
 * \param short_options List of short options used in getopts_long
 * \param option_descriptions Description of each option
 **/
void getopt_long_show_help(const char *usage_instructions, struct option_extended long_options[], const char *short_options);

#endif
