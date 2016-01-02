typedef struct {
	int socket;
} rotctl_info_t;

typedef struct {
	int socket;
	char vfo_name[MAX_NUM_CHARS];
} rigctl_info_t;

/**
 * Connect to rotctl. 
 *
 * \param hostname Hostname/IP address
 * \param port Port
 * \param ret_info Returned rotctl connection instance
 **/
void rotctl_connect(const char *hostname, const char *port, rotctl_info_t *ret_info);

/**
 * Send track data to rotctl. 
 *
 * \param info rotctl connection instance
 * \param azimuth Azimuth in degrees
 * \param elevation Elevation in degrees
 **/
void rotctl_track(const rotctl_info_t *info, double azimuth, double elevation);

/**
 * Connect to rigctl. 
 *
 * \param hostname Hostname/IP address
 * \param port Port
 * \param vfo_name VFO name
 * \param ret_info Returned rigctl connection instance
 **/
void rigctl_connect(const char *hostname, const char *port, const char *vfo_name, rigctl_info_t *ret_info);

/*
 * Send frequency data to rigctl. 
 *
 * \param info rigctl connection instance
 * \param frequency Frequency in MHz
 **/
void rigctl_set_frequency(const rigctl_info_t *info, double frequency);

/**
 * Read frequency from rigctl. 
 *
 * \param info rigctl connection instance
 * \return Current frequency in MHz
 **/
double rigctl_read_frequency(const rigctl_info_t *info);
