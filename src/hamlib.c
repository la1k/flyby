#include "hamlib.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <math.h>

void bailout(const char *msg);

int sock_readline(int sockd, char *message, size_t bufsize)
{
	int len=0, pos=0;
	char c='\0';

	if (message!=NULL) {
		message[bufsize-1]='\0';
	}

	do {
		len = recv(sockd, &c, 1, MSG_WAITALL);
		if (len <= 0) {
			break;
		}
		if (message!=NULL) {
			message[pos]=c;
			message[pos+1]='\0';
		}
		pos+=len;
	} while (c!='\n' && pos<bufsize-2);

	return pos;
}

void rotctld_connect(const char *rotctld_host, const char *rotctld_port, rotctld_info_t *ret_info)
{
	struct addrinfo hints, *servinfo, *servinfop;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	int rotctld_socket = 0;
	int retval = getaddrinfo(rotctld_host, rotctld_port, &hints, &servinfo);
	if (retval != 0) {
		bailout("getaddrinfo error");
		exit(-1);
	}

	for(servinfop = servinfo; servinfop != NULL; servinfop = servinfop->ai_next) {
		if ((rotctld_socket = socket(servinfop->ai_family, servinfop->ai_socktype,
			servinfop->ai_protocol)) == -1) {
			continue;
		}
		if (connect(rotctld_socket, servinfop->ai_addr, servinfop->ai_addrlen) == -1) {
			close(rotctld_socket);
			continue;
		}

		break;
	}
	if (servinfop == NULL) {
		char error_message[MAX_NUM_CHARS];
		snprintf(error_message, MAX_NUM_CHARS, "Unable to connect to rotctld on %s:%s", rotctld_host, rotctld_port);
		bailout(error_message);
		exit(-1);
	}
	freeaddrinfo(servinfo);
	/* TrackDataNet() will wait for confirmation of a command before sending
	   the next so we bootstrap this by asking for the current position */
	send(rotctld_socket, "p\n", 2, 0);
	sock_readline(rotctld_socket, NULL, 256);

	ret_info->socket = rotctld_socket;
	ret_info->connected = true;
	strncpy(ret_info->host, rotctld_host, MAX_NUM_CHARS);
	strncpy(ret_info->port, rotctld_port, MAX_NUM_CHARS);
	ret_info->tracking_horizon = 0;

	ret_info->update_time_interval = 0;
	ret_info->prev_cmd_time = 0;
	ret_info->prev_cmd_azimuth = 0;
	ret_info->prev_cmd_elevation = 0;
	ret_info->first_cmd_sent = false;
}

void rotctld_set_tracking_horizon(rotctld_info_t *info, double horizon)
{
	info->tracking_horizon = horizon;
}

void rotctld_set_update_interval(rotctld_info_t *info, int time_interval)
{
	if (time_interval >= 0) {
		info->update_time_interval = time_interval;
	}
}

bool angles_differ(double prev_angle, double angle)
{
	return (int)round(prev_angle) != (int)round(angle);
}

bool rotctld_directions_differ(rotctld_info_t *info, double azimuth, double elevation)
{
	bool azimuth_differs = angles_differ(info->prev_cmd_azimuth, azimuth);
	bool elevation_differs = angles_differ(info->prev_cmd_elevation, elevation);
	return azimuth_differs || elevation_differs;
}

void rotctld_track(rotctld_info_t *info, double azimuth, double elevation)
{
	time_t curr_time = time(NULL);
	bool use_update_interval = (info->update_time_interval > 0);
	bool coordinates_differ = rotctld_directions_differ(info, azimuth, elevation);

	if (!info->first_cmd_sent) {
		coordinates_differ = true;
		info->first_cmd_sent = true;
	}

	//send when coordinates differ or when a update interval has been specified
	if ((coordinates_differ && !use_update_interval) || (use_update_interval && ((curr_time - info->update_time_interval) >= info->prev_cmd_time))) {
		info->prev_cmd_azimuth = azimuth;
		info->prev_cmd_elevation = elevation;
		info->prev_cmd_time = curr_time;

		char message[30];

		/* If positions are sent too often, rotctld will queue
		   them and the antenna will lag behind. Therefore, we wait
		   for confirmation from last command before sending the
		   next. */
		sock_readline(info->socket, message, sizeof(message));

		sprintf(message, "P %.2f %.2f\n", azimuth, elevation);
		int len = strlen(message);
		if (send(info->socket, message, len, 0) != len) {
			bailout("Failed to send to rotctld");
			exit(-1);
		}
	}
}

void rigctld_connect(const char *rigctld_host, const char *rigctld_port, const char *vfo_name, rigctld_info_t *ret_info)
{
	struct addrinfo hints, *servinfo, *servinfop;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	int rigctld_socket = 0;
	int retval = getaddrinfo(rigctld_host, rigctld_port, &hints, &servinfo);
	if (retval != 0) {
		bailout("getaddrinfo error");
		exit(-1);
	}

	for(servinfop = servinfo; servinfop != NULL; servinfop = servinfop->ai_next) {
		if ((rigctld_socket = socket(servinfop->ai_family, servinfop->ai_socktype,
			servinfop->ai_protocol)) == -1) {
			continue;
		}
		if (connect(rigctld_socket, servinfop->ai_addr, servinfop->ai_addrlen) == -1) {
			close(rigctld_socket);
			continue;
		}

		break;
	}
	if (servinfop == NULL) {
		char error_message[MAX_NUM_CHARS];
		snprintf(error_message, MAX_NUM_CHARS, "Unable to connect to rigctld on %s:%s", rigctld_host, rigctld_port);
		bailout(error_message);
		exit(-1);
	}
	freeaddrinfo(servinfo);
	/* FreqDataNet() will wait for confirmation of a command before sending
	   the next so we bootstrap this by asking for the current frequency */
	send(rigctld_socket, "f\n", 2, 0);

	ret_info->socket = rigctld_socket;
	strncpy(ret_info->vfo_name, vfo_name, MAX_NUM_CHARS);
	ret_info->connected = true;
	strncpy(ret_info->host, rigctld_host, MAX_NUM_CHARS);
	strncpy(ret_info->port, rigctld_port, MAX_NUM_CHARS);
}

void rigctld_set_frequency(const rigctld_info_t *info, double frequency)
{
	char message[256];
	int len;

	/* If frequencies is sent too often, rigctld will queue
	   them and the radio will lag behind. Therefore, we wait
	   for confirmation from last command before sending the
	   next. */
	sock_readline(info->socket, message, sizeof(message));

	if (strlen(info->vfo_name) > 0)	{
		sprintf(message, "V %s\n", info->vfo_name);
		len = strlen(message);
		usleep(100); // hack: avoid VFO selection racing
		if (send(info->socket, message, len, 0) != len)	{
			bailout("Failed to send to rigctld");
			exit(-1);
		}
		sock_readline(info->socket, message, sizeof(message));
	}

	sprintf(message, "F %.0f\n", frequency*1000000);
	len = strlen(message);
	if (send(info->socket, message, len, 0) != len)	{
		bailout("Failed to send to rigctld");
		exit(-1);
	}
}

double rigctld_read_frequency(const rigctld_info_t *info)
{
	char message[256];
	int len;
	double freq;

	/* Read pending return message */
	sock_readline(info->socket, message, sizeof(message));

	if (strlen(info->vfo_name) > 0)	{
		sprintf(message, "V %s\n", info->vfo_name);
		len = strlen(message);
		usleep(100); // hack: avoid VFO selection racing
		if (send(info->socket, message, len, 0) != len)	{
			bailout("Failed to send to rigctld");
			exit(-1);
		}
		sock_readline(info->socket, message, sizeof(message));
	}

	sprintf(message, "f\n");
	len = strlen(message);
	if (send(info->socket, message, len, 0) != len)	{
		bailout("Failed to send to rigctld");
		exit(-1);
	}

	sock_readline(info->socket, message, sizeof(message));
	freq=atof(message)/1.0e6;

	sprintf(message, "f\n");
	len = strlen(message);
	if (send(info->socket, message, len, 0) != len)	{
		bailout("Failed to send to rigctld");
		exit(-1);
	}

	return freq;
}

void rigctld_disconnect(rigctld_info_t *info)
{
	if (info->connected) {
		send(info->socket, "q\n", 2, 0);
		close(info->socket);
		info->connected = false;
	}
}

void rotctld_disconnect(rotctld_info_t *info)
{
	if (info->connected) {
		send(info->socket, "q\n", 2, 0);
		close(info->socket);
		info->connected = false;
	}
}
