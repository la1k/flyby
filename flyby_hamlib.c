#include "flyby_hamlib.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

void bailout(const char *msg);

int sock_readline(int sockd, char *message, size_t bufsize)
{
	int len=0, pos=0;
	char c='\0';

	if (message!=NULL) {
		message[bufsize-1]='\0';
	}

	do {
		if ((len=recv(sockd, &c, 1, 0)) < 0) {
			return len;
		}
		if (message!=NULL) {
			message[pos]=c;
			message[pos+1]='\0';
		}
		pos+=len;
	} while (c!='\n' && pos<bufsize-2);

	return pos;
}


void rotctld_connect(const char *rotctld_host, const char *rotctld_port, bool once_per_second, double tracking_horizon, rotctld_info_t *ret_info)
{
	struct addrinfo hints, *servinfo, *servinfop;
	int rotctld_socket = 0;
	if (getaddrinfo(rotctld_host, rotctld_port, &hints, &servinfo))	{
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
		bailout("Unable to connect to rotctld");
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
	ret_info->tracking_horizon = tracking_horizon;
	ret_info->once_per_second = once_per_second;
}

void rotctld_track(const rotctld_info_t *info, double azimuth, double elevation)
{
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

void rigctld_connect(const char *rigctld_host, const char *rigctld_port, const char *vfo_name, rigctld_info_t *ret_info)
{
	struct addrinfo hints, *servinfo, *servinfop;
	int rigctld_socket = 0;
	if (getaddrinfo(rigctld_host, rigctld_port, &hints, &servinfo)) {
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
		bailout("Unable to connect to uplink rigctld");
		exit(-1);
	}
	freeaddrinfo(servinfo);
	/* FreqDataNet() will wait for confirmation of a command before sending
	   the next so we bootstrap this by asking for the current frequency */
	send(rigctld_socket, "f\n", 2, 0);

	ret_info->socket = rigctld_socket;
	strncpy(ret_info->vfo_name, vfo_name, MAX_NUM_CHARS);
	ret_info->connected = true;
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

void rigctld_disconnect(rigctld_info_t *info) {
	if (info->connected) {
		send(info->socket, "q\n", 2, 0);
		close(info->socket);
		info->connected = false;
	}
}

void rotctld_disconnect(rotctld_info_t *info) {
	if (info->connected) {
		send(info->socket, "q\n", 2, 0);
		close(info->socket);
		info->connected = false;
	}
}
