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


void maidenhead_to_latlon_yields_same_maidenhead(void **params)
{
	//establish a correct longitude/latitude -> locator correspondence
	double latitude = 63.422;
	double longitude = 10.39;
	char expected_locator[] = "JP53ek";
	char locator[MAX_NUM_CHARS] = {0};
	latlon_to_maidenhead(latitude, longitude, locator);
	assert_string_equal(expected_locator, locator);

	//get coordinates from locator
	double newlat = 0, newlon = 0;
	maidenhead_to_latlon(expected_locator, &newlon, &newlat);

	//check that coordinates roughly correspond
	assert_int_equal(floor(latitude), floor(newlat));
	assert_int_equal(floor(longitude), floor(newlon));

	//check that we get the same locator string from these coordinates
	char newloc[MAX_NUM_CHARS] = {0};
	latlon_to_maidenhead(newlat, newlon, newloc);
	assert_string_equal(expected_locator, newloc);
}

int main()
{
	struct CMUnitTest tests[] = {
		cmocka_unit_test(maidenhead_to_latlon_yields_same_maidenhead)
	};

	int rc = cmocka_run_group_tests(tests, NULL, NULL);
	return rc;
}
