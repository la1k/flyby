#include "string_array.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

void test_string_array(void **param)
{
	//size
	string_array_t string_array = {0};
	assert_int_equal(string_array_size(&string_array), 0);
	assert_null(string_array_get(&string_array, 0));

	//add
	assert_int_equal(string_array_add(&string_array, ""), 0);
	assert_int_equal(string_array_add(&string_array, "test"), 0);
	assert_int_equal(string_array_size(&string_array), 2);

	//get
	assert_string_equal(string_array_get(&string_array, 0), "");
	assert_string_equal(string_array_get(&string_array, 1), "test");
	assert_null(string_array_get(&string_array, 2));

	//set
	string_array_set(&string_array, 0, "teststring");
	assert_string_equal(string_array_get(&string_array, 0), "teststring");

	//find
	assert_int_equal(string_array_find(&string_array, "test"), 1);
	assert_int_equal(string_array_find(&string_array, "teststri"), -1);

	//free
	string_array_free(&string_array);
	assert_int_equal(string_array_size(&string_array), 0);
	assert_null(string_array_get(&string_array, 0));
}

#define NUM_STRINGS 5
void test_stringsplit(void **param)
{
	string_array_t string_array = {0};

	//single string
	stringsplit("test", &string_array);
	assert_int_equal(string_array_size(&string_array), 1);

	//additional string to same array
	stringsplit("test", &string_array);
	assert_int_equal(string_array_size(&string_array), 2);
	assert_string_equal(string_array_get(&string_array, 0), "test");
	string_array_free(&string_array);

	//empty string
	stringsplit("", &string_array);
	assert_int_equal(string_array_size(&string_array), 0);

	//actual string case
	stringsplit("test1:test2", &string_array);
	assert_int_equal(string_array_size(&string_array), 2);
	assert_string_equal(string_array_get(&string_array, 0), "test1");
	assert_string_equal(string_array_get(&string_array, 1), "test2");
	string_array_free(&string_array);

	//tailing ':'
	stringsplit("test1:test2:", &string_array);
	assert_int_equal(string_array_size(&string_array), 2);
	assert_string_equal(string_array_get(&string_array, 0), "test1");
	assert_string_equal(string_array_get(&string_array, 1), "test2");
	string_array_free(&string_array);
}

int main()
{
	struct CMUnitTest tests[] = {cmocka_unit_test(test_string_array),
	cmocka_unit_test(test_stringsplit)};

	int rc = cmocka_run_group_tests(tests, NULL, NULL);
	return rc;
}
