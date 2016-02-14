/***************************************************************************\
*          PREDICT: A satellite tracking/orbital prediction program         *
*               Copyright John A. Magliacane, KD2BD 1991-2002               *
*                      Project started: 26-May-1991                         *
*                        Last update: 14-Oct-2002                           *
*****************************************************************************
*         Network sockets added by Ivan Galysh, KD4HBO  10-Jan-2000         *
*               The network port is 1210.  Protocol is UDP.                 *
*                    The pthreads library is required.                      *
*         The socket server is spawned to operate in the background.        *
*****************************************************************************
*    Code to send live AZ/EL tracking data to the serial port for antenna   *
*    tracking was contributed by Vittorio Benvenuti, I3VFJ : 13-Jul-2000    *
*    (Removed in flyby fork -- Knut Magnus Kvamtrø/LA3DPA)                  *
*****************************************************************************
*   SGP4/SDP4 code was derived from Pascal routines originally written by   *
*       Dr. TS Kelso, and converted to C by Neoklis Kyriazis, 5B4AZ         *
*****************************************************************************
*    Extended 250 satellite display capability and other cosmetic mods      *
*     you need to add '-lmenu' to the build file to link in the menu        *
*     handling code.                    Should work with CygWIN too...      *
*           John Heaton, G1YYH <g1yyh@amsat.org> :  1-Oct-2005              *
*****************************************************************************
*                                                                           *
* This program is free software; you can redistribute it and/or modify it   *
* under the terms of the GNU General Public License as published by the     *
* Free Software Foundation; either version 2 of the License or any later    *
* version.                                                                  *
*                                                                           *
* This program is distributed in the hope that it will be useful,           *
* but WITHOUT ANY WARRANTY; without even the implied warranty of            *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
* General Public License for more details.                                  *
*                                                                           *
\***************************************************************************/

#include "config.h"
#include <math.h>
#include <time.h>
#include <sys/timeb.h>
#include <curses.h>
#include <menu.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <predict/predict.h>
#include "filtered_menu.h"
#include <form.h>
#include "ui.h"
#include "qth_config.h"

#define EARTH_RADIUS_KM		6.378137E3		/* WGS 84 Earth radius km */
#define HALF_DELAY_TIME	5
#define	KM_TO_MI		0.621371		/* km to miles */

double reduce(value,rangeMin,rangeMax)
double value, rangeMin, rangeMax;
{
	double range, rangeFrac, fullRanges, retval;

	range     = rangeMax - rangeMin;
	rangeFrac = (rangeMax - value) / range;

	modf(rangeFrac,&fullRanges);

	retval = value + fullRanges * range;

	if (retval > rangeMax)
		retval -= range;

	return(retval);
}

void getMaidenHead(mLtd,mLng,mStr)
double mLtd, mLng;
char   *mStr;
{
	int i, j, k, l, m, n;

	mLng = reduce(180.0 - mLng, 0.0, 360.0);
	mLtd = reduce(90.0 + mLtd, 0.0, 360.0);

	i = (int) (mLng / 20.0);
	j = (int) (mLtd / 10.0);

	mLng -= (double) i * 20.0;
	mLtd -= (double) j * 10.0;

	k = (int) (mLng / 2.0);
	l = (int) (mLtd / 1.0);

	mLng -= (double) k * 2.0;
	mLtd -= (double) l * 1.0;

	m = (int) (mLng * 12.0);
	n = (int) (mLtd * 24.0);

	sprintf(mStr,"%c%c%d%d%c%c",
		'A' + (char) i,
		'A' + (char) j,
		k, l,
		tolower('A' + (char) m),
		tolower('A' + (char) n));

	return;
}

void bailout(string)
char *string;
{
	beep();
	curs_set(1);
	bkgdset(COLOR_PAIR(1));
	clear();
	refresh();
	endwin();
	fprintf(stderr,"*** flyby: %s!\n",string);
}

void Banner()
{
	curs_set(0);
	bkgdset(COLOR_PAIR(3));
	clear();
	refresh();

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(2,18,"                                                     ");
	mvprintw(3,18,"                --== flyby v%s ==--                 ",FLYBY_VERSION);
	mvprintw(4,18,"                                                     ");
	mvprintw(5,18,"   based on PREDICT, by John A. Magliacane (KD2BD)   ");
	mvprintw(6,18,"         with mods by John Heaton (G1YYH)            ");
	mvprintw(7,18,"                                                     ");
}

void AnyKey()
{
	mvprintw(LINES - 2,57,"[Any Key To Continue]");
	refresh();
	getch();
}

char	temp[80];
char *Abbreviate(string,n)
char *string;
int n;
{
	/* This function returns an abbreviated substring of the original,
	   including a '~' character if a non-blank character is chopped
	   out of the generated substring.  n is the length of the desired
	   substring.  It is used for abbreviating satellite names. */

	strncpy(temp,(const char *)string,79);

	if (temp[n]!=0 && temp[n]!=32) {
		temp[n-2]='~';
		temp[n-1]=temp[strlen(temp)-1];
	}

	temp[n]=0;

	return temp;
}

char KepCheck(line1,line2)
char *line1, *line2;
{
	int x;
	unsigned sum1, sum2;

	unsigned char val[256];

	/* Set up translation table for computing TLE checksums */

	for (x=0; x<=255; val[x]=0, x++);
	for (x='0'; x<='9'; val[x]=x-'0', x++);

	val['-']=1;

	/* Compute checksum for each line */

	for (x=0, sum1=0, sum2=0; x<=67; sum1+=val[(int)line1[x]], sum2+=val[(int)line2[x]], x++);

	/* Perform a "torture test" on the data */

	x=(val[(int)line1[68]]^(sum1%10)) | (val[(int)line2[68]]^(sum2%10)) |
	  (line1[0]^'1')  | (line1[1]^' ')  | (line1[7]^'U')  |
	  (line1[8]^' ')  | (line1[17]^' ') | (line1[23]^'.') |
	  (line1[32]^' ') | (line1[34]^'.') | (line1[43]^' ') |
	  (line1[52]^' ') | (line1[61]^' ') | (line1[62]^'0') |
	  (line1[63]^' ') | (line2[0]^'2')  | (line2[1]^' ')  |
	  (line2[7]^' ')  | (line2[11]^'.') | (line2[16]^' ') |
	  (line2[20]^'.') | (line2[25]^' ') | (line2[33]^' ') |
	  (line2[37]^'.') | (line2[42]^' ') | (line2[46]^'.') |
	  (line2[51]^' ') | (line2[54]^'.') | (line1[2]^line2[2]) |
	  (line1[3]^line2[3]) | (line1[4]^line2[4]) |
	  (line1[5]^line2[5]) | (line1[6]^line2[6]) |
	  (isdigit(line1[68]) ? 0 : 1) | (isdigit(line2[68]) ? 0 : 1) |
	  (isdigit(line1[18]) ? 0 : 1) | (isdigit(line1[19]) ? 0 : 1) |
	  (isdigit(line2[31]) ? 0 : 1) | (isdigit(line2[32]) ? 0 : 1);

	return (x ? 0 : 1);
}

double ReadBearing(input)
char *input;
{
	/* This function takes numeric input in the form of a character
	string, and returns an equivalent bearing in degrees as a
	decimal number (double).  The input may either be expressed
	in decimal format (74.2467) or degree, minute, second
	format (74 14 48).  This function also safely handles
	extra spaces found either leading, trailing, or
	embedded within the numbers expressed in the
	input string.  Decimal seconds are permitted. */

	char string[20];
	double bearing=0.0, seconds;
	int a, b, length, degrees, minutes;

	/* Copy "input" to "string", and ignore any extra
	spaces that might be present in the process. */

	string[0]=0;
	length=strlen(input);

	for (a=0, b=0; a<length && a<18; a++) {
		if ((input[a]!=32 && input[a]!='\n') || (input[a]==32 && input[a+1]!=32 && b!=0)) {
			string[b]=input[a];
			b++;
		}
	}

	string[b]=0;

	/* Count number of spaces in the clean string. */

	length=strlen(string);

	for (a=0, b=0; a<length; a++)
		if (string[a]==32)
			b++;

	if (b==0) /* Decimal Format (74.2467) */
		sscanf(string,"%lf",&bearing);

	if (b==2) {
		/* Degree, Minute, Second Format (74 14 48) */
		sscanf(string,"%d %d %lf",&degrees, &minutes, &seconds);

		if (degrees<0.0) {
			minutes=-minutes;
			seconds=-seconds;
		}

		bearing=(double)degrees+((double)minutes/60)+(seconds/3600);
	}

	/* Bizarre results return a 0.0 */

	if (bearing>360.0 || bearing<-360.0)
		bearing=0.0;

	return bearing;
}

char CopyFile(source, destination)
char *source;
char *destination;
{
	/* This function copies file "source" to file "destination"
	   in 64k chunks.  The permissions on the destination file
	   are set to rw-r--r--  (0644).  A 0 is returned if no
	   errors are encountered.  A 1 indicates a problem writing
	   to the destination file.  A 2 indicates a problem reading
	   the source file.  */

	int x, sd, dd;
	char error=0, buffer[65536];

	sd=open(source,O_RDONLY);

	if (sd!=-1)
	{
		dd=open(destination,O_WRONLY | O_CREAT| O_TRUNC, 0644);

		if (dd!=-1)
		{
			x=read(sd,&buffer,65536);

			while (x)
			{
				write(dd,&buffer,x);
				x=read(sd,&buffer,65536);
			}

			close(dd);
		}
		else
			error=1;

		close(sd);
	}
	else
		error+=2;

	return error;
}

void AutoUpdate(const char *string, struct tle_db *tle_db, predict_orbital_elements_t **orbits)
{
	bool interactive_mode = (string[0] == '\0');
	char filename[MAX_NUM_CHARS] = {0};

	if (interactive_mode) {
		//get filename from user
		curs_set(1);
		bkgdset(COLOR_PAIR(3));
		refresh();
		clear();
		echo();

		attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
		mvprintw(0,0,"                                                                                ");
		mvprintw(1,0,"  flyby Keplerian Database Auto Update                                          ");
		mvprintw(2,0,"                                                                                ");

		attrset(COLOR_PAIR(4)|A_BOLD);
		bkgdset(COLOR_PAIR(2));
		mvprintw(19,18,"Enter NASA Two-Line Element Source File Name");
		mvprintw(13,18,"-=> ");
		refresh();
		wgetnstr(stdscr,filename,49);
		clear();
		curs_set(0);
	} else {
		strncpy(filename, string, MAX_NUM_CHARS);
	}

	//update TLE database with file
	bool was_updated[MAX_NUM_SATS] = {0};
	bool in_new_file[MAX_NUM_SATS] = {0};
	tle_db_update(filename, tle_db, was_updated, in_new_file);

	if (interactive_mode) {
		move(12, 0);
	}

	int num_updated = 0;
	for (int i=0; i < tle_db->num_tles; i++) {
		if (was_updated[i]) {
			//print updated entries
			if (interactive_mode) {
				printw("Updated %s (%ld)", tle_db->tles[i].name, tle_db->tles[i].satellite_number);
			} else {
				printf("Updated %s (%ld)", tle_db->tles[i].name, tle_db->tles[i].satellite_number);
			}
			if (in_new_file[i]) {
				if (interactive_mode) {
					printw(" (*)");
				} else {
					printf(" (*)");
				}
			}
			if (interactive_mode) {
				printw("\n");
			} else {
				printf("\n");
			}
			num_updated++;

			//update orbital elements
			if (orbits != NULL) {
				predict_destroy_orbital_elements(orbits[i]);

				char *tle[] = {tle_db->tles[i].line1, tle_db->tles[i].line2};
				orbits[i] = predict_parse_tle(tle);
			}
		}
	}

	//print information about new files
	for (int i=0; i < tle_db->num_tles; i++) {
		if (in_new_file[i]) {
			if (interactive_mode) {
				printw("\nSatellites marked with (*) were ");
			} else {
				printf("\nSatellites marked with (*) were ");
			}
			if (!tle_db->read_from_xdg) {
				if (interactive_mode) {
					printw("located in non-writeable locations and are ignored.\n");
				} else {
					printf("located in non-writeable locations and are ignored.\n");
				}
			} else {
				if (interactive_mode) {
					printw("put in a new file (%s).\n", tle_db->tles[i].filename);
				} else {
					printf("put in a new file (%s).\n", tle_db->tles[i].filename);
				}
			}
			break;
		}
	}

	if (num_updated == 0) {
		if (interactive_mode) {
			printw("No TLE updates/file not found.\n");
		} else {
			printf("No TLE updates/file not found.\n");
		}
	}

	if (interactive_mode) {
		refresh();
		AnyKey();
	}
}

int Select(struct tle_db *tle_db, predict_orbital_elements_t **orbital_elements_array)
{
	int num_orbits = tle_db->num_tles;
	ITEM **my_items;
	int c;
	MENU *my_menu;
	WINDOW *my_menu_win;
	int n_choices, i, j = -1;

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	clear();

	mvprintw(0,0,"                                                                                ");
	mvprintw(1,0,"  flyby Satellite Selector                                                      ");
	mvprintw(2,0,"                                                                                ");

	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw( 5,46,"Use cursor keys to move up/down");
	mvprintw( 6,46,"the list and then select with ");
	mvprintw( 7,46,"the 'Enter' key.");
	mvprintw( 9,46,"Press 'q' to return to menu.");

	if (num_orbits >= MAX_NUM_SATS)
		mvprintw(LINES-3,46,"Truncated to %d satellites",MAX_NUM_SATS);
	else
		mvprintw(LINES-3,46,"%d satellites",num_orbits);

	/* Create the window to be associated with the menu */
	my_menu_win = newwin(LINES-5, 40, 4, 4);
	keypad(my_menu_win, TRUE);
	wattrset(my_menu_win, COLOR_PAIR(4));

	/* Print a border around the main window and print a title */
#if	!defined (__CYGWIN32__)
	box(my_menu_win, 0, 0);
#endif

	/* Create items */
	my_items = (ITEM **)calloc(num_orbits + 1, sizeof(ITEM *));

	int item_ind = 0;
	for(i = 0; i < num_orbits; ++i) {
		if (tle_db_entry_enabled(tle_db, i)) {
			my_items[item_ind] = new_item(tle_db->tles[i].name, orbital_elements_array[i]->designator);
			item_ind++;
		}
	}
	n_choices = item_ind;
	my_items[item_ind] = NULL; //terminate the menu list

	if (n_choices > 0) {
		/* Create menu */
		my_menu = new_menu((ITEM **)my_items);

		set_menu_back(my_menu,COLOR_PAIR(1));
		set_menu_fore(my_menu,COLOR_PAIR(5)|A_BOLD);

		/* Set main window and sub window */
		set_menu_win(my_menu, my_menu_win);
		set_menu_sub(my_menu, derwin(my_menu_win, LINES-7, 38, 2, 1));
		set_menu_format(my_menu, LINES-9, 1);

		/* Set menu mark to the string " * " */
		set_menu_mark(my_menu, " * ");

		/* Post the menu */
		post_menu(my_menu);

		refresh();
		wrefresh(my_menu_win);

		bool should_run = true;
		bool should_exit = false;
		while (should_run) {
			c = wgetch(my_menu_win);
			switch(c) {
				case 'q':
					should_run = false;
					should_exit = true;
					break;
				case KEY_DOWN:
					menu_driver(my_menu, REQ_DOWN_ITEM);
					break;
				case KEY_UP:
					menu_driver(my_menu, REQ_UP_ITEM);
					break;
				case KEY_NPAGE:
					menu_driver(my_menu, REQ_SCR_DPAGE);
					break;
				case KEY_PPAGE:
					menu_driver(my_menu, REQ_SCR_UPAGE);
					break;
				case 10: /* Enter */
					pos_menu_cursor(my_menu);
					should_run = false;
					break;
			}
			wrefresh(my_menu_win);
		}

		if (!should_exit) {
			for (i=0, j=0; i<num_orbits; i++)
				if (strcmp(item_name(current_item(my_menu)),tle_db->tles[i].name)==0)
					j = i;
		}

		/* Unpost and free all the memory taken up */
		unpost_menu(my_menu);
		free_menu(my_menu);
	} else {
		refresh();
		wrefresh(my_menu_win);
		c = wgetch(my_menu_win);
		j = -1;
	}

	for(i = 0; i < n_choices; ++i) {
		free_item(my_items[i]);
	}
	free(my_items);

	return j;
}

long DayNum(m,d,y)
int  m, d, y;
{
	/* This function calculates the day number from m/d/y. */

	long dn;
	double mm, yy;

	if (m<3) {
		y--;
		m+=12;
	}

	if (y<57)
		y+=100;

	yy=(double)y;
	mm=(double)m;
	dn=(long)(floor(365.25*(yy-80.0))-floor(19.0+yy/100.0)+floor(4.75+yy/400.0)-16.0);
	dn+=d+30*m+(long)floor(0.6*mm-0.3);
	return dn;
}

double GetStartTime(const char* info_str)
{
	/* This function prompts the user for the time and date
	   the user wishes to begin prediction calculations,
	   and returns the corresponding fractional day number.
	   31Dec79 00:00:00 returns 0.  Default is NOW. */

	int	x, hr, min, sec ,mm=0, dd=0, yy;
	char	good, mon[MAX_NUM_CHARS], line[MAX_NUM_CHARS], string[MAX_NUM_CHARS], bozo_count=0,
		*month[12]= {"Jan", "Feb", "Mar", "Apr", "May",
		"Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	do {
		bkgdset(COLOR_PAIR(2)|A_BOLD);
		clear();

		printw("\n\n\n\t     Starting Date and Time for Predictions of ");

		printw("%-15s\n\n", info_str);

		bozo_count++;

		time_t epoch = time(NULL);
		strftime(string, MAX_NUM_CHARS, "%a %d%b%y %H:%M%S", gmtime(&epoch));

		for (x=4; x<24; x++)
			string[x-4]=string[x];

		attrset(COLOR_PAIR(4)|A_BOLD);
		printw("\t\t    Format: %s -or- ",string);
		string[7]=0;
		printw("%s",string);

		attrset(COLOR_PAIR(2)|A_BOLD);
		mvprintw(21,30,"Default is `NOW'");
		attrset(COLOR_PAIR(3)|A_BOLD);
		mvprintw(13,1,"Enter Start Date & Time >> ");
		curs_set(1);
		refresh();
		echo();
		string[0]=0;
		wgetnstr(stdscr,string,29);
		curs_set(0);
		noecho();

		if (strlen(string)!=0)
			strcpy(line,string);
		else
			/* Select `NOW' */
			return(predict_to_julian(time(NULL)));

		if (strlen(line)==7) {
			line[7]=' ';
			line[8]='0';
			line[9]='0';
			line[10]=':';
			line[11]='0';
			line[12]='0';
			line[13]=':';
			line[14]='0';
			line[15]='0';
			line[16]=0;
		}

		/* Check Day */
		good = (isdigit(line[0]) && isdigit(line[1])) ? 1 : 0;

		/* Month */
		good = (good && isalpha(line[2]) && isalpha(line[3]) && isalpha(line[4])) ? 1 : 0;

		/* Year */
		good = (good && isdigit(line[5]) && isdigit(line[6]) && (line[7]==' ')) ? 1 : 0;

		/* Hour */
		good = (good && isdigit(line[8]) && isdigit(line[9]) && (line[10]==':')) ? 1 : 0;

		/* Minute */
		good = (good && isdigit(line[11]) && isdigit(line[12]) && (line[13]==':')) ? 1 : 0;

		/* Seconds */
		good = (good && isdigit(line[14]) && isdigit(line[15])) ? 1 : 0;

		if (good) {
			/* Decode Day */
			dd=10*(line[0]-'0')+line[1]-'0';

			/* Decode Month Number */
			line[2]=toupper(line[2]);
			line[3]=tolower(line[3]);
			line[4]=tolower(line[4]);

			mon[0]=line[2];
			mon[1]=line[3];
			mon[2]=line[4];
			mon[3]=0;

			for (mm=0; (mm<12 && strcmp(mon,month[mm])!=0); mm++);

			mm++;

			good=(mm>12) ? 0 : 1;
		}

		if (good==0)
			beep();

	} while (good==0 && bozo_count<6);

	if (good==0) {
		/* If the user can't enter the starting date/time
		   correctly after several attempts, then the user
		   is a "bozo" and obviously can't follow directions. */

		bailout("Too Many Errors");
		exit(-1);
	}

	/* Decode Year */
	yy=10*(line[5]-'0')+line[6]-'0';

	/* Decode Time */
	for (x=8; x<16; x++)
		string[x-8]=line[x];

	string[8]=0;

	hr=10*(line[8]-'0')+line[9]-'0';
	min=10*(line[11]-'0')+line[12]-'0';
	sec=10*(line[14]-'0')+line[15]-'0';

	return ((double)DayNum(mm,dd,yy)+((hr/24.0)+(min/1440.0)+(sec/86400.0)));
}

int Print(const char *title, const char *string, char mode)
{
	char type[20], head2[81];
	int key, ans=0;
	static char buffer[5000], lines, quit;
	static FILE *fd;

	/* Pass a NULL string to initialize the buffer, counter, and flags */

	if (string[0]==0) {
		lines=0;
		quit=0;
		buffer[0]=0;
		fd=NULL;
	} else {
		if (mode=='p')
			strcpy(type,"Satellite Passes");

		if (mode=='v')
			strcpy(type,"Visual");

		if (mode=='s')
			strcpy(type,"Solar Illumination");

		if (mode=='m') {
			strcpy(type,"Moon");
		}

		if (mode=='o') {
			strcpy(type,"Sun");
		}

		if (mode=='m' || mode=='o') {
			sprintf(head2,"           Date     Time    El   Az   RA     Dec    GHA     Vel   Range         ");
		}

		if (mode!='m' && mode!='o') {
			if (mode=='s')
				sprintf(head2,"           Date     Mins/Day    Sun           Date     Mins/Day    Sun          ");
			else {
				sprintf(head2,"           Date     Time    El   Az  Phase  %s   %s    Range   Orbit        ","LatN","LonE");
			}
		}

		strcat(buffer,string);
		lines++;
//JHJHJH
		if (lines==(LINES-8)) {
			attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
			clear();
			mvprintw(0,0,"                                                                                ");
			mvprintw(1,0,"  flyby Calendar :                                                              ");
			mvprintw(1,21,"%-24s", type);
			mvprintw(2,0,"                                                                                ");
			mvprintw(1,60, "%s", title);
			attrset(COLOR_PAIR(2)|A_REVERSE|A_BOLD);
			mvprintw(3,0,head2);

			attrset(COLOR_PAIR(2)|A_BOLD);
	                mvprintw(4,0,"\n");

			addstr(buffer);
			attrset(COLOR_PAIR(4)|A_BOLD);

			if (buffer[0]=='\n')
                              printw("\n");

			if (fd==NULL)
				mvprintw(LINES-2,63,"        ");
			else
				mvprintw(LINES-2,63,"Log = ON");

			mvprintw(LINES-2,6,"More? [y/n] >> ");
			curs_set(1);
			refresh();

			while (ans==0) {
				key=toupper(getch());

				if (key=='Y' || key=='\n' || key==' ') {
					key='Y';
					ans=1;
					quit=0;
				}

				if (key=='N' || key=='Q' || key==27) {
					key='N';
					ans=1;
					quit=1;
				}

				buffer[0]=0;
			}

			lines=0;
			curs_set(0);
		}
	}
	return (quit);
}

int PrintVisible(const char *title, const char *string)
{
	static char buffer[10000];
	char line[80], plus, asterisk, visible;
	int x, y, quit=0;

	if (string[0]==0)
		buffer[0]=0;
	else {
		strcat(buffer,string);

		if (string[0]=='\n') {
			plus=0;
			visible=0;
			asterisk=0;

			for (x=0; buffer[x]!=0 && visible==0; x++) {
				if (buffer[x]=='+')
					plus++;

				if (buffer[x]=='*')
					asterisk++;

				/* At least 3 +'s or at least 2 +'s
				   combined with at least 2 *'s is
				   worth displaying as a visible pass. */

				if ((plus>3) || (plus>2 && asterisk>2))
					visible=1;
			}

			if (visible) {
				/* Dump buffer to Print() line by line */

				for (x=0, y=0; buffer[x]!=0 && quit==0; x++) {
					line[y]=buffer[x];

					if (line[y]=='\n') {
						line[y+1]=0;
						quit=Print(title,line,'v');
						line[0]=0;
						y=0;
					} else
						y++;
				}
			}

			buffer[0]=0;
		}
	}

	return quit;
}

void Predict(const char *name, predict_orbital_elements_t *orbital_elements, predict_observer_t *qth, char mode)
{
	bool should_quit = false;
	bool should_break = false;
	char data_string[MAX_NUM_CHARS];
	char time_string[MAX_NUM_CHARS];

	predict_julian_date_t curr_time = GetStartTime(name);

	struct predict_orbit orbit;
	predict_orbit(orbital_elements, &orbit, curr_time);
	clear();

	char title[MAX_NUM_CHARS] = {0};
	sprintf(title, "%s (%d)", name, orbital_elements->satellite_number);

	if (predict_aos_happens(orbital_elements, qth->latitude) && !predict_is_geostationary(orbital_elements) && !(orbit.decayed)) {
		do {
			predict_julian_date_t next_aos = predict_next_aos(qth, orbital_elements, curr_time);
			predict_julian_date_t next_los = predict_next_los(qth, orbital_elements, next_aos);
			curr_time = next_aos;

			struct predict_observation obs;
			predict_orbit(orbital_elements, &orbit, curr_time);
			predict_observe_orbit(qth, &orbit, &obs);
			bool has_printed_last_entry = false;
			int last_printed_elevation = 1;
			do {
				//get formatted time
				time_t epoch = predict_from_julian(curr_time);
				strftime(time_string, MAX_NUM_CHARS, "%a %d%b%y %H:%M:%S", gmtime(&epoch));

				//modulo 256 phase
				int ma256 = (int)rint(256.0*(orbit.phase/(2*M_PI)));

				//satellite visibility status
				char visibility;
				if (obs.visible) {
					visibility = '+';
				} else if (!(orbit.eclipsed)) {
					visibility = '*';
				} else {
					visibility = ' ';
				}

				//format line of data
				sprintf(data_string,"      %s%4d %4d  %4d  %4d   %4d   %6ld  %6ld %c\n", time_string, (int)(obs.elevation*180.0/M_PI), (int)(obs.azimuth*180.0/M_PI), ma256, (int)(orbit.latitude*180.0/M_PI), (int)(orbit.longitude*180.0/M_PI), (long)(obs.range), orbit.revolutions, visibility);
				last_printed_elevation = obs.elevation*180.0/M_PI;

				//print data to screen
				if (mode=='p') {
					should_quit=Print(title,data_string,'p');
				}

				//print only visible passes to screen
				if (mode=='v') {
					nodelay(stdscr,TRUE);
					attrset(COLOR_PAIR(4));
					mvprintw(LINES - 2,6,"                 Calculating... Press [ESC] To Quit");

					/* Allow a way out if this
					   should continue forever... */

					if (getch()==27) {
						//will continue through the pass, and then break the outer whileloop
						should_break = true;
					}

					nodelay(stdscr,FALSE);

					should_quit=PrintVisible(title,data_string);
				}

				//calculate results for next timestep
				curr_time += cos((obs.elevation*180/M_PI-1.0)*M_PI/180.0)*sqrt(orbit.altitude)/25000.0; //predict's magic time increment formula
				predict_orbit(orbital_elements, &orbit, curr_time);
				predict_observe_orbit(qth, &orbit, &obs);

				//make sure that the last printed line is at elevation 0 (since that looks nicer)
				if ((last_printed_elevation != 0) && (obs.elevation < 0) && !has_printed_last_entry) {
					has_printed_last_entry = true;
					curr_time = next_los;

					predict_orbit(orbital_elements, &orbit, curr_time);
					predict_observe_orbit(qth, &orbit, &obs);
				}
			} while (((obs.elevation >= 0) || (curr_time <= next_los)) && !should_quit);

			if (mode=='p') {
				should_quit=Print(title,"\n",'p');
			}

			if (mode=='v') {
				should_quit=PrintVisible(title,"\n");
			}
		} while (!should_quit && !should_break && !(orbit.decayed));
	} else {
		//display warning that passes are impossible
		bkgdset(COLOR_PAIR(5)|A_BOLD);
		clear();

		if (!predict_aos_happens(orbital_elements, qth->latitude) || orbit.decayed) {
			mvprintw(12,5,"*** Passes for %s cannot occur for your ground station! ***\n",name);
		}

		if (predict_is_geostationary(orbital_elements)) {
			mvprintw(12,3,"*** Orbital predictions cannot be made for a geostationary satellite! ***\n");
		}

		beep();
		bkgdset(COLOR_PAIR(7)|A_BOLD);
		AnyKey();
		refresh();
	}
}

void celestial_predict(enum celestial_object object, predict_observer_t *qth, predict_julian_date_t time, struct predict_observation *obs) {
	switch (object) {
		case PREDICT_SUN:
			predict_observe_sun(qth, time, obs);
		break;
		case PREDICT_MOON:
			predict_observe_moon(qth, time, obs);
		break;
	}
}

void PredictSunMoon(enum celestial_object object, predict_observer_t *qth)
{
	char print_mode;
	char name_str[MAX_NUM_CHARS];
	switch (object){
		case PREDICT_SUN:
			print_mode='o';
			strcpy(name_str, "the Sun");
		break;
		case PREDICT_MOON:
			print_mode='m';
			strcpy(name_str, "the Moon");
		break;
	}

	int iaz, iel, lastel=0;
	char string[MAX_NUM_CHARS], quit=0;
	double lastdaynum, rise=0.0;
	char time_string[MAX_NUM_CHARS];

	predict_julian_date_t daynum = GetStartTime(name_str);
	clear();
	struct predict_observation obs = {0};

	const double HORIZON_THRESHOLD = 0.03;
	const double REDUCTION_FACTOR = 0.004;

	double right_ascension = 0;
	double declination = 0;
	double longitude = 0;

	do {
		//determine sun- or moonrise
		celestial_predict(object, qth, daynum, &obs);

		while (rise==0.0) {
			if (fabs(obs.elevation*180.0/M_PI)<HORIZON_THRESHOLD) {
				rise=daynum;
			} else {
				daynum-=(REDUCTION_FACTOR*obs.elevation*180.0/M_PI);
				celestial_predict(object, qth, daynum, &obs);
			}
		}

		celestial_predict(object, qth, rise, &obs);
		daynum=rise;

		iaz=(int)rint(obs.azimuth*180.0/M_PI);
		iel=(int)rint(obs.elevation*180.0/M_PI);

		//display pass of sun or moon from rise
		do {
			//display data
			time_t epoch = predict_from_julian(daynum);
			strftime(time_string, MAX_NUM_CHARS, "%a %d%b%y %H:%M:%S", gmtime(&epoch));
			sprintf(string,"      %s%4d %4d  %5.1f  %5.1f  %5.1f  %6.1f%7.3f\n",time_string, iel, iaz, right_ascension, declination, longitude, obs.range_rate, obs.range);
			quit=Print("",string,print_mode);
			lastel=iel;
			lastdaynum=daynum;

			//calculate data
			daynum+=0.04*(cos(M_PI/180.0*(obs.elevation*180.0/M_PI+0.5)));
			celestial_predict(object, qth, daynum, &obs);
			iaz=(int)rint(obs.azimuth*180.0/M_PI);
			iel=(int)rint(obs.elevation*180.0/M_PI);
		} while (iel>3 && quit==0);

		//end the pass
		while (lastel!=0 && quit==0) {
			daynum=lastdaynum;

			//find sun/moon set
			do {
				daynum+=0.004*(sin(M_PI/180.0*(obs.elevation*180.0/M_PI+0.5)));
				celestial_predict(object, qth, daynum, &obs);
				iaz=(int)rint(obs.azimuth*180.0/M_PI);
				iel=(int)rint(obs.elevation*180.0/M_PI);
			} while (iel>0);

			time_t epoch = predict_from_julian(daynum);
			strftime(time_string, MAX_NUM_CHARS, "%a %d%b%y %H:%M:%S", gmtime(&epoch));

			sprintf(string,"      %s%4d %4d  %5.1f  %5.1f  %5.1f  %6.1f%7.3f\n",time_string, iel, iaz, right_ascension, declination, longitude, obs.range_rate, obs.range);
			quit=Print("",string,print_mode);
			lastel=iel;
		} //will continue until we have elevation 0 at the end of the pass

		quit=Print("","\n",'o');
		daynum+=0.4;
		rise=0.0;

	} while (quit==0);
}

bool KbEdit(int x, int y, int num_characters, char *output_string)
{
	char *input_string = (char*)malloc(sizeof(char)*num_characters);
	bool filled = false;

	echo();
	move(y-1,x-1);
	wgetnstr(stdscr,input_string,num_characters);

	if (strlen(input_string)!=0) {
		filled = true;
		strncpy(output_string,input_string,num_characters);
	}

	mvprintw(y-1,x-1,"%-25s",output_string);

	refresh();
	noecho();


	free(input_string);

	return filled;
}

void ShowOrbitData(struct tle_db *tle_db, predict_orbital_elements_t **orbital_elements_array)
{
	int c, x, namelength, age;
	double an_period, no_period, sma, c1, e2, satepoch;
	char days[5];

	x=Select(tle_db, orbital_elements_array);

	while (x!=-1) {
		predict_orbital_elements_t *orbital_elements = orbital_elements_array[x];
		if (orbital_elements->mean_motion!=0.0) {
			bkgdset(COLOR_PAIR(2)|A_BOLD);
			clear();
			sma=331.25*exp(log(1440.0/orbital_elements->mean_motion)*(2.0/3.0));
			an_period=1440.0/orbital_elements->mean_motion;
			c1=cos(orbital_elements->inclination*M_PI/180.0);
			e2=1.0-(orbital_elements->eccentricity*orbital_elements->eccentricity);
			no_period=(an_period*360.0)/(360.0+(4.97*pow((EARTH_RADIUS_KM/sma),3.5)*((5.0*c1*c1)-1.0)/(e2*e2))/orbital_elements->mean_motion);
			satepoch=DayNum(1,0,orbital_elements->epoch_year)+orbital_elements->epoch_day;
			age=(int)rint(predict_to_julian(time(NULL))-satepoch);

			if (age==1)
				strcpy(days,"day");
			else
				strcpy(days,"days");

			namelength=strlen(tle_db->tles[x].name);

			printw("\n");

			for (c=41; c>namelength; c-=2)
				printw(" ");

			bkgdset(COLOR_PAIR(3)|A_BOLD);
			attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
			clear();

			mvprintw(0,0,"                                                                                ");
			mvprintw(1,0,"  flyby Orbital Data                                                            ");
			mvprintw(2,0,"                                                                                ");

			mvprintw(1,25,"(%ld) %s", orbital_elements->satellite_number, tle_db->tles[x].name);

			attrset(COLOR_PAIR(4)|A_BOLD);
			mvprintw( 4, 4,"Data Issued        : ");
			mvprintw( 5, 4,"Reference Epoch    : ");
			mvprintw( 6, 4,"Inclination        : ");
			mvprintw( 7, 4,"RAAN               : ");
			mvprintw( 8, 4,"Eccentricity       : ");
			mvprintw( 9, 4,"Arg of Perigee     : ");
			mvprintw(10, 4,"Mean Anomaly       : ");
			mvprintw(11, 4,"Mean Motion        : ");
			mvprintw(12, 4,"Decay Rate         : ");
			mvprintw(13, 4,"Nddot/6 Drag       : ");
			mvprintw(14, 4,"Bstar Drag Factor  : ");
			mvprintw(15, 4,"Semi-Major Axis    : ");
			mvprintw(16, 4,"Apogee Altitude    : ");
			mvprintw(17, 4,"Perigee Altitude   : ");
			mvprintw(18, 4,"Anomalistic Period : ");
			mvprintw(19, 4,"Nodal Period       : ");
			mvprintw(20, 4,"Orbit Number       : ");
			mvprintw(21, 4,"Element Set Number : ");

			attrset(COLOR_PAIR(2)|A_BOLD);
			mvprintw( 4,25,"%d %s ago",age,days);
			mvprintw( 5,25,"%02d %.8f",orbital_elements->epoch_year,orbital_elements->epoch_day);
			mvprintw( 6,25,"%.4f deg",orbital_elements->inclination);
			mvprintw( 7,25,"%.4f deg",orbital_elements->right_ascension);
			mvprintw( 8,25,"%g",orbital_elements->eccentricity);
			mvprintw( 9,25,"%.4f deg",orbital_elements->argument_of_perigee);
			mvprintw(10,25,"%.4f deg",orbital_elements->mean_anomaly);
			mvprintw(11,25,"%.8f rev/day",orbital_elements->mean_motion);
			mvprintw(12,25,"%g rev/day/day",orbital_elements->derivative_mean_motion);
			mvprintw(13,25,"%g rev/day/day/day",orbital_elements->second_derivative_mean_motion);
			mvprintw(14,25,"%g 1/earth radii",orbital_elements->bstar_drag_term);
			mvprintw(15,25,"%.4f km",sma);
			mvprintw(16,25,"%.4f km",sma*(1.0+orbital_elements->eccentricity)-EARTH_RADIUS_KM);
			mvprintw(17,25,"%.4f km",sma*(1.0-orbital_elements->eccentricity)-EARTH_RADIUS_KM);
			mvprintw(18,25,"%.4f mins",an_period);
			mvprintw(19,25,"%.4f mins",no_period);
			mvprintw(20,25,"%ld",orbital_elements->revolutions_at_epoch);
			mvprintw(21,25,"%ld",orbital_elements->element_number);

			attrset(COLOR_PAIR(3)|A_BOLD);
			refresh();
			AnyKey();
		}
		x=Select(tle_db, orbital_elements_array);
	 };
}

void QthEdit(const char *qthfile, predict_observer_t *qth)
{
	//display current QTH information
	bkgdset(COLOR_PAIR(3)|A_BOLD);
	clear();

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(0,0,"                                                                                ");
	mvprintw(1,0,"  flyby Ground Station Location                                                 ");
	mvprintw(2,0,"                                                                                ");

	curs_set(1);

	attrset(COLOR_PAIR(4)|A_BOLD);
	mvprintw(11,20,"Station Callsign  : ");
	mvprintw(12,20,"Station Latitude  : ");
	mvprintw(13,20,"Station Longitude : ");
	mvprintw(14,20,"Station Altitude  : ");

	attrset(COLOR_PAIR(2)|A_BOLD);
	mvprintw(11,43,"%s",qth->name);
	mvprintw(12,43,"%g [DegN]",qth->latitude*180.0/M_PI);
	mvprintw(13,43,"%g [DegE]",qth->longitude*180.0/M_PI);
	mvprintw(14,43,"%d",qth->altitude);

	refresh();

	#define INPUT_NUM_CHARS 128
	char input_string[INPUT_NUM_CHARS] = {0};
	bool should_save = false;

	//edit QTH name
	mvprintw(18,15, " Enter the callsign of your ground station");
	strncpy(input_string, qth->name, INPUT_NUM_CHARS);
	if (KbEdit(44, 12, INPUT_NUM_CHARS, input_string)) {
		strncpy(qth->name, input_string, INPUT_NUM_CHARS);
		should_save = true;
	}

	//edit latitude
	mvprintw(18,15," Enter your latitude in degrees NORTH      ");
	mvprintw(19,15," Decimal (74.2467) or DMS (74 14 48) format allowed");
	sprintf(input_string,"%g [DegN]",qth->latitude*180.0/M_PI);
	if (KbEdit(44,13, INPUT_NUM_CHARS, input_string)) {
		qth->latitude = ReadBearing(input_string)*M_PI/180.0;
		should_save = true;
	}

	//edit longitude
	mvprintw(18,15," Enter your longitude in degrees EAST      ");
	sprintf(input_string, "%g [DegE]", qth->longitude*180.0/M_PI);
	if (KbEdit(44, 14, INPUT_NUM_CHARS, input_string)) {
		qth->longitude = ReadBearing(input_string)*M_PI/180.0;
		should_save = true;
	}
	move(19,15);
	clrtoeol();

	//edit altitude
	mvprintw(18,15," Enter your altitude above sea level (in meters)      ");
	sprintf(input_string, "%d", (int)floor(qth->altitude));
	if (KbEdit(44,15, INPUT_NUM_CHARS, input_string)) {
		qth->altitude = strtod(input_string, NULL);
		should_save = true;
	}

	if (should_save) {
		qth_to_file(qthfile, qth);
	}
}

/**
 * Get next enabled entry within the TLE database. Used for navigating between enabled satellites within SingleTrack().
 *
 * \param curr_index Current index
 * \param step Step used for finding next enabled entry (-1 or +1, preferably)
 * \param tle_db TLE database
 * \return Next entry in step direction which is enabled, or curr_index if none was found
 **/
int get_next_enabled_satellite(int curr_index, int step, struct tle_db *tle_db)
{
	int index = curr_index;
	index += step;
	while ((index >= 0) && (index < tle_db->num_tles)) {
		if (tle_db_entry_enabled(tle_db, index)) {
			return index;
		}
		index += step;
	}
	return curr_index;
}

void SingleTrack(int orbit_ind, predict_orbital_elements_t **orbital_elements_array, predict_observer_t *qth, struct transponder_db *sat_db, struct tle_db *tle_db, rotctld_info_t *rotctld, rigctld_info_t *downlink_info, rigctld_info_t *uplink_info)
{
	bool once_per_second = rotctld->once_per_second;
	double horizon = rotctld->tracking_horizon;

	struct sat_db_entry *sat_db_entries = sat_db->sats;
	struct tle_db_entry *tle_db_entries = tle_db->tles;

	int     ans;
	bool	downlink_update=true, uplink_update=true, readfreq=false;

	do {
		int     length, xponder=0,
			polarity=0;
		bool	aos_alarm=0;
		double	nextaos=0.0, lostime=0.0, aoslos=0.0,
			downlink=0.0, uplink=0.0, downlink_start=0.0,
			downlink_end=0.0, uplink_start=0.0, uplink_end=0.0;

		double doppler100, delay;
		double dopp;
		double loss;

		//elevation and azimuth at previous timestep, for checking when to send messages to rotctld
		int prev_elevation = 0;
		int prev_azimuth = 0;
		time_t prev_time = 0;

		char ephemeris_string[MAX_NUM_CHARS];

		char time_string[MAX_NUM_CHARS];

		predict_orbital_elements_t *orbital_elements = orbital_elements_array[orbit_ind];
		struct predict_orbit orbit;
		struct sat_db_entry sat_db = sat_db_entries[orbit_ind];

		switch (orbital_elements->ephemeris) {
			case EPHEMERIS_SGP4:
				strcpy(ephemeris_string, "SGP4");
			break;
			case EPHEMERIS_SDP4:
				strcpy(ephemeris_string, "SDP4");
			break;
			case EPHEMERIS_SGP8:
				strcpy(ephemeris_string, "SGP8");
			break;
			case EPHEMERIS_SDP8:
				strcpy(ephemeris_string, "SDP8");
			break;
		}

		bool comsat = sat_db.num_transponders > 0;

		if (comsat) {
			downlink_start=sat_db.downlink_start[xponder];
			downlink_end=sat_db.downlink_end[xponder];
			uplink_start=sat_db.uplink_start[xponder];
			uplink_end=sat_db.uplink_end[xponder];

			if (downlink_start>downlink_end)
				polarity=-1;

			if (downlink_start<downlink_end)
				polarity=1;

			if (downlink_start==downlink_end)
				polarity=0;

			downlink=0.5*(downlink_start+downlink_end);
			uplink=0.5*(uplink_start+uplink_end);
		} else {
			downlink_start=0.0;
			downlink_end=0.0;
			uplink_start=0.0;
			uplink_end=0.0;
			polarity=0;
			downlink=0.0;
			uplink=0.0;
		}

		bool aos_happens = predict_aos_happens(orbital_elements, qth->latitude);
		bool geostationary = predict_is_geostationary(orbital_elements);

		predict_julian_date_t daynum = predict_to_julian(time(NULL));
		predict_orbit(orbital_elements, &orbit, daynum);
		bool decayed = orbit.decayed;

		halfdelay(HALF_DELAY_TIME);
		curs_set(0);
		bkgdset(COLOR_PAIR(3));
		clear();

		attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
		mvprintw(0,0,"                                                                                ");
		mvprintw(1,0,"  flyby Tracking:                                                               ");
		mvprintw(2,0,"                                                                                ");
		mvprintw(1,21,"%-24s (%d)", tle_db_entries[orbit_ind].name, orbital_elements->satellite_number);

		attrset(COLOR_PAIR(4)|A_BOLD);

		mvprintw(4,1,"Satellite     Direction     Velocity     Footprint    Altitude     Slant Range");

		mvprintw(5,1,"        .            Az           mi            mi          mi              mi");
		mvprintw(6,1,"        .            El           km            km          km              km");

		mvprintw(17,1,"Eclipse Depth   Orbital Phase   Orbital Model   Squint Angle      AutoTracking");


		if (comsat) {
			mvprintw(11,1,"Uplink   :");
			mvprintw(12,1,"Downlink :");
			mvprintw(13,1,"Delay    :");
			mvprintw(13,55,"Echo      :");
			mvprintw(12,29,"RX:");
			mvprintw(12,55,"Path loss :");
			mvprintw(11,29,"TX:");
			mvprintw(11,55,"Path loss :");
		}

		do {
			if (downlink_info->connected && readfreq)
				downlink = rigctld_read_frequency(downlink_info)/(1+1.0e-08*doppler100);
			if (uplink_info->connected && readfreq)
				uplink = rigctld_read_frequency(uplink_info)/(1-1.0e-08*doppler100);


			//predict and observe satellite orbit
			time_t epoch = time(NULL);
			daynum = predict_to_julian(epoch);
			predict_orbit(orbital_elements, &orbit, daynum);
			struct predict_observation obs;
			predict_observe_orbit(qth, &orbit, &obs);
			double sat_vel = sqrt(pow(orbit.velocity[0], 2.0) + pow(orbit.velocity[1], 2.0) + pow(orbit.velocity[2], 2.0));
			double squint = predict_squint_angle(qth, &orbit, sat_db.alon, sat_db.alat);

			//display current time
			attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
			strftime(time_string, MAX_NUM_CHARS, "%a %d%b%y %j.%H:%M:%S", gmtime(&epoch));
			mvprintw(1,54,"%s",time_string);

			attrset(COLOR_PAIR(4)|A_BOLD);
			mvprintw(5,8,"N");
			mvprintw(6,8,"E");

			//display satellite data
			attrset(COLOR_PAIR(2)|A_BOLD);
			mvprintw(5,1,"%-6.2f",orbit.latitude*180.0/M_PI);

			attrset(COLOR_PAIR(2)|A_BOLD);
			mvprintw(5,55,"%0.f ",orbit.altitude*KM_TO_MI);
			mvprintw(6,55,"%0.f ",orbit.altitude);
			mvprintw(5,68,"%-5.0f",obs.range*KM_TO_MI);
			mvprintw(6,68,"%-5.0f",obs.range);
			mvprintw(6,1,"%-7.2f",orbit.longitude*180.0/M_PI);
			mvprintw(5,15,"%-7.2f",obs.azimuth*180.0/M_PI);
			mvprintw(6,14,"%+-6.2f",obs.elevation*180.0/M_PI);
			mvprintw(5,29,"%0.f ",(3600.0*sat_vel)*KM_TO_MI);
			mvprintw(6,29,"%0.f ",3600.0*sat_vel);
			mvprintw(18,3,"%+6.2f deg",orbit.eclipse_depth*180.0/M_PI);
			mvprintw(18,20,"%5.1f",256.0*(orbit.phase/(2*M_PI)));
			mvprintw(18,37,"%s",ephemeris_string);
			if (sat_db.squintflag) {
				mvprintw(18,52,"%+6.2f",squint);
			} else {
				mvprintw(18,52,"N/A");
			}
			mvprintw(5,42,"%0.f ",orbit.footprint*KM_TO_MI);
			mvprintw(6,42,"%0.f ",orbit.footprint);

			attrset(COLOR_PAIR(1)|A_BOLD);
			mvprintw(20,1,"Orbit Number: %ld", orbit.revolutions);

			mvprintw(22,1,"Spacecraft is currently ");
			if (obs.visible) {
				mvprintw(22,25,"visible    ");
			} else if (!(orbit.eclipsed)) {
				mvprintw(22,25,"in sunlight");
			} else {
				mvprintw(22,25,"in eclipse ");
			}

			//display satellite AOS/LOS information
			if (geostationary && (obs.elevation>=0.0)) {
				mvprintw(21,1,"Satellite orbit is geostationary");
				aoslos=-3651.0;
			} else if ((obs.elevation>=0.0) && !geostationary && !decayed && daynum>lostime) {
				predict_julian_date_t calc_time = daynum;
				lostime = predict_next_los(qth, orbital_elements, calc_time);
				time_t epoch = predict_from_julian(lostime);
				strftime(time_string, MAX_NUM_CHARS, "%a %d%b%y %j.%H:%M:%S", gmtime(&epoch));
				mvprintw(21,1,"LOS at: %s %s  ",time_string, "GMT");
				aoslos=lostime;
			} else if (obs.elevation<0.0 && !geostationary && !decayed && aos_happens && daynum>aoslos) {
				predict_julian_date_t calc_time = daynum + 0.003;  /* Move ahead slightly... */
				nextaos=predict_next_aos(qth, orbital_elements, calc_time);
				time_t epoch = predict_from_julian(nextaos);
				strftime(time_string, MAX_NUM_CHARS, "%a %d%b%y %j.%H:%M:%S", gmtime(&epoch));
				mvprintw(21,1,"Next AOS: %s %s",time_string, "GMT");
				aoslos=nextaos;
			} else if (decayed || !aos_happens || (geostationary && (obs.elevation<0.0))){
				mvprintw(21,1,"This satellite never reaches AOS");
				aoslos=-3651.0;
			}

			//predict and observe sun and moon
			struct predict_observation sun;
			predict_observe_sun(qth, daynum, &sun);

			struct predict_observation moon;
			predict_observe_moon(qth, daynum, &moon);

			//display sun and moon
			attrset(COLOR_PAIR(4)|A_REVERSE|A_BOLD);
			mvprintw(20,55,"   Sun   ");
			mvprintw(20,70,"   Moon  ");
			if (sun.elevation > 0.0)
				attrset(COLOR_PAIR(3)|A_BOLD);
			else
				attrset(COLOR_PAIR(2));
			mvprintw(21,55,"%-7.2fAz",sun.azimuth*180.0/M_PI);
			mvprintw(22,55,"%+-6.2f El",sun.elevation*180.0/M_PI);

			attrset(COLOR_PAIR(3)|A_BOLD);
			if (moon.elevation > 0.0)
				attrset(COLOR_PAIR(1)|A_BOLD);
			else
				attrset(COLOR_PAIR(1));
			mvprintw(21,70,"%-7.2fAz",moon.azimuth*180.0/M_PI);
			mvprintw(22,70,"%+-6.2f El",moon.elevation*180.0/M_PI);

			attrset(COLOR_PAIR(2)|A_BOLD);

			//display downlink/uplink information
			if (comsat) {
				length=strlen(sat_db.transponder_name[xponder])/2;
	      mvprintw(10,0,"                                                                                ");
				mvprintw(10,40-length,"%s",sat_db.transponder_name[xponder]);

				if (downlink!=0.0)
					mvprintw(12,11,"%11.5f MHz%c%c%c",downlink,
					readfreq ? '<' : ' ',
					(readfreq || downlink_update) ? '=' : ' ',
					downlink_update ? '>' : ' ');

				else
					mvprintw(12,11,"               ");

				if (uplink!=0.0)
					mvprintw(11,11,"%11.5f MHz%c%c%c",uplink,
					readfreq ? '<' : ' ',
					(readfreq || uplink_update) ? '=' : ' ',
					uplink_update ? '>' : ' ');

				else
					mvprintw(11,11,"               ");
			}

			//calculate and display downlink/uplink information during pass, and control rig if available
			doppler100=-100.0e06*((obs.range_rate*1000.0)/299792458.0);
			delay=1000.0*((1000.0*obs.range)/299792458.0);
			if (obs.elevation>=horizon) {
				if (obs.elevation>=0 && aos_alarm==0) {
					beep();
					aos_alarm=1;
				}

				if (comsat) {
					attrset(COLOR_PAIR(4)|A_BOLD);

					if (fabs(obs.range_rate)<0.1)
						mvprintw(13,34,"    TCA    ");
					else {
						if (obs.range_rate<0.0)
							mvprintw(13,34,"Approaching");

						if (obs.range_rate>0.0)
							mvprintw(13,34,"  Receding ");
					}

					attrset(COLOR_PAIR(2)|A_BOLD);

					if (downlink!=0.0) {
						dopp=1.0e-08*(doppler100*downlink);
						mvprintw(12,32,"%11.5f MHz",downlink+dopp);
						loss=32.4+(20.0*log10(downlink))+(20.0*log10(obs.range));
						mvprintw(12,67,"%7.3f dB",loss);
						mvprintw(13,13,"%7.3f   ms",delay);
						if (downlink_info->connected && downlink_update)
							rigctld_set_frequency(downlink_info, downlink+dopp);
					}

					else
					{
						mvprintw(12,32,"                ");
						mvprintw(12,67,"          ");
						mvprintw(13,13,"            ");
					}
					if (uplink!=0.0) {
						dopp=1.0e-08*(doppler100*uplink);
						mvprintw(11,32,"%11.5f MHz",uplink-dopp);
						loss=32.4+(20.0*log10(uplink))+(20.0*log10(obs.range));
						mvprintw(11,67,"%7.3f dB",loss);
						if (uplink_info->connected && uplink_update)
							rigctld_set_frequency(uplink_info, uplink-dopp);
					}
					else
					{
						mvprintw(11,32,"                ");
						mvprintw(11,67,"          ");
					}

					if (uplink!=0.0 && downlink!=0.0)
						mvprintw(12,67,"%7.3f ms",2.0*delay);
					else
						mvprintw(13,67,"              ");
				}

			} else {
				lostime=0.0;
				aos_alarm=0;

				if (comsat) {
					mvprintw(11,32,"                ");
					mvprintw(11,67,"          ");
					mvprintw(12,32,"                ");
					mvprintw(12,67,"          ");
					mvprintw(13,13,"            ");
					mvprintw(13,34,"           ");
					mvprintw(13,67,"          ");
				}
			}

			//display rotation information
			if (rotctld->connected) {
				if (obs.elevation>=horizon)
					mvprintw(17,67,"   Active   ");
				else
					mvprintw(17,67,"Standing  By");
			} else
				mvprintw(18,67,"Not  Enabled");


			/* Send data to rotctld, either when it changes, or at a given interval
       * (as specified by the user). TODO: Implement this, currently fixed at
       * once per second. */
			if (obs.elevation*180.0/M_PI >= horizon) {
				time_t curr_time = time(NULL);
				int elevation = (int)round(obs.elevation*180.0/M_PI);
				int azimuth = (int)round(obs.azimuth*180.0/M_PI);

				if ((elevation != prev_elevation) || (azimuth != prev_azimuth) || (once_per_second && (curr_time > prev_time))) {
					if (rotctld->connected) rotctld_track(rotctld, obs.azimuth*180.0/M_PI, obs.elevation*180.0/M_PI);
					prev_elevation = elevation;
					prev_azimuth = azimuth;
					prev_time = curr_time;
				}
			}

			/* Get input from keyboard */

			ans=getch();

			if (comsat) {
				if (ans==' ' && sat_db.num_transponders>1) {
					xponder++;

					if (xponder>=sat_db.num_transponders)
						xponder=0;

					move(9,1);
					clrtoeol();

					downlink_start=sat_db.downlink_start[xponder];
					downlink_end=sat_db.downlink_end[xponder];
					uplink_start=sat_db.uplink_start[xponder];
					uplink_end=sat_db.uplink_end[xponder];

					if (downlink_start>downlink_end)
						polarity=-1;

					if (downlink_start<downlink_end)
						polarity=1;

					if (downlink_start==downlink_end)
						polarity=0;

					downlink=0.5*(downlink_start+downlink_end);
					uplink=0.5*(uplink_start+uplink_end);
				}

				double shift = 0;

				/* Raise uplink frequency */
				if (ans==KEY_UP || ans=='>' || ans=='.') {
					if (ans==KEY_UP || ans=='>')
						shift=0.001;  /* 1 kHz */
					else
						shift=0.0001; /* 100 Hz */
				}

				/* Lower uplink frequency */
				if (ans==KEY_DOWN || ans=='<' || ans== ',') {
					if (ans==KEY_DOWN || ans=='<')
						shift=-0.001;  /* 1 kHz */
					else
						shift=-0.0001; /* 100 Hz */
				}

				uplink+=shift*(double)abs(polarity);
				downlink=downlink+(shift*(double)polarity);

				if (uplink < uplink_start) {
					uplink=uplink_end;
					downlink=downlink_end;
				}
				if (uplink > uplink_end) {
					uplink=uplink_start;
					downlink=downlink_start;
				}

				if (ans=='d')
					downlink_update=true;
				if (ans=='D')
					downlink_update=false;
				if (ans=='u')
					uplink_update=true;
				if (ans=='U')
					uplink_update=false;
				if (ans=='f' || ans=='F')
				{
					if (downlink_info->connected)
						downlink = rigctld_read_frequency(downlink_info)/(1+1.0e-08*doppler100);
					if (uplink_info->connected)
						uplink = rigctld_read_frequency(uplink_info)/(1-1.0e-08*doppler100);
					if (ans=='f')
					{
						downlink_update=true;
						uplink_update=true;
					}
				}
				if (ans=='m')
					readfreq=true;
				if (ans=='M')
					readfreq=false;
				if (ans=='x') // Reverse VFO uplink and downlink names
				{
					if (downlink_info->connected && uplink_info->connected)
					{
						char tmp_vfo[MAX_NUM_CHARS];
						strncpy(tmp_vfo, downlink_info->vfo_name, MAX_NUM_CHARS);
						strncpy(downlink_info->vfo_name, uplink_info->vfo_name, MAX_NUM_CHARS);
						strncpy(uplink_info->vfo_name, tmp_vfo, MAX_NUM_CHARS);
					}
				}
			}

			refresh();

			if ((ans == KEY_LEFT) || (ans == '-')) {
				orbit_ind = get_next_enabled_satellite(orbit_ind, -1, tle_db);
			}

			if ((ans == KEY_RIGHT) || (ans == '+')) {
				orbit_ind = get_next_enabled_satellite(orbit_ind, +1, tle_db);
			}

			halfdelay(HALF_DELAY_TIME);

		} while (ans!='q' && ans!='Q' && ans!=27 &&
		 	ans!='+' && ans!='-' &&
		 	ans!=KEY_LEFT && ans!=KEY_RIGHT);

	} while (ans!='q' && ans!=17);

	cbreak();
}

NCURSES_ATTR_T MultiColours(scrk, scel)
double scrk, scel;
{
	if (scrk < 8000)
		if (scrk < 4000)
			if (scrk < 2000)
				if (scrk < 1000)
					if (scel > 10)
						return (COLOR_PAIR(6)|A_REVERSE); /* red */
					else
						return (COLOR_PAIR(3)|A_REVERSE); /* yellow */
				else
					if (scel > 20)
						return (COLOR_PAIR(3)|A_REVERSE); /* yellow */
					else
						return (COLOR_PAIR(4)|A_REVERSE); /* cyan */
			else
				if (scel > 40)
					return (COLOR_PAIR(4)|A_REVERSE); /* cyan */
				else
					return (COLOR_PAIR(1)|A_REVERSE); /* white */
		else
			return (COLOR_PAIR(1)|A_REVERSE); /* white */
	else
		return (COLOR_PAIR(2)|A_REVERSE); /* reverse */
}

void MultiTrack(predict_observer_t *qth, predict_orbital_elements_t **input_orbital_elements_array, struct tle_db *tle_db, char multitype, char disttype)
{
	int num_orbits = tle_db->num_tles;
	int 		*satindex = (int*)malloc(sizeof(int)*num_orbits);

	double		*aos = (double*)malloc(sizeof(double)*num_orbits);
	double		*los = (double*)malloc(sizeof(double)*num_orbits);
	char ans = '\0';

	struct predict_observation *observations = (struct predict_observation*)malloc(sizeof(struct predict_observation)*num_orbits);
	struct predict_orbit *orbits = (struct predict_orbit*)malloc(sizeof(struct predict_orbit)*num_orbits);
	NCURSES_ATTR_T *attributes = (NCURSES_ATTR_T*)malloc(sizeof(NCURSES_ATTR_T)*num_orbits);
	char **string_lines = (char**)malloc(sizeof(char*)*num_orbits);
	for (int i=0; i < num_orbits; i++) {
		string_lines[i] = (char*)malloc(sizeof(char)*MAX_NUM_CHARS);
	}
	predict_orbital_elements_t **orbital_elements_array = (predict_orbital_elements_t**)malloc(sizeof(predict_orbital_elements_t*)*num_orbits);
	int *entry_mapping = (int*)calloc(num_orbits, sizeof(int));

	//select only enabled satellites
	int enabled_ind = 0;
	for (int i=0; i < num_orbits; i++) {
		if (tle_db_entry_enabled(tle_db, i)) {
			orbital_elements_array[enabled_ind] = input_orbital_elements_array[i];
			entry_mapping[enabled_ind] = i;
			enabled_ind++;
		}
	}
	num_orbits = enabled_ind;


	curs_set(0);
	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	clear();

	mvprintw(0,0,"                                                                                ");
	mvprintw(1,0,"  flyby Real-Time Multi-Tracking                                                ");
	mvprintw(2,0,"                                                                                ");

	attrset(COLOR_PAIR(2)|A_REVERSE);

	mvprintw(3,0,"  Satellite     Azim   Elev  Lat Long    Alt  Range     Next AOS/LOS            ");

	attrset(COLOR_PAIR(4)|A_REVERSE|A_BOLD);
	mvprintw(5,70,"   QTH   ");
	attrset(COLOR_PAIR(2));
	mvprintw(6,70,"%9s",Abbreviate(qth->name,9));
	char maidenstr[9];
	getMaidenHead(qth->latitude*180.0/M_PI, -qth->longitude*180.0/M_PI, maidenstr);
	mvprintw(7,70,"%9s",maidenstr);

	predict_julian_date_t daynum = predict_to_julian(time(NULL));
	char time_string[MAX_NUM_CHARS];

	for (int x=0; x < num_orbits; x++) {
		predict_orbit(orbital_elements_array[x], &orbits[x], daynum);
		los[x]=0.0;
		aos[x]=0.0;
		satindex[x]=x;
	}

	do {
		attrset(COLOR_PAIR(2)|A_REVERSE);
		mvprintw(3,28,(multitype=='m') ? " Locator " : " Lat Long");
		attrset(COLOR_PAIR(2));
		mvprintw(12,70,(disttype=='i') ? "  (miles)" : "     (km)");
		mvprintw(13,70,"    (GMT)");

		attrset(COLOR_PAIR(4)|A_REVERSE|A_BOLD);
		mvprintw( 9,70," Control ");
		attrset(COLOR_PAIR(2)|A_REVERSE);
		mvprintw(10,70," ik lm q ");
		mvprintw(10,(disttype=='i') ? 71 : 72," ");
		mvprintw(10,(multitype=='m') ? 75 : 74," ");


		daynum = predict_to_julian(time(NULL));

		//predict orbits
		for (int i=0; i < num_orbits; i++){
			struct predict_orbit *orbit = &(orbits[i]);

			struct predict_observation obs;
			predict_orbit(orbital_elements_array[i], orbit, daynum);
			predict_observe_orbit(qth, orbit, &obs);

			//sun status
			char sunstat;
			if (!orbit->eclipsed) {
				if (obs.visible) {
					sunstat='V';
				} else {
					sunstat='D';
				}
			} else {
				sunstat='N';
			}

			//satellite approaching status
			char rangestat;
			if (fabs(obs.range_rate) < 0.1) {
				rangestat = '=';
			} else if (obs.range_rate < 0.0) {
				rangestat = '/';
			} else if (obs.range_rate > 0.0) {
				rangestat = '\\';
			}

			//set text formatting attributes according to satellite state, set AOS/LOS string
			bool can_predict = !predict_is_geostationary(orbital_elements_array[i]) && predict_aos_happens(orbital_elements_array[i], qth->latitude) && !(orbits[i].decayed);
			char aos_los[MAX_NUM_CHARS] = {0};
			if (obs.elevation >= 0) {
				//different colours according to range and elevation
				attributes[i] = MultiColours(obs.range, obs.elevation*180/M_PI);

				if (predict_is_geostationary(orbital_elements_array[i])){
					sprintf(aos_los, "*GeoS*");
				} else {
					time_t epoch = predict_from_julian(los[i] - daynum);
					strftime(aos_los, MAX_NUM_CHARS, "%M:%S", gmtime(&epoch)); //time until LOS
				}
			} else if ((obs.elevation < 0) && can_predict) {
				if ((aos[i]-daynum) < 0.00694) {
					//satellite is close, set bold
					attributes[i] = COLOR_PAIR(2);
					time_t epoch = predict_from_julian(aos[i] - daynum);
					strftime(aos_los, MAX_NUM_CHARS, "%M:%S", gmtime(&epoch)); //minutes and seconds left until AOS
				} else {
					//satellite is far, set normal coloring
					attributes[i] = COLOR_PAIR(4);
					time_t epoch = predict_from_julian(aos[i]);
					strftime(aos_los, MAX_NUM_CHARS, "%j.%H:%M:%S", gmtime(&epoch)); //time for AOS
				}
			} else if (!can_predict) {
				attributes[i] = COLOR_PAIR(3);
				sprintf(aos_los, "*GeoS-NoAOS*");
			}

			char abs_pos_string[MAX_NUM_CHARS] = {0};
			if (multitype == 'm') {
				getMaidenHead(orbit->latitude*180.0/M_PI, orbit->longitude*180.0/M_PI, abs_pos_string);
			} else {
				sprintf(abs_pos_string, "%3.0f  %3.0f", orbit->latitude*180.0/M_PI, orbit->longitude*180.0/M_PI);
			}

			/* Calculate Next Event (AOS/LOS) Times */
			if (can_predict && (daynum > los[i]) && (obs.elevation > 0)) {
				los[i]= predict_next_los(qth, orbital_elements_array[i], daynum);
			}

			if (can_predict && (daynum > aos[i])) {
				if (obs.elevation < 0) {
					aos[i] = predict_next_aos(qth, orbital_elements_array[i], daynum);
				}
			}

			//altitude and range in km/miles
			double disp_altitude = orbit->altitude;
			double disp_range = obs.range;
			if (disttype == 'i') {
				disp_altitude = disp_altitude*KM_TO_MI;
				disp_range = disp_range*KM_TO_MI;
			}

			//set string to display
			char disp_string[MAX_NUM_CHARS];
			sprintf(disp_string, " %-13s%5.1f  %5.1f %8s  %6.0f %6.0f %c %c %12s ", Abbreviate(tle_db->tles[entry_mapping[i]].name, 12), obs.azimuth*180.0/M_PI, obs.elevation*180.0/M_PI, abs_pos_string, disp_altitude, disp_range, sunstat, rangestat, aos_los);

			//overwrite everything if orbit was decayed
			if (orbit->decayed) {
				attributes[i] = COLOR_PAIR(2);
				sprintf(disp_string, " %-10s   ----------------       Decayed        ---------------", Abbreviate(tle_db->tles[entry_mapping[i]].name,9));
			}

			memcpy(string_lines[i], disp_string, sizeof(char)*MAX_NUM_CHARS);
			observations[i] = obs;
		}

		//predict and observe sun and moon
		struct predict_observation sun;
		predict_observe_sun(qth, daynum, &sun);

		struct predict_observation moon;
		predict_observe_moon(qth, daynum, &moon);

		//display sun and moon
		attrset(COLOR_PAIR(4)|A_REVERSE|A_BOLD);
		mvprintw(16,70,"   Sun   ");
		mvprintw(20,70,"   Moon  ");
		if (sun.elevation > 0.0)
			attrset(COLOR_PAIR(3)|A_BOLD);
		else
			attrset(COLOR_PAIR(2));
		mvprintw(17,70,"%-7.2fAz",sun.azimuth*180.0/M_PI);
		mvprintw(18,70,"%+-6.2f El",sun.elevation*180.0/M_PI);

		attrset(COLOR_PAIR(3)|A_BOLD);
		if (moon.elevation > 0.0)
			attrset(COLOR_PAIR(1)|A_BOLD);
		else
			attrset(COLOR_PAIR(1));
		mvprintw(21,70,"%-7.2fAz",moon.azimuth*180.0/M_PI);
		mvprintw(22,70,"%+-6.2f El",moon.elevation*180.0/M_PI);


		//sort satellites before displaying them

		//those with elevation > 0 at the top
		int above_horizon_counter = 0;
		for (int i=0; i < num_orbits; i++){
			if (observations[i].elevation > 0) {
				satindex[above_horizon_counter] = i;
				above_horizon_counter++;
			}
		}

		//satellites that will eventually rise above the horizon
		int below_horizon_counter = 0;
		for (int i=0; i < num_orbits; i++){
			bool will_be_visible = !(orbits[i].decayed) && predict_aos_happens(orbital_elements_array[i], qth->latitude) && !predict_is_geostationary(orbital_elements_array[i]);
			if ((observations[i].elevation <= 0) && will_be_visible) {
				satindex[below_horizon_counter + above_horizon_counter] = i;
				below_horizon_counter++;
			}
		}

		//satellites that will never be visible, with decayed orbits last
		int nevervisible_counter = 0;
		int decayed_counter = 0;
		for (int i=0; i < num_orbits; i++){
			bool never_visible = !predict_aos_happens(orbital_elements_array[i], qth->latitude) || predict_is_geostationary(orbital_elements_array[i]);
			if ((observations[i].elevation <= 0) && never_visible && !(orbits[i].decayed)) {
				satindex[below_horizon_counter + above_horizon_counter + nevervisible_counter] = i;
				nevervisible_counter++;
			} else if (orbits[i].decayed) {
				satindex[num_orbits - 1 - decayed_counter] = i;
				decayed_counter++;
			}
		}

		//sort internally according to AOS/LOS
		for (int i=0; i < above_horizon_counter + below_horizon_counter; i++) {
			for (int j=0; j < above_horizon_counter + below_horizon_counter - 1; j++){
				if (aos[satindex[j]]>aos[satindex[j+1]]) {
					int x = satindex[j];
					satindex[j] = satindex[j+1];
					satindex[j+1] = x;
				}
			}
		}

		attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);

		time_t epoch = time(NULL);
		daynum=predict_to_julian(epoch);
		strftime(time_string, MAX_NUM_CHARS, "%a %d%b%y %j.%H%M%S", gmtime(&epoch));
		mvprintw(1,54,"%s",time_string);
		mvprintw(1,35,"(%d/%d in view)  ", above_horizon_counter, num_orbits);

		//display satellites
		int line = 5;
		for (int i=0; i < above_horizon_counter; i++) {
			attrset(attributes[satindex[i]]);
			mvprintw((line++), 1, "%s", string_lines[satindex[i]]);
		}
		attrset(0);
		mvprintw((line++), 1, "                                                                   ");
		for (int i=above_horizon_counter; i < (below_horizon_counter + above_horizon_counter + nevervisible_counter); i++) {
			attrset(attributes[satindex[i]]);
			mvprintw((line++), 1, "%s", string_lines[satindex[i]]);
		}
		attrset(0);
		mvprintw((line++), 1, "                                                                   ");
		for (int i=above_horizon_counter + below_horizon_counter + nevervisible_counter; i < num_orbits; i++) {
			attrset(attributes[satindex[i]]);
			mvprintw((line++), 1, "%s", string_lines[satindex[i]]);
		}

		if (num_orbits == 0) {
			mvprintw((line++), 1, "Satellite list is empty. Are any satellites enabled?");
			mvprintw((line++), 1, "(Go back to main menu and press 'W')");
		}

		refresh();
		halfdelay(HALF_DELAY_TIME);  // Increase if CPU load is too high
		ans=tolower(getch());

		if (ans=='m') multitype='m';
		if (ans=='l') multitype='l';
		if (ans=='k') disttype ='k';
		if (ans=='i') disttype ='i';

	} while (ans!='q' && ans!=27);

	cbreak();

	free(satindex);
	free(aos);
	free(los);
	free(observations);
	free(attributes);
	for (int i=0; i < tle_db->num_tles; i++) {
		free(string_lines[i]);
	}
	free(orbits);
	free(string_lines);
	free(orbital_elements_array);
	free(entry_mapping);
}

void Illumination(const char *name, predict_orbital_elements_t *orbital_elements)
{
	double startday, oneminute, sunpercent;
	int eclipses, minutes, quit, breakout=0, count;
	char string1[MAX_NUM_CHARS], string[MAX_NUM_CHARS], datestring[MAX_NUM_CHARS];

	oneminute=1.0/(24.0*60.0);

	predict_julian_date_t daynum = floor(GetStartTime(name));
	startday=daynum;
	count=0;

	curs_set(0);
	clear();

	const int NUM_MINUTES = 1440;

	struct predict_orbit orbit;


	do {
		attrset(COLOR_PAIR(4));
		mvprintw(LINES - 2,6,"                 Calculating... Press [ESC] To Quit");
		refresh();

		count++;
		daynum=startday;

		mvprintw(1,60, "%s (%d)", name, orbital_elements->satellite_number);

		for (minutes=0, eclipses=0; minutes<NUM_MINUTES; minutes++) {
			predict_orbit(orbital_elements, &orbit, daynum);

			if (orbit.eclipsed) {
				eclipses++;
			}

			daynum=startday+(oneminute*(double)minutes);
		}

		sunpercent=((double)eclipses)/((double)minutes);
		sunpercent=100.0-(sunpercent*100.0);

		time_t epoch = predict_from_julian(startday);
		strftime(datestring, MAX_NUM_CHARS, "%a %d%b%y %H:%M:%S", gmtime(&epoch));
		datestring[11]=0;

		sprintf(string1,"      %s    %4d    %6.2f%c",datestring,NUM_MINUTES-eclipses,sunpercent,37);

		/* Allow a quick way out */

		nodelay(stdscr,TRUE);

		if (getch()==27)
			breakout=1;

		nodelay(stdscr,FALSE);

		startday+= (LINES-8);

		daynum=startday;

		for (minutes=0, eclipses=0; minutes<NUM_MINUTES; minutes++) {
			predict_orbit(orbital_elements, &orbit, daynum);

			if (orbit.eclipsed) {
				eclipses++;
			}

			daynum=startday+(oneminute*(double)minutes);
		}

		sunpercent=((double)eclipses)/((double)minutes);
		sunpercent=100.0-(sunpercent*100.0);

		epoch = predict_from_julian(startday);
		strftime(datestring, MAX_NUM_CHARS, "%a %d%b%y %H:%M:%S", gmtime(&epoch));

		datestring[11]=0;
		sprintf(string,"%s\t %s    %4d    %6.2f%c\n",string1,datestring,1440-eclipses,sunpercent,37);

		char title[MAX_NUM_CHARS] = {0};
		sprintf(title, "%s (%d)", name, orbital_elements->satellite_number);

		quit=Print(title,string,'s');

		/* Allow a quick way out */

		nodelay(stdscr,TRUE);

		if (getch()==27)
			breakout=1;

		nodelay(stdscr,FALSE);

		if (count< (LINES-8))
			startday-= (LINES-9);
		else {
			count=0;
			startday+=1.0;
		}
	}
	while (quit!=1 && breakout!=1 && !(orbit.decayed));
}

void pattern_prepare(char *string)
{
	int length = strlen(string);

	//trim whitespaces from end
	for (int i=length-1; i >= 0; i--) {
		if (string[i] == ' ') {
			string[i] = '\0';
		} else if (isdigit(string[i]) || isalpha(string[i])) {
			break;
		}
	}

	//lowercase to uppercase
	for (int i=0; i < length; i++) {
		string[i] = toupper(string[i]);
	}
}

void EditWhitelist(struct tle_db *tle_db)
{
	/* Print header */
	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	clear();

	int row = 0;
	mvprintw(row++,0,"                                                                                ");
	mvprintw(row++,0,"  flyby Enable/Disable Satellites                                               ");
	mvprintw(row++,0,"                                                                                ");

	int c;

	WINDOW *my_menu_win;

	int *tle_index = (int*)calloc(tle_db->num_tles, sizeof(int));

	attrset(COLOR_PAIR(3)|A_BOLD);
	if (tle_db->num_tles >= MAX_NUM_SATS)
		mvprintw(LINES-3,46,"Truncated to %d satellites",MAX_NUM_SATS);
	else
		mvprintw(LINES-3,46,"%d satellites",tle_db->num_tles);

	/* Create form for query input */
	FIELD *field[2];
	FORM  *form;

	field[0] = new_field(1, 24, 1, 1, 0, 0);
	field[1] = NULL;

	set_field_back(field[0], A_UNDERLINE);
	field_opts_off(field[0], O_AUTOSKIP);

	form = new_form(field);
	int rows, cols;
	scale_form(form, &rows, &cols);

	int form_win_height = rows + 4;
	WINDOW *form_win = newwin(rows + 4, cols + 4, row+1, 3);
	row += form_win_height;
	keypad(form_win, TRUE);
	wattrset(form_win, COLOR_PAIR(4));
	box(form_win, 0, 0);

	/* Set main window and sub window */
	set_form_win(form, form_win);
	set_form_sub(form, derwin(form_win, rows, cols, 2, 2));

	post_form(form);
	wrefresh(form_win);

	/* Create the window to be associated with the menu */
	int window_width = 35;
	int window_ypos = row;
	my_menu_win = newwin(LINES-window_ypos-1, window_width, window_ypos, 5);

	keypad(my_menu_win, TRUE);
	wattrset(my_menu_win, COLOR_PAIR(4));
	box(my_menu_win, 0, 0);

	/* Print description */
	attrset(COLOR_PAIR(3)|A_BOLD);
	int col = 42;
	row = 5;
	mvprintw( 6,col,"Use upper-case characters to ");
	mvprintw( 7,col,"filter satellites by name.");


	mvprintw( 10,col,"Use cursor keys to move up/down");
	mvprintw( 11,col,"the list and then disable/enable");
	mvprintw( 12,col,"with        .");

	mvprintw( 14,col,"Press  q  to return to menu.");
	mvprintw( 15,col,"Press  a  to toggle all displayed");
	mvprintw( 16,col,"TLES.");
	mvprintw( 17,col,"Press  w  or  q  to wipe query field.");
	mvprintw(5, 6, "Filter TLEs by name:");
	row = 18;

	/* Print keyboard bindings in special format */
	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw( 12,col+6," SPACE ");
	mvprintw( 14,col+6," q ");
	mvprintw( 15,col+6," a ");
	mvprintw( 17,col+6," w ");
	mvprintw( 17,col+13," q ");

	refresh();

	struct filtered_menu menu = {0};
	filtered_menu_from_tle_db(&menu, tle_db, my_menu_win);

	char field_contents[MAX_NUM_CHARS] = {0};

	refresh();
	wrefresh(my_menu_win);
	form_driver(form, REQ_VALIDATION);
	wrefresh(form_win);
	bool run_menu = true;

	while (run_menu) {
		//handle keyboard
		c = wgetch(my_menu_win);
		bool handled = false;

		handled = filtered_menu_handle(&menu, c);

		wrefresh(my_menu_win);

		if (!handled) {
			switch (c) {
				case 'q':
					strncpy(field_contents, field_buffer(field[0], 0), MAX_NUM_CHARS);
					pattern_prepare(field_contents);

					if (strlen(field_contents) > 0) {
						//wipe field if field is non-empty
						c = 'w';
					} else {
						//exit whitelister otherwise
						run_menu = false;
						break;
					}
				case KEY_BACKSPACE:
					form_driver(form, REQ_DEL_PREV);
				default:
					if (isupper(c) || isdigit(c)) {
						form_driver(form, c);
					}
					if (c == 'w') {
						form_driver(form, REQ_CLR_FIELD);
					}

					form_driver(form, REQ_VALIDATION); //update buffer with field contents

					strncpy(field_contents, field_buffer(field[0], 0), MAX_NUM_CHARS);
					pattern_prepare(field_contents);

					filtered_menu_pattern_match(&menu, field_contents);

					wrefresh(form_win);
					break;
			}
		}
	}

	filtered_menu_to_tle_db(&menu, tle_db);
	filtered_menu_free(&menu);

	whitelist_write_to_default(tle_db);

	unpost_form(form);

	free(tle_index);
	free_form(form);

	free_field(field[0]);
}

void MainMenu()
{
	/* Start-up menu.  Your wish is my command. :-) */

	Banner();
	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw( 9,2," P ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw( 9,6," Predict Satellite Passes");

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(11,2," V ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw(11,6," Predict Visible Passes");

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(13,2," S ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw(13,6," Solar Illumination Predictions");

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(15,2," N ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw(15,6," Lunar Pass Predictions");

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(17,2," O ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw(17,6," Solar Pass Predictions");

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(19,2," T ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw(19,6," Single Satellite Tracking Mode");

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(21,2," M ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw(21,6," Multi-Satellite Tracking Mode");


	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw( 9,41," I ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw( 9,45," Program Information");

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(11,41," G ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw(11,45," Edit Ground Station Information");

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(13,41," D ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw(13,45," Display Satellite Orbital Data");

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(15,41," U ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw(15,45," Update Sat Elements From File");

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(17,41," W ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw(17,45," Enabled/disable satellites");

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(21,41," Q ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw(21,45," Exit flyby");

	refresh();

}

void ProgramInfo(const char *qthfile, struct tle_db *tle_db, struct transponder_db *transponder_db, rotctld_info_t *rotctld)
{
	Banner();
	attrset(COLOR_PAIR(3)|A_BOLD);

	printw("\n\n\n\n\n\t\tflyby version : %s\n",FLYBY_VERSION);
	printw("\t\tQTH file        : %s\n", qthfile);
	printw("\t\tTLE file        : ");
	if (tle_db->num_tles > 0) {
		printw("Loaded\n");
	} else {
		printw("Not loaded\n");
	}
	printw("\t\tDatabase file   : ");
	if (transponder_db->loaded) {
		printw("Loaded\n");
	} else {
		printw("Not loaded\n");
	}

	if (rotctld->connected) {
		printw("\t\tAutoTracking    : Enabled\n");
		printw("\t\t - Connected to rotctld: %s:%s\n", rotctld->host, rotctld->port);

		printw("\t\tTracking horizon: %.2f degrees. ", rotctld->tracking_horizon);

		if (rotctld->once_per_second)
			printw("Update every second");

		printw("\n");
	} else
		printw("\t\tAutoTracking    : Not enabled\n");

	refresh();
	attrset(COLOR_PAIR(4)|A_BOLD);
	AnyKey();
}

void NewUser()
{
	Banner();
	attrset(COLOR_PAIR(3)|A_BOLD);

	mvprintw(12,2,"Welcome to flyby!  Since you are a new user to the program, default\n");
	printw("  orbital data and ground station location information was copied into\n");
	printw("  your home directory to get you going.  Please select option [G] from\n");
	printw("  flyby's main menu to edit your ground station information, and update\n");
	printw("  your orbital database using option [U] or [E].  Enjoy the program!  :-)");
	refresh();

	attrset(COLOR_PAIR(4)|A_BOLD);
	AnyKey();
}

void RunFlybyUI(bool new_user, const char *qthfile, predict_observer_t *observer, struct tle_db *tle_db, struct transponder_db *sat_db, rotctld_info_t *rotctld, rigctld_info_t *downlink, rigctld_info_t *uplink)
{
	/* Start ncurses */
	initscr();
	keypad(stdscr, TRUE);
	start_color();
	cbreak();
	noecho();
	scrollok(stdscr,TRUE);
	curs_set(0);

	init_pair(1,COLOR_WHITE,COLOR_BLACK);
	init_pair(2,COLOR_YELLOW,COLOR_BLACK);
	init_pair(3,COLOR_GREEN,COLOR_BLACK);
	init_pair(4,COLOR_CYAN,COLOR_BLACK);
	init_pair(5,COLOR_WHITE,COLOR_RED);
	init_pair(6,COLOR_RED,COLOR_WHITE);
	init_pair(7,COLOR_CYAN,COLOR_RED);

	if (new_user) {
		NewUser();
		QthEdit(qthfile, observer);
	}

	/* Parse TLEs */
	int num_sats = tle_db->num_tles;
	predict_orbital_elements_t **orbital_elements_array = (predict_orbital_elements_t**)malloc(sizeof(predict_orbital_elements_t*)*num_sats);
	for (int i=0; i < num_sats; i++){
		char *tle[2] = {tle_db->tles[i].line1, tle_db->tles[i].line2};
		orbital_elements_array[i] = predict_parse_tle(tle);
	}

	/* Display main menu and handle keyboard input */
	MainMenu();
	int indx = 0;
	char key;
	do {
		key=getch();

		if (key!='T')
			key=tolower(key);

		switch (key) {
			case 'p':
			case 'v':
				Print("","",0);
				PrintVisible("","");
				indx=Select(tle_db, orbital_elements_array);

				if (indx!=-1) {
					Predict(tle_db->tles[indx].name, orbital_elements_array[indx], observer, key);
				}

				MainMenu();
				break;

			case 'n':
				Print("","",0);
				PredictSunMoon(PREDICT_MOON, observer);
				MainMenu();
				break;

			case 'o':
				Print("","",0);
				PredictSunMoon(PREDICT_SUN, observer);
				MainMenu();
				break;

			case 'u':
				AutoUpdate("", tle_db, orbital_elements_array);
				MainMenu();
				break;

			case 'd':
				ShowOrbitData(tle_db, orbital_elements_array);
				MainMenu();
				break;

			case 'g':
				QthEdit(qthfile, observer);
				MainMenu();
				break;

			case 't':
			case 'T':
				indx=Select(tle_db, orbital_elements_array);

				if (indx!=-1) {
					SingleTrack(indx, orbital_elements_array, observer, sat_db, tle_db, rotctld, downlink, uplink);
				}

				MainMenu();
				break;

			case 'm':
			case 'l':

				MultiTrack(observer, orbital_elements_array, tle_db, key, 'k');
				MainMenu();
				break;

			case 'i':
				ProgramInfo(qthfile, tle_db, sat_db, rotctld);
				MainMenu();
				break;

			case 's':
				indx=Select(tle_db, orbital_elements_array);

				if (indx!=-1) {
					Print("","",0);

					Illumination(tle_db->tles[indx].name, orbital_elements_array[indx]);
				}

				MainMenu();
				break;

			case 'w':
			case 'W':
				EditWhitelist(tle_db);
				MainMenu();
				break;
		}

	} while (key!='q' && key!=27);

	curs_set(1);
	bkgdset(COLOR_PAIR(1));
	clear();
	refresh();
	endwin();

	for (int i=0; i < num_sats; i++){
		predict_destroy_orbital_elements(orbital_elements_array[i]);
	}
	free(orbital_elements_array);
}
