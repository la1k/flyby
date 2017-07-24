#ifndef QTH_CONFIG_H_DEFINED
#define QTH_CONFIG_H_DEFINED

#include <predict/predict.h>

/**
 * Used for determining from where the QTH file was read.
 **/
enum qth_file_state {
	QTH_FILE_HOME, //read from XDG_CONFIG_HOME
	QTH_FILE_SYSTEMWIDE, //read from XDG_CONFIG_DIRS
	QTH_FILE_NOTFOUND //not found
};

/**
 * Read flyby.qth from XDG filepaths. Try XDG_CONFIG_HOME/flyby/flyby.qth first, then the paths in XDG_CONFIG_DIRS/flyby/flyby.qth.
 *
 * \param ret_observer Returned QTH information
 * \return Where the QTH file was read from: user home, system dir or not found at all
 **/
enum qth_file_state qth_from_search_paths(predict_observer_t *ret_observer);

/**
 * Read QTH information from file.
 *
 * \param qth_file QTH config file
 * \param ret_observer Returned observer structure
 * \return 0 on success, -1 otherwise
 **/
int qth_from_file(const char *qth_file, predict_observer_t *ret_observer);

/**
 * Write QTH information to specified file.
 *
 * \param qth_path File path
 * \param qth QTH information to write
 **/
void qth_to_file(const char *qth_path, predict_observer_t *qth);

/**
 * Get local user qth filepath (XDG_CONFIG_HOME/flyby/flyby.qth, AKA ~/.config/flyby/flyby.qth). Creates the directory if missing.
 **/
char* qth_default_writepath();

#endif
