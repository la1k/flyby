/*
 * Hamlib rotctld rotor controller plugin for gsat
 *
 * The plugin takes two config parameters: host and port
 * Format: "host=my.host.name port=1234"
 * Defaults: host=localhost port=4533
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
int plugin_open_rotor( char * config );
void plugin_close_rotor( void );
void plugin_set_rotor( double azimuth, double elevation );

#define HOST_NAME_MAX 255
#define ROTCTLD_PORT "4533\0\0"

int sd;
int debug = 0;

char * plugin_info( void )
{
  return "Hamlib rotctld";
}

int plugin_open_rotor( char * config )
{
  char hostname[HOST_NAME_MAX + 1];
  char port[6] = ROTCTLD_PORT;
  char *param;
  struct addrinfo hints, *servinfo, *p;

  if (debug && config == NULL) fprintf(stderr, "Config string is null. You might have to open prefs and apply them.\n");

  /* Parse config to get hostname and port */
  memset(hostname, 0, sizeof(hostname));
  if (config != NULL) {
    param = config;
    do {
      if (sscanf(param, "debug=%u", &debug) && debug > 2) fprintf(stderr, "debug=%u\n", debug);
      if (sscanf(param, "host=%255s", hostname) && debug > 2) fprintf(stderr, "host=%s\n", hostname);
      if (sscanf(param, "port=%5s", port) && debug > 2) fprintf(stderr, "port=%s\n", port);
      param = strchr(param, ' ');
    } while (param < config + strlen(config) - 1 && param++ != NULL) ;
  }
  if (!strlen(hostname)) memcpy(hostname, &"localhost\0", 10);

  /* Connect to rotctld server */
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
  /* plugin_set_rotor will wait for confirmation of a command before sending
     the next so we bootstrap this by asking for the current position */
  send(sd, "p\n", 2, 0);

  return 1;
}

void plugin_close_rotor( void )
{
  send(sd, "q\n", 2, 0);
  close(sd);
}

void plugin_set_rotor( double azimuth, double elevation )
{
  char buf[64]; /* Should be more than enough */

  /* If positions are sent too often, rotctld will queue
     them and the antenna will lag behind. Therefore, we wait
     for confirmation from last command before sending the
     next. */
  if (recv(sd, buf, sizeof(buf), MSG_DONTWAIT) < 1)
    return;

  snprintf(buf, 63, "P %f %f\n", azimuth, elevation);
  send(sd, buf, strlen(buf), 0);
}
