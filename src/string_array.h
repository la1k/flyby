#ifndef STRING_ARRAY_H_DEFINED
#define STRING_ARRAY_H_DEFINED

/**
 * Dynamic size string array for the situations where we don't know in advance the number of filenames.
 * Used for accumulating TLE filenames. Exchange by std::vector if we start using C++ instead. :^)
 **/
typedef struct {
	int available_size; //available size within string array
	int num_strings; //current number of strings
	char **strings; //strings
} string_array_t;

/**
 * Add string to string array. Reallocates available space to twice the size when current available size is exceeded.
 *
 * \param string_array String array
 * \param string String to add
 * \return 0 on success, -1 on failure
 **/
int string_array_add(string_array_t *string_array, const char *string);

/**
 * Get string at specified index from string array.
 *
 * \param string_array String array
 * \param index Index at which we want to extract a string
 * \return String at specified index
 **/
const char* string_array_get(string_array_t *string_array, int index);

/**
 * Get string array size.
 *
 * \param string_array String array
 * \return Size
 **/
int string_array_size(string_array_t *string_array);

/**
 * Free memory allocated in string array.
 *
 * \param string_array String array to free
 **/
void string_array_free(string_array_t *string_array);

/**
 * Find string in string array.
 *
 * \param string_array String array
 * \param string String to find
 * \return Index in string array, -1 if string is not found
 **/
int string_array_find(string_array_t *string_array, const char *string);

/**
 * Split string at ':'-delimiter.
 * \param string_list Input string list delimited by :
 * \param ret_string_list Returned string array
 **/
void stringsplit(const char *string_list, string_array_t *ret_string_list);

/**
 * Set string at specified position to supplied string.
 *
 * \param string_array String array
 * \param i String array position to set
 * \param string String to set
 **/
void string_array_set(string_array_t *string_array, int i, const char *string);

#endif
