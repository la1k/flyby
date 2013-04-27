/*
 * YAESU FT-847 plugin for gsat
 *
 * Copyright (C) 2003 by Ralf Baechle DO1GRB (ralf@linux-mips.org)
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
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>

int plugin_open_rig(char *config);
void plugin_close_rig(void);
void plugin_set_downlink_frequency(double frequency);
void plugin_set_uplink_frequency(double frequency);

static int radiofd;

static void cat_on(int fd)
{
	static const char cmd_buf[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };

	write(fd, cmd_buf, 5);
}

static void cat_off(int fd)
{
	static const char cmd_buf[5] = { 0x00, 0x00, 0x00, 0x00, 0x80 };

	write(fd, cmd_buf, 5);
}

static void cat_sat_on(int fd)
{
	static const char cmd_buf[5] = { 0x00, 0x00, 0x00, 0x00, 0x4e };

	write(fd, cmd_buf, 5);
}

static void cat_sat_off(int fd)
{
	static const char cmd_buf[5] = { 0x00, 0x00, 0x00, 0x00, 0x8e };

	write(fd, cmd_buf, 5);
}

static void cat_send_freq(unsigned int freq, int vfo)
{
	unsigned int bcd0, bcd1;
	char cmd_buf[5];

	/*
	 * CAT only permits setting the frequency with an accuracy of 10Hz,
	 * so we round to the nearest multiple of of 10Hz.
	 */
	freq = (freq + 5) / 10 * 10;

	bcd0 = freq / 100000000; freq -= bcd0 * 100000000;
	bcd1 = freq / 10000000; freq -= bcd1 * 10000000;
	cmd_buf[0] = bcd0 << 4 | bcd1;

	bcd0 = freq / 1000000; freq -= bcd0 * 1000000;
	bcd1 = freq / 100000; freq -= bcd1 * 100000;
	cmd_buf[1] = bcd0 << 4 | bcd1;

	bcd0 = freq / 10000; freq -= bcd0 * 10000;
	bcd1 = freq / 1000; freq -= bcd1 * 1000;
	cmd_buf[2] = bcd0 << 4 | bcd1;

	bcd0 = freq / 100; freq -= bcd0 * 100;
	bcd1 = freq / 10; freq -= bcd1 * 10;
	cmd_buf[3] = bcd0 << 4 | bcd1;

	cmd_buf[4] = vfo;

	write(radiofd, cmd_buf, 5);
}

static void cat_set_freq_sat_rx_vfo(unsigned int freq)
{
	cat_send_freq(freq, 0x11);
}

static void cat_set_freq_sat_tx_vfo(unsigned int freq)
{
	cat_send_freq(freq, 0x21);
}

char *plugin_info(void)
{
	return "YAESU FT847 V0.1";
}

int plugin_open_rig(char *config)
{
	struct termios cat_termios;
	speed_t speed = 0;
	char dumm[64];
	char tty[12];
	char *ptr, *parm;

	tty[0] = '\0';

	if (config) {
		strncpy(dumm, config, 64);

		ptr = dumm;
		parm = ptr;
		while (parm != NULL) {
			parm = strsep(&ptr, ":");
			if (parm == NULL)
				break;
			if (strlen(parm) != 0) {
				switch (*parm) {
				case 'D':	/* tty port */
					strcpy(tty, parm + 1);
					break;
				case 'S':	/* Speed */
					speed = atoi(parm + 1);
					break;
				}
			}
		}
	}

	if (strlen(tty) == 0)
		strcpy(tty, "/dev/ttyS0");

	/*
	 * If no serial speed or a bad rate was specified fall back to 4800
	 * which is the factory default for the FT-847.
	 */
	if (speed != 4800 && speed != 9600 && speed != 57600)
		speed = 4800;

	/*
	 * Open CAT port
	 */
	radiofd = open(tty, O_RDWR | O_NOCTTY);
	if (radiofd == -1) {
		fprintf(stderr, "can't open %s\n", tty);
		return 0;
	}

	/*
	 * Set line speed to 4800 8N2
	 */
	tcgetattr(radiofd, &cat_termios);

	cat_termios.c_cflag = CLOCAL | CS8 | CSTOPB | CREAD;
	//cat_termios.c_cflag = CLOCAL | CS8 | CREAD;

	/*
	 * ICANON  : enable canonical input disable all echo functionality,
	 * and don't send signals to calling program
	 */
	cat_termios.c_lflag |= ICANON;
	cat_termios.c_lflag &= ~(ECHO | ECHOCTL);

	/* ignore bytes with parity errors */
	cat_termios.c_iflag |= IGNPAR;
	cat_termios.c_iflag &= ~IXON;

	/* Raw output.  */
	cat_termios.c_oflag &= ~OPOST;

	cfsetspeed(&cat_termios, speed);
	tcsetattr(radiofd, TCSADRAIN, &cat_termios);
	sleep(1);

	cat_on(radiofd);
	cat_sat_on(radiofd);

	return 1;
}

void plugin_close_rig(void)
{
	cat_sat_off(radiofd);
	cat_off(radiofd);

	close(radiofd);
}

void plugin_set_downlink_frequency(double frequency)
{
	cat_set_freq_sat_rx_vfo((unsigned int)(frequency * 1000.0));
}

void plugin_set_uplink_frequency(double frequency)
{
	cat_set_freq_sat_tx_vfo((unsigned int)(frequency * 1000.0));
}
