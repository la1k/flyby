#include "flyby_defines.h"
#include <stdbool.h>

#define ROTCTLD_DEFAULT_HOST "localhost"
#define ROTCTLD_DEFAULT_PORT "4533\0\0"
#define RIGCTLD_UPLINK_DEFAULT_HOST "localhost"
#define RIGCTLD_UPLINK_DEFAULT_PORT "4532\0\0"
#define RIGCTLD_DOWNLINK_DEFAULT_HOST "localhost"
#define RIGCTLD_DOWNLINK_DEFAULT_PORT "4532\0\0"

typedef struct {
	bool connected;
	int socket;
	char host[MAX_NUM_CHARS];
	char port[MAX_NUM_CHARS];
} rotctld_info_t;

typedef struct {
	bool connected;
	int socket;
	char vfo_name[MAX_NUM_CHARS];
} rigctld_info_t;

/**
 * Connect to rotctld. 
 *
 * \param hostname Hostname/IP address
 * \param port Port
 * \param ret_info Returned rotctld connection instance
 **/
void rotctld_connect(const char *hostname, const char *port, rotctld_info_t *ret_info);

/**
 * Disconnect from rotctld.
 * \param info Rigctld connection instance
 **/
void rotctld_disconnect(rotctld_info_t *info);

/**
 * Send track data to rotctld. 
 *
 * \param info rotctld connection instance
 * \param azimuth Azimuth in degrees
 * \param elevation Elevation in degrees
 **/
void rotctld_track(const rotctld_info_t *info, double azimuth, double elevation);

/**
 * Connect to rigctld. 
 *
 * \param hostname Hostname/IP address
 * \param port Port
 * \param vfo_name VFO name
 * \param ret_info Returned rigctld connection instance
 **/
void rigctld_connect(const char *hostname, const char *port, const char *vfo_name, rigctld_info_t *ret_info);

/**
 * Disconnect from rigctld.
 * \param info Rigctld connection instance
 **/
void rigctld_disconnect(rigctld_info_t *info);

/*
 * Send frequency data to rigctld. 
 *
 * \param info rigctld connection instance
 * \param frequency Frequency in MHz
 **/
void rigctld_set_frequency(const rigctld_info_t *info, double frequency);

/**
 * Read frequency from rigctld. 
 *
 * \param info rigctld connection instance
 * \return Current frequency in MHz
 **/
double rigctld_read_frequency(const rigctld_info_t *info);
