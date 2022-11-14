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
#include <errno.h>

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

/**
 * Make rotctld produce a response which will be ready for reading
 * on the next command to be sent to rotctld.
 *
 * \param socket Socket fid
 **/
void rotctld_bootstrap_response(int socket)
{
	// Send request for position w/ extended response protocol:
	// will get the full response in a single line the next time
	// we read from the socket.
	send(socket, ";p\n", 3, MSG_NOSIGNAL);
}

rotctld_error socket_connect(const char *host, const char *port, int *socket_fid)
{
	struct addrinfo hints, *servinfo, *servinfop;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	int retval = getaddrinfo(host, port, &hints, &servinfo);
	if (retval != 0) {
		return ROTCTLD_GETADDRINFO_ERR;
	}

	for(servinfop = servinfo; servinfop != NULL; servinfop = servinfop->ai_next) {
		if ((*socket_fid = socket(servinfop->ai_family, servinfop->ai_socktype,
			servinfop->ai_protocol)) == -1) {
			continue;
		}
		if (connect(*socket_fid, servinfop->ai_addr, servinfop->ai_addrlen) == -1) {
			close(*socket_fid);
			continue;
		}

		break;
	}
	if (servinfop == NULL) {
		return ROTCTLD_CONNECTION_FAILED;
	}
	freeaddrinfo(servinfo);
}

rotctld_error rotctld_connect(const char *rotctld_host, const char *rotctld_port, rotctld_info_t *ret_info)
{
	strncpy(ret_info->host, rotctld_host, MAX_NUM_CHARS);
	strncpy(ret_info->port, rotctld_port, MAX_NUM_CHARS);
	ret_info->connected = false;
	ret_info->track_buffer.buffer_pos = 0;
	ret_info->last_track_response_received = true;

	rotctld_error retval;
	retval = socket_connect(rotctld_host, rotctld_port, &(ret_info->read_socket));
	if (retval != ROTCTLD_NO_ERR) {
		return retval;
	}
	retval = socket_connect(rotctld_host, rotctld_port, &(ret_info->track_socket));
	if (retval != ROTCTLD_NO_ERR) {
		return retval;
	}

	/* TrackDataNet() will wait for confirmation of a command before sending
	   the next so we bootstrap this by asking for the current position */
	rotctld_bootstrap_response(ret_info->track_socket);

	ret_info->connected = true;
	ret_info->tracking_horizon = 0;

	ret_info->prev_cmd_azimuth = 0;
	ret_info->prev_cmd_elevation = 0;
	ret_info->first_cmd_sent = false;

	return ROTCTLD_NO_ERR;
}

const char *rotctld_error_message(rotctld_error errorcode)
{
	switch (errorcode) {
		case ROTCTLD_NO_ERR:
			return "No error.";
		case ROTCTLD_GETADDRINFO_ERR:
			return "Unable to connect to rotctld: unknown host";
		case ROTCTLD_CONNECTION_FAILED:
			return "Unable to connect to rotctld.";
		case ROTCTLD_SEND_FAILED:
			return "Unable to send to rotctld or rotctld disconnected.";
		case ROTCTLD_RETURNED_STATUS_ERROR:
			return "Message from rotctl contained a non-zero status code";
	}
	return "Unsupported error code.";
}

void rotctld_fail_on_errors(rotctld_error errorcode)
{
	if (errorcode != ROTCTLD_NO_ERR) {
		bailout(rotctld_error_message(errorcode));
		exit(-1);
	}
}

void rotctld_set_tracking_horizon(rotctld_info_t *info, double horizon)
{
	info->tracking_horizon = horizon;
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

rotctld_error try_read_response_nonblocking(int socket, buffer_t *buffer) {
	char *read_buffer = buffer->buffer + buffer->buffer_pos;
	int buffer_len = MAX_NUM_CHARS - buffer->buffer_pos;

	if (buffer_len <= 0) {
		return ROTCTLD_READ_BUFFER_OVERFLOW;
	}

	int read_bytes = recv(socket, read_buffer, buffer_len, MSG_DONTWAIT);

	if ((read_bytes < 0) && (errno != EWOULDBLOCK)) {
		return ROTCTLD_READ_FAILED;
	} else {
		buffer->buffer_pos += read_bytes;
	}

	return ROTCTLD_NO_ERR;
}

bool message_in_buffer_is_complete(buffer_t *buffer) {
	return buffer->buffer[buffer->buffer_pos-1] == '\n';
}

void reset_buffer(buffer_t *buffer) {
	buffer->buffer_pos = 0;
}

rotctld_error rotctld_track(rotctld_info_t *info, double azimuth, double elevation)
{
	bool coordinates_differ = rotctld_directions_differ(info, azimuth, elevation);

	if (!info->first_cmd_sent) {
		coordinates_differ = true;
		info->first_cmd_sent = true;
	}

	/* If positions are sent too often, rotctld will queue
	   them and the antenna will lag behind. Therefore, we wait
	   for confirmation from last command before sending the
	   next. */
	if (!info->last_track_response_received) {
		rotctld_error err = try_read_response_nonblocking(info->track_socket, &info->track_buffer);
		if (err != ROTCTLD_NO_ERR) {
			return err;
		}

		if (message_in_buffer_is_complete(&info->track_buffer)) {
			reset_buffer(&info->track_buffer);
			info->last_track_response_received = true;
		}
	}


	if (coordinates_differ && info->last_track_response_received) {
		info->prev_cmd_azimuth = azimuth;
		info->prev_cmd_elevation = elevation;

		char message[256];

		sprintf(message, "P %.2f %.2f\n", azimuth, elevation);
		int len = strlen(message);
		if (send(info->track_socket, message, len, MSG_NOSIGNAL) != len) {
			info->connected = false;
			return ROTCTLD_SEND_FAILED;
		}

		info->last_track_response_received = false;
	}

	return ROTCTLD_NO_ERR;
}

/**
 * Send request to rotctld for current position.
 *
 * \param socket Socket
 * \return ROTCTLD_NO_ERR on success
 **/
rotctld_error rotctld_send_position_request(int socket)
{
	char message[256];
	sprintf(message, "p\n");
	int len = strlen(message);
	if (send(socket, message, len, MSG_NOSIGNAL) != len) {
		return ROTCTLD_SEND_FAILED;
	}

	return ROTCTLD_NO_ERR;
}

bool msg_is_netrotctl_error(char *message) {
	const char *NETROTCTL_RET = "RPRT ";
	if (strlen(message) < strlen(NETROTCTL_RET) + 1) {
		return false;
	}

	bool is_netrotctl_status = strncmp(message, NETROTCTL_RET, strlen(NETROTCTL_RET)) == 0;
	int netrotctl_status = atoi(message + strlen(NETROTCTL_RET));
	return is_netrotctl_status && (netrotctl_status < 0);
}

rotctld_error rotctld_read_position(rotctld_info_t *info, float *azimuth, float *elevation)
{
	char message[256];

	//send position request
	rotctld_error ret_err = rotctld_send_position_request(info->read_socket);
	if (ret_err != ROTCTLD_NO_ERR) {
		info->connected = false;
		return ret_err;
	}

	//get response
	sock_readline(info->read_socket, message, sizeof(message));
	if (msg_is_netrotctl_error(message)) {
		return ROTCTLD_RETURNED_STATUS_ERROR;
	}

	sscanf(message, "%f\n", azimuth);
	sock_readline(info->read_socket, message, sizeof(message));
	sscanf(message, "%f\n", elevation);

	return ROTCTLD_NO_ERR;
}

/**
 * Convenience function for sending messages to rigctld.
 *
 * \param socket Socket fid
 * \param message Message
 * \return RIGCTLD_NO_ERR on success
 **/
rigctld_error rigctld_send_message(int socket, char *message)
{
	int len;
	len = strlen(message);
	if (send(socket, message, len, MSG_NOSIGNAL) != len) {
		return RIGCTLD_SEND_FAILED;
	}
	return RIGCTLD_NO_ERR;
}

/**
 * Make rigctld produce a response which will be ready
 * for reading on the next command.
 *
 * \param socket Socket
 **/
rigctld_error rigctld_bootstrap_response(int socket)
{
	char message[256];
	sprintf(message, "f\n");
	return rigctld_send_message(socket, message);
}

rigctld_error rigctld_connect(const char *rigctld_host, const char *rigctld_port, rigctld_info_t *ret_info)
{
	strncpy(ret_info->host, rigctld_host, MAX_NUM_CHARS);
	strncpy(ret_info->port, rigctld_port, MAX_NUM_CHARS);

	struct addrinfo hints, *servinfo, *servinfop;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	int rigctld_socket = 0;
	int retval = getaddrinfo(rigctld_host, rigctld_port, &hints, &servinfo);
	if (retval != 0) {
		ret_info->connected = false;
		return RIGCTLD_GETADDRINFO_ERR;
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
		return RIGCTLD_CONNECTION_FAILED;
	}
	freeaddrinfo(servinfo);
	/* FreqDataNet() will wait for confirmation of a command before sending
	   the next so we bootstrap this by asking for the current frequency */
	rigctld_error ret_err = rigctld_bootstrap_response(rigctld_socket);
	if (ret_err != RIGCTLD_NO_ERR) {
		ret_info->connected = false;
		return ret_err;
	}

	ret_info->socket = rigctld_socket;
	ret_info->connected = true;

	return RIGCTLD_NO_ERR;
}

/**
 * Set VFO in rigctld daemon.
 *
 * \param socket rigctld socket
 * \param vfo_name VFO name
 * \return RIGCTLD_NO_ERR on success
 **/
rigctld_error rigctld_send_vfo_command(int socket, const char *vfo_name)
{
	if (strlen(vfo_name) > 0)	{
		char message[256];
		sprintf(message, "V %s\n", vfo_name);
		usleep(100); // hack: avoid VFO selection racing

		rigctld_error ret_err = rigctld_send_message(socket, message);
		if (ret_err != RIGCTLD_NO_ERR) {
			return ret_err;
		}
		sock_readline(socket, message, sizeof(message));
	}
	return RIGCTLD_NO_ERR;
}

rigctld_error rigctld_set_frequency(rigctld_info_t *info, double frequency)
{
	char message[256];

	/* If frequencies is sent too often, rigctld will queue
	   them and the radio will lag behind. Therefore, we wait
	   for confirmation from last command before sending the
	   next. */
	sock_readline(info->socket, message, sizeof(message));

	rigctld_error ret_err = rigctld_send_vfo_command(info->socket, info->vfo_name);
	if (ret_err != RIGCTLD_NO_ERR) {
		info->connected = false;
		return ret_err;
	}

	sprintf(message, "F %.0f\n", frequency*1000000);
	return rigctld_send_message(info->socket, message);
}

void rigctld_fail_on_errors(rigctld_error errorcode)
{
	if (errorcode != RIGCTLD_NO_ERR) {
		bailout(rigctld_error_message(errorcode));
		exit(-1);
	}
}

const char *rigctld_error_message(rigctld_error errorcode)
{
	switch (errorcode) {
		case RIGCTLD_NO_ERR:
			return "No error.";
		case RIGCTLD_GETADDRINFO_ERR:
			return "Unable to connect to rigctld: unknown host";
		case RIGCTLD_CONNECTION_FAILED:
			return "Unable to connect to rigctld.";
		case RIGCTLD_SEND_FAILED:
			return "Unable to send to rigctld or rigctld disconnected.";
	}
	return "Unsupported error code.";
}

rigctld_error rigctld_read_frequency(rigctld_info_t *info, double *ret_frequency)
{
	char message[256];

	//read pending return message
	sock_readline(info->socket, message, sizeof(message));

	rigctld_error ret_err = rigctld_send_vfo_command(info->socket, info->vfo_name);
	if (ret_err != RIGCTLD_NO_ERR) {
		info->connected = false;
		return ret_err;
	}

	sprintf(message, "f\n");
	ret_err = rigctld_send_message(info->socket, message);
	if (ret_err != RIGCTLD_NO_ERR) {
		info->connected = false;
		return ret_err;
	}

	sock_readline(info->socket, message, sizeof(message));
	*ret_frequency = atof(message)/1.0e6;

	//prepare new pending reply
	ret_err = rigctld_bootstrap_response(info->socket);
	if (ret_err != RIGCTLD_NO_ERR) {
		info->connected = false;
		return ret_err;
	}

	return RIGCTLD_NO_ERR;
}

rigctld_error rigctld_set_vfo(rigctld_info_t *ret_info, const char *vfo_name)
{
	strncpy(ret_info->vfo_name, vfo_name, MAX_NUM_CHARS);
	return RIGCTLD_NO_ERR;
}

void rigctld_disconnect(rigctld_info_t *info)
{
	if (info->connected) {
		send(info->socket, "q\n", 2, MSG_NOSIGNAL);
		close(info->socket);
		info->connected = false;
	}
}

void rotctld_disconnect(rotctld_info_t *info)
{
	if (info->connected) {
		send(info->read_socket, "q\n", 2, MSG_NOSIGNAL);
		send(info->track_socket, "q\n", 2, MSG_NOSIGNAL);
		close(info->read_socket);
		close(info->track_socket);
		info->connected = false;
	}
}
