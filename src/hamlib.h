#ifndef FLYBY_HAMLIB_H_DEFINED
#define FLYBY_HAMLIB_H_DEFINED

#include "defines.h"
#include <stdbool.h>
#include <time.h>
#include "string_array.h"

#define ROTCTLD_DEFAULT_HOST "localhost"
#define ROTCTLD_DEFAULT_PORT "4533"
#define RIGCTLD_DEFAULT_HOST "localhost"
#define RIGCTLD_DEFAULT_PORT "4532"

// TODO: Name
// TODO: Maybe socket should be a part of this?
// TODO: boolean that specifies whether response was received
typedef struct {
	int buffer_pos;
	char buffer[MAX_NUM_CHARS];
} buffer_t;

typedef struct {
	///Whether we are connected to a rotctld instance
	bool connected;
	///Socket fid for reading rotctld positions
	int read_socket;
	///Socket fid for setting rotctld positions
	int track_socket;
	///Hostname
	char host[MAX_NUM_CHARS];
	///Port
	char port[MAX_NUM_CHARS];
	///Horizon above which we start tracking. Defaults to 0.
	double tracking_horizon;
	///Whether first command has been sent, and whether we can guarantee that prev_cmd_azimuth/elevation contain correct values
	bool first_cmd_sent;
	///Previous sent azimuth
	double prev_cmd_azimuth;
	///Previous sent elevation
	double prev_cmd_elevation;
	// TODO: Description + var name
	// TODO: Maybe separate this into a different struct, and reuse
	// for rigctld_info_t
	bool last_track_response_received;
	buffer_t track_buffer;
	bool last_read_response_received;
	buffer_t read_buffer;
} rotctld_info_t;

typedef struct {
	///Whether we are connected to a rigctld instance
	bool connected;
	///Socket file identificator
	int socket;
	///Hostname
	char host[MAX_NUM_CHARS];
	///Port
	char port[MAX_NUM_CHARS];
	///VFO name
	char vfo_name[MAX_NUM_CHARS];
} rigctld_info_t;

/**
 * Rotctld connection error codes.
 **/
enum rotctld_error_e {
	ROTCTLD_NO_ERR = 0,
	ROTCTLD_GETADDRINFO_ERR = -1,
	ROTCTLD_CONNECTION_FAILED = -2,
	ROTCTLD_SEND_FAILED = -3,
	ROTCTLD_RETURNED_STATUS_ERROR = -4,
	ROTCTLD_READ_BUFFER_OVERFLOW = -5,
	ROTCTLD_READ_FAILED = -6,
};
typedef enum rotctld_error_e rotctld_error;

/**
 * Get error message corresponding to each error code.
 *
 * \param errorcode Rotctld error code
 * \return Error message
 **/
const char *rotctld_error_message(rotctld_error errorcode);

/**
 * Print an error message and exit if errorcode differs from ROTCTLD_NO_ERR.
 * Used for wrapping calls to the rotctld client if we want failures to make
 * flyby shut down ncurses and exit.
 *
 * \param errorcode Error code
 **/
void rotctld_fail_on_errors(rotctld_error errorcode);

/**
 * Connect to rotctld. 
 *
 * \param hostname Hostname/IP address
 * \param port Port
 * \param ret_info Returned rotctld connection instance
 **/
rotctld_error rotctld_connect(const char *hostname, const char *port, rotctld_info_t *ret_info);

/**
 * Disconnect from rotctld.
 *
 * \param info Rigctld connection instance
 **/
void rotctld_disconnect(rotctld_info_t *info);

/**
 * Send track data to rotctld.
 *
 * Data is sent only when input azi/ele differs from previously sent azi/ele
 *
 * \param info rotctld connection instance
 * \param azimuth Azimuth in degrees
 * \param elevation Elevation in degrees
 * \return ROTCTLD_NO_ERR on success
 **/
rotctld_error rotctld_track(rotctld_info_t *info, double azimuth, double elevation);

/**
 * Read current rotctld position.
 *
 * \param info Rotctld connection instance
 * \param ret_azimuth Returned azimuth angle
 * \param ret_elevation Returned elevation angle
 * \return ROTCTLD_NO_ERR on success
 **/
rotctld_error rotctld_read_position(rotctld_info_t *info, float *ret_azimuth, float *ret_elevation);

/**
 * Set current tracking horizon.
 *
 * \param info Rotctld connection instance
 * \param horizon Tracking horizon
 **/
void rotctld_set_tracking_horizon(rotctld_info_t *info, double horizon);

/**
 * Rigctld-related errors.
 **/
enum rigctld_error_e {
	RIGCTLD_NO_ERR = 0,
	RIGCTLD_GETADDRINFO_ERR = -1,
	RIGCTLD_CONNECTION_FAILED = -2,
	RIGCTLD_SEND_FAILED = -3,
};
typedef enum rigctld_error_e rigctld_error;

/**
 * Make flyby fail and shut down ncurses on rigctld errors.
 *
 * \param errorcode Error code
 **/
void rigctld_fail_on_errors(rigctld_error errorcode);

/**
 * Get error message related to input errorcode.
 *
 * \param errorcode Error code
 * \return Error message
 **/
const char *rigctld_error_message(rigctld_error errorcode);

/**
 * Connect to rigctld. 
 *
 * \param hostname Hostname/IP address
 * \param port Port
 * \param ret_info Returned rigctld connection instance
 * \return RIGCTLD_NO_ERR on success
 **/
rigctld_error rigctld_connect(const char *hostname, const char *port, rigctld_info_t *ret_info);

/**
 * Set VFO name to be used by this rigctld connection instance. Will not switch VFO in rigctld until set_frequency.
 *
 * \param ret_info Rigctld connection instance
 * \param vfo_name
 * \return RIGCTLD_NO_ERR on success
 **/
rigctld_error rigctld_set_vfo(rigctld_info_t *ret_info, const char *vfo_name);

/**
 * Disconnect from rigctld.
 *
 * \param info Rigctld connection instance
 **/
void rigctld_disconnect(rigctld_info_t *info);

/*
 * Send frequency data to rigctld. 
 *
 * \param info rigctld connection instance
 * \param frequency Frequency in MHz
 * \return RIGCTLD_NO_ERR on success
 **/
rigctld_error rigctld_set_frequency(rigctld_info_t *info, double frequency);

/**
 * Read frequency from rigctld. 
 *
 * \param info rigctld connection instance
 * \param frequency Returned frequency in MHz
 * \return RIGCTLD_NO_ERR on success
 **/
rigctld_error rigctld_read_frequency(rigctld_info_t *info, double *frequency);

#endif
