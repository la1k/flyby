/*
 * Hamlib rigctld radio plugin for gsat
 *
 * The config string consists of the string "rx" or "tx" followed by
 * parameters. The plugin takes three config parameters: host, port and vfo
 * Format: "rx host=my.host.name port=1234 vfo=VFOA"
 * Defaults: host=localhost port=4532
 *
 * Copyright (C) 2009 Norvald H. Ryeng, LA6YKA
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

char * plugin_info( void );
int plugin_open_rig( char * config );
void plugin_close_rig( void );
void plugin_set_downlink_frequency( double frequency );
void plugin_set_uplink_frequency( double frequency );

#define HOST_NAME_MAX 255
#define RIGCTLD_PORT "4532\0\0"
#define VFO_NAME_MAX 10

int rx_sd = -1;
int tx_sd = -1;
char rx_vfo[VFO_NAME_MAX + 1];
char tx_vfo[VFO_NAME_MAX + 1];
int refcount = 0;
int debug = 0;

char * plugin_info( void )
{
  return "Hamlib rigctld";
}

int plugin_open_rig( char * config )
{
  char hostname[HOST_NAME_MAX + 1];
  char port[6] = RIGCTLD_PORT;
  char *vfo;
  char *param;
  int rx;
  int sd = -1;
  struct addrinfo hints, *servinfo, *p;

  /* Config string starts with "rx" or "tx" */
  if (config != NULL && !strncmp(config, "rx", 2)) {
    vfo = rx_vfo;
    rx = 1;
  } else if (config != NULL && !strncmp(config, "tx", 2)) {
    vfo = tx_vfo;
    rx = 0;
  } else {
    if (debug) {
      fprintf(stderr, "rx or tx not specified\n");
      if (config == NULL) fprintf(stderr, "Config string is null. You might have to open prefs and apply them.\n");
    }
    return 0;
  }
  param = config + 2;

  /* Parse config to get hostname, port and vfo */
  memset(hostname, 0, sizeof(hostname));
  memset(vfo, 0, sizeof(*vfo));
  do {
    if (sscanf(param, "debug=%u", &debug) && debug > 2) fprintf(stderr, "debug=%u\n", debug);
    if (sscanf(param, "host=%255s", hostname) && debug > 2) fprintf(stderr, "host=%s\n", hostname);
    if (sscanf(param, "port=%5s", port) && debug > 2) fprintf(stderr, "port=%s\n", port);
    if (sscanf(param, "vfo=%10s", vfo) && debug > 2) fprintf(stderr, "vfo=%s\n", vfo);
    param = strchr(param, ' ');
  } while (param < config + strlen(config) - 1 && param++ != NULL) ;
  if (!strlen(hostname)) memcpy(hostname, &"localhost\0", 10);

  /* Connect to rigctld server */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(hostname, port, &hints, &servinfo))
    return 0;
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
      continue;
    if (connect(sd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sd);
      continue;
    }
    break;
  }
  freeaddrinfo(servinfo);
  if (p == NULL) {
    if (debug) perror("Connect failed");
    return 0;
  }
  if (rx)
    rx_sd = sd;
  else
    tx_sd = sd;

  refcount++;
  if (debug > 3) fprintf(stderr, "refcount=%i\n", refcount);

  /* plugin_set_up/downlink_frequency will wait for confirmation of a
   * command before sending the next so we bootstrap this by asking for
   * the current frequency */
  send(sd, "f\n", 2, 0);

  return 1;
}

void plugin_close_rig( void )
{
  /* If radio is open while applying preferences, they are closed, but
   * checkbox in GUI is still checked, so the close method may be called
   * again, so to avoid a negative refcount: */
  if (refcount) refcount--;

  if (refcount <= 0) {
    if (rx_sd >= 0) {
      send(rx_sd, "q\n", 2, 0);
      close(rx_sd);
      rx_sd = -1;
    }
    if (tx_sd >= 0) {
      send(tx_sd, "q\n", 2, 0);
      close(tx_sd);
      tx_sd = -1;
    }
  }
}

/* Frequency in kHz */
void plugin_set_downlink_frequency( double frequency )
{
  char buf[64]; /* Should be more than enough */

  /* If frequencies are sent too often, rigctld will queue
     them and the radio will lag behind. Therefore, we wait
     for confirmation from last command before sending the
     next. */
  if (recv(rx_sd, buf, sizeof(buf), MSG_DONTWAIT) < 1)
    return;

  if (strlen(rx_vfo)) {
    snprintf(buf, sizeof(buf), "V %s\n", rx_vfo);
    send(rx_sd, buf, strlen(buf), 0);
  }
  snprintf(buf, sizeof(buf), "F %.0f\n", frequency * 1000);
  send(rx_sd, buf, strlen(buf), 0);
}

/* Frequency in kHz */
void plugin_set_uplink_frequency( double frequency )
{
  char buf[64]; /* Should be more than enough */

  /* If frequencies are sent too often, rigctld will queue
     them and the radio will lag behind. Therefore, we wait
     for confirmation from last command before sending the
     next. */
  if (recv(tx_sd, buf, sizeof(buf), MSG_DONTWAIT) < 1)
    return;

  if (strlen(tx_vfo)) {
    snprintf(buf, sizeof(buf), "V %s\n", tx_vfo);
    send(tx_sd, buf, strlen(buf), 0);
  }
  snprintf(buf, sizeof(buf), "F %.0f\n", frequency * 1000);
  send(tx_sd, buf, strlen(buf), 0);
}
