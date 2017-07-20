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
#include "locator.h"
#include "defines.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

void latlon_to_maidenhead_yields_expected_maidenhead(void **params)
{
	//establish a correct longitude/latitude -> locator correspondence
	double latitude = 63.422;
	double longitude = 10.39;
	char expected_locator[] = "JP53ek";
	char locator[MAX_NUM_CHARS] = {0};
	latlon_to_maidenhead(latitude, longitude, locator);
	assert_string_equal(expected_locator, locator);
}

void maidenhead_to_latlon_yields_same_maidenhead(void **params)
{
	//get coordinates from locator
	const char expected_locator[] = "JP53ek";
	double newlat = 0, newlon = 0;
	maidenhead_to_latlon(expected_locator, &newlon, &newlat);

	//check that we get the same locator string from these coordinates
	char newloc[MAX_NUM_CHARS] = {0};
	latlon_to_maidenhead(newlat, newlon, newloc);
	assert_string_equal(expected_locator, newloc);
}

void maidenhead_to_latlon_yields_same_maidenhead_at_differing_locator_lengths(void **params)
{
	const char locator[] = "JP53ek";
	char *expected_locator = strdup(locator);

	for (int locator_length = 1; locator_length <= strlen(locator); locator_length++) {
		strncpy(expected_locator, locator, locator_length);
		expected_locator[locator_length] = '\0';

		//get coordinates from locator
		double newlat = 0, newlon = 0;
		maidenhead_to_latlon(expected_locator, &newlon, &newlat);

		//check that we get the same locator string from these coordinates
		char newloc[MAX_NUM_CHARS] = {0};
		latlon_to_maidenhead(newlat, newlon, newloc);
		newloc[locator_length] = '\0';
		assert_string_equal(expected_locator, newloc);
	}

	free(expected_locator);
}

int main()
{
	struct CMUnitTest tests[] = {
		cmocka_unit_test(latlon_to_maidenhead_yields_expected_maidenhead),
		cmocka_unit_test(maidenhead_to_latlon_yields_same_maidenhead),
		cmocka_unit_test(maidenhead_to_latlon_yields_same_maidenhead_at_differing_locator_lengths)
	};

	int rc = cmocka_run_group_tests(tests, NULL, NULL);
	return rc;
}
