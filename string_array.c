#include "string_array.h"
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include "flyby_defines.h"

int string_array_add(string_array_t *string_array, const char *string)
{
	//initialize
	if (string_array->available_size < 1) {
		string_array->strings = (char**)malloc(sizeof(char*));
		string_array->available_size = 1;
		string_array->num_strings = 0;
	}

	//extend size to twice the current size
	if (string_array->num_strings+1 > string_array->available_size) {
		char **temp = realloc(string_array->strings, sizeof(char*)*string_array->available_size*2);
		if (temp == NULL) {
			return -1;
		}
		string_array->available_size = string_array->available_size*2;
		string_array->strings = temp;
	}

	//copy string
	string_array->strings[string_array->num_strings] = (char*)malloc(sizeof(char)*MAX_NUM_CHARS);
	strncpy(string_array->strings[string_array->num_strings], string, MAX_NUM_CHARS);
	string_array->num_strings++;
	return 0;
}

const char* string_array_get(string_array_t *string_array, int index)
{
	if (index < string_array->num_strings) {
		return string_array->strings[index];
	}
	return NULL;
}

void string_array_free(string_array_t *string_array)
{
	for (int i=0; i < string_array->num_strings; i++) {
		free(string_array->strings[i]);
	}
	free(string_array->strings);
	string_array->num_strings = 0;
	string_array->available_size = 0;
	string_array->strings = NULL;
}

int string_array_size(string_array_t *string_array)
{
	return string_array->num_strings;
}

void stringsplit(const char *string_list, string_array_t *ret_string_list)
{
	char *copy = strdup(string_list);
	const char *delimiter = ":";
	char *token = strtok(copy, delimiter);
	while (token != NULL) {
		string_array_add(ret_string_list, token);
		token = strtok(NULL, delimiter);
	}
	free(copy);
}

