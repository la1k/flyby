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
#include "flyby_defs.h"

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

#define maxsats		250
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define xkmper		6.378137E3		/* WGS 84 Earth radius km */
#define s		1.012229
#define MAX_NUM_CHARS 80
#define halfdelaytime	5
#define	km2mi		0.621371		/* km to miles */

char *flybypath={"/etc/flyby"}, soundcard=0;

struct	{  char line1[70];
	   char line2[70];
	   char name[25];
	   long catnum;
	   long setnum;
	   char designator[10];
	   int year;
	   double refepoch;
	   double incl;
	   double raan;
	   double eccn;
	   double argper;
	   double meanan;
	   double meanmo;
	   double drag;
	   double nddot6;
	   double bstar;
	   long orbitnum;
	}  sat[maxsats];

struct	{  char callsign[17];
	   double stnlat;
	   double stnlong;
	   int stnalt;
	   int tzoffset;
	}  qth;

struct sat_db_entry {
	   char name[25];
	   long catnum;
	   char squintflag;
	   double alat;
	   double alon;
	   unsigned char transponders;
	   char transponder_name[10][80];
	   double uplink_start[10];
	   double uplink_end[10];
	   double downlink_start[10];
	   double downlink_end[10];
	   unsigned char dayofweek[10];
	   int phase_start[10];
	   int phase_end[10];
	};

struct sat_db_entry sat_db[maxsats];


/* Global variables for sharing data among functions... */


char	qthfile[50], tlefile[50], dbfile[50], temp[80], output[25],
	rotctld_host[256], rotctld_port[6]="4533\0\0",
	uplink_host[256], uplink_port[6]="4532\0\0", uplink_vfo[30],
	downlink_host[256], downlink_port[6]="4532\0\0", downlink_vfo[30],
	resave=0, reload_tle=0, netport[8],
	once_per_second=0, ephem[5], sat_sun_status, findsun,
	calc_squint, database=0, io_lat='N', io_lon='E', maidenstr[9];

int	rotctld_socket, uplink_socket, downlink_socket, totalsats=0;

unsigned char val[256];

char	tracking_mode[30];

unsigned short portbase=0;

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
	/* This function quits ncurses, resets and "beeps"
	   the terminal, and displays an error message (string)
	   when we need to bail out of the program in a hurry. */

	beep();
	curs_set(1);
	bkgdset(COLOR_PAIR(1));
	clear();
	refresh();
	endwin();
	fprintf(stderr,"*** flyby: %s!\n",string);
}

int sock_readline(int sockd, char *message, size_t bufsize)
{
	int len=0, pos=0;
	char c='\0';

	if (message!=NULL)
		message[bufsize-1]='\0';
	do
	{
		if ((len=recv(sockd, &c, 1, 0)) < 0)
			return len;
		if (message!=NULL)
		{
			message[pos]=c;
			message[pos+1]='\0';
		}
		pos+=len;
	} while (c!='\n' && pos<bufsize-2);

	return pos;
}

void TrackDataNet(int sockd, double elevation, double azimuth)
{
	char message[30];

	/* If positions are sent too often, rotctld will queue
	   them and the antenna will lag behind. Therefore, we wait
	   for confirmation from last command before sending the
	   next. */
	sock_readline(sockd, message, sizeof(message));

	sprintf(message, "P %.2f %.2f\n", azimuth, elevation);
	int len = strlen(message);
	if (send(sockd, message, len, 0) != len)
	{
		bailout("Failed to send to rotctld");
		exit(-1);
	}
}

void FreqDataNet(int sockd, char *vfo, double freq)
{
	char message[256];
	int len;

	/* If frequencies is sent too often, rigctld will queue
	   them and the radio will lag behind. Therefore, we wait
	   for confirmation from last command before sending the
	   next. */
	sock_readline(sockd, message, sizeof(message));

	if (vfo)
	{
		sprintf(message, "V %s\n", vfo);
		len = strlen(message);
    usleep(100); // hack: avoid VFO selection racing
		if (send(sockd, message, len, 0) != len)
		{
			bailout("Failed to send to rigctld");
			exit(-1);
		}
	}

	sock_readline(sockd, message, sizeof(message));

	sprintf(message, "F %.0f\n", freq*1000000);
	len = strlen(message);
	if (send(sockd, message, len, 0) != len)
	{
		bailout("Failed to send to rigctld");
		exit(-1);
	}
}

double ReadFreqDataNet(int sockd, char *vfo)
{
	char message[256];
	int len;
	double freq;

	/* Read pending return message */
	sock_readline(sockd, message, sizeof(message));

	if (vfo)
	{
		sprintf(message, "V %s\n", vfo);
		len = strlen(message);
    usleep(100); // hack: avoid VFO selection racing
		if (send(sockd, message, len, 0) != len)
		{
			bailout("Failed to send to rigctld");
			exit(-1);
		}
	}

	sock_readline(sockd, message, sizeof(message));

	sprintf(message, "f\n");
	len = strlen(message);
	if (send(sockd, message, len, 0) != len)
	{
		bailout("Failed to send to rigctld");
		exit(-1);
	}

	sock_readline(sockd, message, sizeof(message));
	freq=atof(message)/1.0e6;

	sprintf(message, "f\n");
	len = strlen(message);
	if (send(sockd, message, len, 0) != len)
	{
		bailout("Failed to send to rigctld");
		exit(-1);
	}

	return freq;
}

int passivesock(char *service, char *protocol, int qlen)
{
	/* This function opens the socket port */

	struct servent *pse;
	struct protoent *ppe;
	struct sockaddr_in sin;
	int sd, type;

	memset((char *)&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family=AF_INET;
	sin.sin_addr.s_addr=INADDR_ANY;

	if ((pse=getservbyname(service,protocol)))
		sin.sin_port=htons(ntohs((unsigned short)pse->s_port)+portbase);

	else if ((sin.sin_port=htons((unsigned short)atoi(service)))==0) {
		bailout("Can't get service");
		exit(-1);
	}

	if ((ppe=getprotobyname(protocol))==0) {
		bailout("Can't get protocol");
		exit(-1);
	}

	if (strcmp(protocol,"udp")==0)
		type=SOCK_DGRAM;
	else
		type=SOCK_STREAM;

	sd=socket(PF_INET,type, ppe->p_proto);

	if (sd<0) {
		bailout("Can't open socket");
		exit(-1);
	}

	if (bind(sd,(struct sockaddr *)&sin,sizeof(sin))<0) {
		bailout("Can't bind");
		exit(-1);
	}

	if ((type=SOCK_STREAM && listen(s,qlen))<0) {
		bailout("Listen fail");
		exit(-1);
	}

	return sd;
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

char *SubString(string,start,end)
char *string, start, end;
{
	/* This function returns a substring based on the starting
	   and ending positions provided.  It is used heavily in
	   the AutoUpdate function when parsing 2-line element data. */

	unsigned x, y;

	if (end>=start) {
		for (x=start, y=0; x<=end && string[x]!=0; x++)
			if (string[x]!=' ') {
				temp[y]=string[x];
				y++;
			}

		temp[y]=0;
		return temp;
	} else
		return NULL;
}

void CopyString(source, destination, start, end)
char *source, *destination, start, end;
{
	/* This function copies elements of the string "source"
	   bounded by "start" and "end" into the string "destination". */

	unsigned j, k=0;

	for (j=start; j<=end; j++)
		if (source[k]!=0) {
			destination[j]=source[k];
			k++;
		}
}

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
	/* This function scans line 1 and line 2 of a NASA 2-Line element
	   set and returns a 1 if the element set appears to be valid or
	   a 0 if it does not.  If the data survives this torture test,
	   it's a pretty safe bet we're looking at a valid 2-line
	   element set and not just some random text that might pass
	   as orbital data based on a simple checksum calculation alone. */

	int x;
	unsigned sum1, sum2;

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

void InternalUpdate(x)
int x;
{
	/* Updates data in TLE structure based on
	   line1 and line2 stored in structure. */

	double tempnum;

	strncpy(sat[x].designator,SubString(sat[x].line1,9,16),8);
	sat[x].designator[9]=0;
	sat[x].catnum=atol(SubString(sat[x].line1,2,6));
	sat[x].year=atoi(SubString(sat[x].line1,18,19));
	sat[x].refepoch=atof(SubString(sat[x].line1,20,31));
	tempnum=1.0e-5*atof(SubString(sat[x].line1,44,49));
	sat[x].nddot6=tempnum/pow(10.0,(sat[x].line1[51]-'0'));
	tempnum=1.0e-5*atof(SubString(sat[x].line1,53,58));
	sat[x].bstar=tempnum/pow(10.0,(sat[x].line1[60]-'0'));
	sat[x].setnum=atol(SubString(sat[x].line1,64,67));
	sat[x].incl=atof(SubString(sat[x].line2,8,15));
	sat[x].raan=atof(SubString(sat[x].line2,17,24));
	sat[x].eccn=1.0e-07*atof(SubString(sat[x].line2,26,32));
	sat[x].argper=atof(SubString(sat[x].line2,34,41));
	sat[x].meanan=atof(SubString(sat[x].line2,43,50));
	sat[x].meanmo=atof(SubString(sat[x].line2,52,62));
	sat[x].drag=atof(SubString(sat[x].line1,33,42));
	sat[x].orbitnum=atof(SubString(sat[x].line2,63,67));
}

char *noradEvalue(value)
double value;
{
	/* Converts numeric values to E notation used in NORAD TLEs */

	char string[15];

	sprintf(string,"%11.4e",value*10.0);

	output[0]=string[0];
	output[1]=string[1];
	output[2]=string[3];
	output[3]=string[4];
	output[4]=string[5];
	output[5]=string[6];
	output[6]='-';
	output[7]=string[10];
	output[8]=0;

	return output;
}

void Data2TLE(x)
int x;
{
	/* This function converts orbital data held in the numeric
	   portion of the sat tle structure to ASCII TLE format,
	   and places the result in ASCII portion of the structure. */

	int i;
	char string[15], line1[70], line2[70];
	unsigned sum;

	/* Fill lines with blanks */

	for (i=0; i<70; line1[i]=32, line2[i]=32, i++);

	line1[69]=0;
	line2[69]=0;

	/* Insert static characters */

	line1[0]='1';
	line1[7]='U'; /* Unclassified */
	line2[0]='2';

	line1[62]='0'; /* For publically released TLEs */

	/* Insert orbital data */

	sprintf(string,"%05ld",sat[x].catnum);
	CopyString(string,line1,2,6);
	CopyString(string,line2,2,6);

	CopyString(sat[x].designator,line1,9,16);

	sprintf(string,"%02d",sat[x].year);
	CopyString(string,line1,18,19);

	sprintf(string,"%12.8f",sat[x].refepoch);
	CopyString(string,line1,20,32);

	sprintf(string,"%.9f",fabs(sat[x].drag));

	CopyString(string,line1,33,42);

	if (sat[x].drag<0.0)
		line1[33]='-';
	else
		line1[33]=32;

	CopyString(noradEvalue(sat[x].nddot6),line1,44,51);
	CopyString(noradEvalue(sat[x].bstar),line1,53,60);

	sprintf(string,"%4lu",sat[x].setnum);
	CopyString(string,line1,64,67);

	sprintf(string,"%9.4f",sat[x].incl);
	CopyString(string,line2,7,15);

	sprintf(string,"%9.4f",sat[x].raan);
	CopyString(string,line2,16,24);

	sprintf(string,"%13.12f",sat[x].eccn);

	/* Erase eccentricity's decimal point */

	for (i=2; i<=9; string[i-2]=string[i], i++);

	CopyString(string,line2,26,32);

	sprintf(string,"%9.4f",sat[x].argper);
	CopyString(string,line2,33,41);

	sprintf(string,"%9.5f",sat[x].meanan);
	CopyString(string,line2,43,50);

	sprintf(string,"%12.9f",sat[x].meanmo);
	CopyString(string,line2,52,62);

	sprintf(string,"%5lu",sat[x].orbitnum);
	CopyString(string,line2,63,67);

	/* Compute and insert checksum for line 1 and line 2 */

	for (i=0, sum=0; i<=67; sum+=val[(int)line1[i]], i++);

	line1[68]=(sum%10)+'0';

	for (i=0, sum=0; i<=67; sum+=val[(int)line2[i]], i++);

	line2[68]=(sum%10)+'0';

	line1[69]=0;
	line2[69]=0;

	strcpy(sat[x].line1,line1);
	strcpy(sat[x].line2,line2);
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

char ReadDataFiles()
{
	/* This function reads "flyby.qth" and "flyby.tle"
	   files into memory.  Return values are as follows:

	   0 : No files were loaded
	   1 : Only the qth file was loaded
	   2 : Only the tle file was loaded
	   3 : Both files were loaded successfully */

	FILE *fd;
	long catnum;
	unsigned char dayofweek;
	int x=0, y, entry=0, max_entries=10, transponders=0;
	char flag=0, match, name[80], line1[80], line2[80];

	fd=fopen(qthfile,"r");

	if (fd!=NULL) {
		fgets(qth.callsign,16,fd);
		qth.callsign[strlen(qth.callsign)-1]=0;
		fscanf(fd,"%lf", &qth.stnlat);
		fscanf(fd,"%lf", &qth.stnlong);
		fscanf(fd,"%d", &qth.stnalt);
		fscanf(fd,"%d", &qth.tzoffset);
		fclose(fd);

		flag=1;
	}

	fd=fopen(tlefile,"r");

	if (fd!=NULL) {
		while (x<maxsats && feof(fd)==0) {
			/* Initialize variables */

			name[0]=0;
			line1[0]=0;
			line2[0]=0;

			/* Read element set */

			fgets(name,75,fd);
			fgets(line1,75,fd);
			fgets(line2,75,fd);

			if (KepCheck(line1,line2) && (feof(fd)==0)) {
				/* We found a valid TLE! */

				/* Some TLE sources left justify the sat
				   name in a 24-byte field that is padded
				   with blanks.  The following lines cut
				   out the blanks as well as the line feed
				   character read by the fgets() function. */

				y=strlen(name);

				while (name[y]==32 || name[y]==0 || name[y]==10 || name[y]==13 || y==0) {
					name[y]=0;
					y--;
				}

				/* Copy TLE data into the sat data structure */

				strncpy(sat[x].name,name,24);
				strncpy(sat[x].line1,line1,69);
				strncpy(sat[x].line2,line2,69);

				/* Update individual parameters */

				InternalUpdate(x);

				x++;

			}
		}

		flag+=2;
		resave=0;
		fclose(fd);
		totalsats=x;

	}


	/* Load satellite database file */

	fd=fopen(dbfile,"r");

	if (fd!=NULL) {
		database=1;

		fgets(line1,40,fd);

		while (strncmp(line1,"end",3)!=0 && line1[0]!='\n' && feof(fd)==0) {
			/* The first line is the satellite
			   name which is ignored here. */

			fgets(line1,40,fd);
			sscanf(line1,"%ld",&catnum);

			/* Search for match */

				for (y=0, match=0; y<maxsats && match==0; y++) {
					if (catnum==sat[y].catnum)
						match=1;
				}

				if (match) {
					transponders=0;
					entry=0;
					y--;
				}

				fgets(line1,40,fd);

				if (match) {
					if (strncmp(line1,"No",2)!=0) {
						sscanf(line1,"%lf, %lf",&sat_db[y].alat, &sat_db[y].alon);
						sat_db[y].squintflag=1;
					}

					else
						sat_db[y].squintflag=0;
				}

				fgets(line1,80,fd);

				while (strncmp(line1,"end",3)!=0 && line1[0]!='\n' && feof(fd)==0) {
					if (entry<max_entries) {
						if (match) {
							if (strncmp(line1,"No",2)!=0) {
								line1[strlen(line1)-1]=0;
								strcpy(sat_db[y].transponder_name[entry],line1);
							} else
								sat_db[y].transponder_name[entry][0]=0;
						}

						fgets(line1,40,fd);

						if (match)
							sscanf(line1,"%lf, %lf", &sat_db[y].uplink_start[entry], &sat_db[y].uplink_end[entry]);

						fgets(line1,40,fd);

						if (match)
							sscanf(line1,"%lf, %lf", &sat_db[y].downlink_start[entry], &sat_db[y].downlink_end[entry]);

						fgets(line1,40,fd);

						if (match) {
							if (strncmp(line1,"No",2)!=0) {
								dayofweek=(unsigned char)atoi(line1);
								sat_db[y].dayofweek[entry]=dayofweek;
							} else
								sat_db[y].dayofweek[entry]=0;
						}

						fgets(line1,40,fd);

						if (match) {
							if (strncmp(line1,"No",2)!=0)
								sscanf(line1,"%d, %d",&sat_db[y].phase_start[entry], &sat_db[y].phase_end[entry]);
							else {
								sat_db[y].phase_start[entry]=0;
								sat_db[y].phase_end[entry]=0;
							}

							if (sat_db[y].uplink_start[entry]!=0.0 || sat_db[y].downlink_start[entry]!=0.0)
								transponders++;

							entry++;
						}
					}
					fgets(line1,80,fd);
				}
				fgets(line1,80,fd);

				if (match)
					sat_db[y].transponders=transponders;

				entry=0;
				transponders=0;
			}

			fclose(fd);
		}


	return flag;
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

void SaveQTH()
{
	/* This function saves QTH data file normally
	   found under ~/.flyby/flyby.qth */

	FILE *fd;

	fd=fopen(qthfile,"w");

	fprintf(fd,"%s\n",qth.callsign);
	fprintf(fd," %g\n",qth.stnlat);
	fprintf(fd," %g\n",qth.stnlong);
	fprintf(fd," %d\n",qth.stnalt);
	fprintf(fd," %d\n",qth.tzoffset);

	fclose(fd);
}

void SaveTLE()
{
	int x;
	FILE *fd;

	/* Save orbital data to tlefile */

	fd=fopen(tlefile,"w");

	for (x=0; x<totalsats; x++) {
		/* Convert numeric orbital data to ASCII TLE format */

		Data2TLE(x);

		/* Write name, line1, line2 to flyby.tle */

		fprintf(fd,"%s\n", sat[x].name);
		fprintf(fd,"%s\n", sat[x].line1);
		fprintf(fd,"%s\n", sat[x].line2);
	}

	fclose(fd);
}

int AutoUpdate(string)
char *string;
{
	/* This function updates PREDICT's orbital datafile from a NASA
	   2-line element file either through a menu (interactive mode)
	   or via the command line.  string==filename of 2-line element
	   set if this function is invoked via the command line. */

	char line1[80], line2[80], str0[80], str1[80], str2[80],
	     filename[50], saveflag=0, interactive=0;

	float database_epoch=0.0, tle_epoch=0.0, database_year, tle_year;
	int i, success=0, kepcount=0, savecount=0;
	FILE *fd;

	do {
		if (string[0]==0) {
			interactive=1;
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
		} else
			strcpy(filename,string);

		/* Prevent "." and ".." from being used as a
		   filename otherwise strange things happen. */

		if (strlen(filename)==0 || strncmp(filename,".",1)==0 || strncmp(filename,"..",2)==0)
			return 0;

		fd=fopen(filename,"r");

		if (interactive && fd==NULL) {
			bkgdset(COLOR_PAIR(5));
			clear();
			move(12,0);

			for (i=47; i>strlen(filename); i-=2)
				printw(" ");

			printw("*** ERROR: File \"%s\" not found! ***\n",filename);
			beep();
			attrset(COLOR_PAIR(7)|A_BOLD);
			AnyKey();
		}

		if (fd!=NULL) {
			success=1;

			fgets(str0,75,fd);
			fgets(str1,75,fd);
			fgets(str2,75,fd);

			do {
				if (KepCheck(str1,str2)) {
					/* We found a valid TLE!
					   Copy strings str1 and
					   str2 into line1 and line2 */

					strncpy(line1,str1,75);
					strncpy(line2,str2,75);
					kepcount++;

					/* Scan for object number in datafile to see
					   if this is something we're interested in */

					for (i=0; (i<maxsats && sat[i].catnum!=atol(SubString(line1,2,6))); i++);

					if (i!=maxsats) {
						/* We found it!  Check to see if it's more
						   recent than the data we already have. */

						if (sat[i].year<57)
							database_year=365.25*(100.0+(float)sat[i].year);
						else
							database_year=365.25*(float)sat[i].year;

						database_epoch=(float)sat[i].refepoch+database_year;

						tle_year=(float)atof(SubString(line1,18,19));

						if (tle_year<57.0)
							tle_year+=100.0;

						tle_epoch=(float)atof(SubString(line1,20,31))+(tle_year*365.25);

						/* Update only if TLE epoch >= epoch in data file
						   so we don't overwrite current data with older
						   data. */

						if (tle_epoch>=database_epoch) {
							if (saveflag==0) {
								if (interactive) {

									bkgdset(COLOR_PAIR(3)|A_BOLD);
									attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
									clear();

									mvprintw(0,0,"                                                                                ");
									mvprintw(1,0,"  flyby Orbital Data : Updating...                                              ");
									mvprintw(2,0,"                                                                                ");

									attrset(COLOR_PAIR(2));
									refresh();
									move(4,0);
								}
								saveflag=1;
							}

							if (interactive) {
								bkgdset(COLOR_PAIR(3));
								printw(" %-8s ",sat[i].name);
							}

							savecount++;

							/* Copy TLE data into the sat data structure */

							strncpy(sat[i].line1,line1,69);
							strncpy(sat[i].line2,line2,69);
							InternalUpdate(i);
						}
					}

					fgets(str0,75,fd);
					fgets(str1,75,fd);
					fgets(str2,75,fd);
				} else {
					strcpy(str0,str1);
					strcpy(str1,str2);
					fgets(str2,75,fd);
				}

			} while (feof(fd)==0);

			fclose(fd);

			if (interactive) {
				bkgdset(COLOR_PAIR(2));

				if (kepcount==1)
					mvprintw(LINES-3,2,"Only 1 NASA Two Line Element was found.");
				else
					mvprintw(LINES-3,2,"%3u NASA Two Line Elements were read.",kepcount);

				if (saveflag) {
					if (savecount==1)
						mvprintw(LINES-2,2,"Only 1 satellite was updated.");
					else {
						if (savecount==totalsats)
							mvprintw(LINES-2,2,"All satellites were updated!");
						else
							mvprintw(LINES-2,2,"%3u out of %3u satellites were updated.",savecount,totalsats);
					}
				}

				refresh();
			}
		}

		if (interactive) {
			noecho();

			if (strlen(filename) && fd!=NULL) {
				attrset(COLOR_PAIR(4)|A_BOLD);
				AnyKey();
			}
		}

		if (saveflag)
			SaveTLE();
	} while (success==0 && interactive);

	return (saveflag ? 0 : -1);
}

int Select(int num_orbits, predict_orbit_t **orbits)
{
	ITEM **my_items;
	int c;
	MENU *my_menu;
	WINDOW *my_menu_win;
	int n_choices, i, j;

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

	if (num_orbits >= maxsats)
		mvprintw(LINES-3,46,"Truncated to %d satellites",maxsats);
	else
		mvprintw(LINES-3,46,"%d satellites",totalsats);

	/* Create items */
	n_choices = num_orbits;
	my_items = (ITEM **)calloc(n_choices, sizeof(ITEM *));
	for(i = 0; i < n_choices; ++i)
		my_items[i] = new_item(orbits[i]->name, orbits[i]->orbital_elements.designator);

	/* Create menu */
	my_menu = new_menu((ITEM **)my_items);

	/* Set menu option not to show the description */
//	menu_opts_off(my_menu, O_SHOWDESC);

	/* Create the window to be associated with the menu */
	my_menu_win = newwin(LINES-5, 40, 4, 4);
	keypad(my_menu_win, TRUE);

	set_menu_back(my_menu,COLOR_PAIR(1));
	set_menu_fore(my_menu,COLOR_PAIR(5)|A_BOLD);

	wattrset(my_menu_win, COLOR_PAIR(4));

	/* Set main window and sub window */
	set_menu_win(my_menu, my_menu_win);
	set_menu_sub(my_menu, derwin(my_menu_win, LINES-7, 38, 2, 1));
	set_menu_format(my_menu, LINES-9, 1);

	/* Set menu mark to the string " * " */
	set_menu_mark(my_menu, " * ");

	/* Print a border around the main window and print a title */
#if	!defined (__CYGWIN32__)
	box(my_menu_win, 0, 0);
#endif

	/* Post the menu */
	post_menu(my_menu);

	refresh();
	wrefresh(my_menu_win);

	while((c = wgetch(my_menu_win)) != 10) {
		switch(c) {
			case 'q':
				return -1;
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
				break;
		}
		wrefresh(my_menu_win);
	}

	for (i=0, j=0; i<num_orbits; i++)
		if (strcmp(item_name(current_item(my_menu)),orbits[i]->name)==0)
			j = i;

	/* Unpost and free all the memory taken up */
	unpost_menu(my_menu);
	free_menu(my_menu);
	for(i = 0; i < n_choices; ++i)
		free_item(my_items[i]);

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

char *Daynum2String(daynum, stlen, stfmt)
double daynum;
int stlen;
char *stfmt;
{
	/* This function takes the given epoch as a fractional number of
	   days since 31Dec79 00:00:00 GMT and returns the corresponding
	   date as a string. */

	char timestr[26];
	time_t t;
	int x;

	/* Convert daynum to Unix time (seconds since 01-Jan-70) */
	t=(time_t)rint(86400.0*(daynum+3651.0))+((stlen == 8) ? 0 : (qth.tzoffset*3600));

	strftime(timestr, stlen+1, stfmt, gmtime(&t));
	for (x=0; x<=stlen; output[x]=timestr[x], x++);

	return output;
}

double GetStartTime(const char* info_str)
{
	/* This function prompts the user for the time and date
	   the user wishes to begin prediction calculations,
	   and returns the corresponding fractional day number.
	   31Dec79 00:00:00 returns 0.  Default is NOW. */

	int	x, hr, min, sec ,mm=0, dd=0, yy;
	char	good, mon[5], line[30], string[30], bozo_count=0,
		*month[12]= {"Jan", "Feb", "Mar", "Apr", "May",
		"Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	do {
		bkgdset(COLOR_PAIR(2)|A_BOLD);
		clear();

		printw("\n\n\n\t     Starting %s Date and Time for Predictions of ",(qth.tzoffset==0) ? "GMT" : "Local");

		printw("%-15s\n\n", info_str);

		bozo_count++;

		strcpy(string,Daynum2String(predict_to_julian(time(NULL)),20,"%a %d%b%y %H:%M:%S"));

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

int Print(string,mode)
char *string, mode;
{
	/* This function buffers and displays orbital predictions
	   and allows screens to be saved to a disk file. */

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
				if (calc_squint)
					sprintf(head2,"           Date     Time    El   Az  Phase  %s   %s    Range  Squint        ",(io_lat=='N'?"LatN":"LatS"),(io_lon=='W'?"LonW":"LonE"));
				else
					sprintf(head2,"           Date     Time    El   Az  Phase  %s   %s    Range   Orbit        ",(io_lat=='N'?"LatN":"LatS"),(io_lon=='W'?"LonW":"LonE"));
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

int PrintVisible(string)
char *string;
{
	/* This function acts as a filter to display passes that could
	   possibly be visible to the ground station.  It works by first
	   buffering prediction data generated by the Predict() function
	   and then checking it to see if at least a part of the pass
	   is visible.  If it is, then the buffered prediction data
	   is sent to the Print() function so it can be displayed
	   to the user and optionally logged to a file. */

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
						quit=Print(line,'v');
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

void Predict(predict_orbit_t *orbit, predict_observer_t *qth, char mode)
{
	/* This function predicts satellite passes.  It displays
	   output through the Print() function if mode=='p' (show
	   all passes), or through the PrintVisible() function if
	   mode=='v' (show only visible passes). */

	bool should_quit = false;
	bool should_break = false;
	char data_string[MAX_NUM_CHARS];
	char time_string[MAX_NUM_CHARS];

	predict_julian_date_t curr_time = GetStartTime(orbit->name);
	predict_orbit(orbit, curr_time);
	clear();

	if (predict_aos_happens(orbit, qth->latitude) && !predict_is_geostationary(orbit) && !predict_decayed(orbit)) {
		do {
			predict_julian_date_t next_aos = predict_next_aos(qth, orbit, curr_time);
			predict_julian_date_t next_los = predict_next_los(qth, orbit, next_aos);
			curr_time = next_aos;

			struct predict_observation obs;
			predict_orbit(orbit, curr_time);
			predict_observe_orbit(qth, orbit, &obs);
			bool has_printed_last_entry = false;
			do {
				mvprintw(1,60, "%s (%d)", orbit->name, orbit->orbital_elements.satellite_number);

				//get formatted time
				time_t epoch = predict_from_julian(curr_time);
				strftime(time_string, MAX_NUM_CHARS, "%a %d%b%y %H:%M:%S", gmtime(&epoch));

				//modulo 256 phase
				int ma256 = (int)rint(256.0*(orbit->phase/(2*M_PI)));

				//satellite visibility status
				char visibility;
				if (obs.visible) {
					visibility = '+';
				} else if (!(orbit->eclipsed)) {
					visibility = '*';
				} else {
					visibility = ' ';
				}

				//format line of data
				sprintf(data_string,"      %s%4d %4d  %4d  %4d   %4d   %6ld  %6ld %c\n", time_string, (int)(obs.elevation*180.0/M_PI), (int)(obs.azimuth*180.0/M_PI), ma256, (int)(orbit->latitude*180.0/M_PI), (int)(orbit->longitude*180.0/M_PI), (long)(obs.range), orbit->revolutions, visibility);

				//print data to screen
				if (mode=='p') {
					should_quit=Print(data_string,'p');
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

					should_quit=PrintVisible(data_string);
				}

				//calculate results for next timestep
				curr_time += cos((obs.elevation*180/M_PI-1.0)*M_PI/180.0)*sqrt(orbit->altitude)/25000.0; //predict's magic time increment formula
				predict_orbit(orbit, curr_time);
				predict_observe_orbit(qth, orbit, &obs);

				//make sure that the last printed line is at elevation 0 (since that looks nicer)
				if ((obs.elevation < 0) && !has_printed_last_entry) {
					has_printed_last_entry = true;
					curr_time = next_los;

					predict_orbit(orbit, curr_time);
					predict_observe_orbit(qth, orbit, &obs);
				}
			} while (((obs.elevation >= 0) || (curr_time <= next_los)) && !should_quit);

			if (mode=='p') {
				should_quit=Print("\n",'p');
			}

			if (mode=='v') {
				should_quit=PrintVisible("\n");
			}
		} while (!should_quit && !should_break && !predict_decayed(orbit));
	} else {
		//display warning that passes are impossible
		bkgdset(COLOR_PAIR(5)|A_BOLD);
		clear();

		if (!predict_aos_happens(orbit, qth->latitude) || predict_decayed(orbit)) {
			mvprintw(12,5,"*** Passes for %s cannot occur for your ground station! ***\n",orbit->name);
		}

		if (predict_is_geostationary(orbit)) {
			mvprintw(12,3,"*** Orbital predictions cannot be made for a geostationary satellite! ***\n");
		}

		beep();
		bkgdset(COLOR_PAIR(7)|A_BOLD);
		AnyKey();
		refresh();
	}
}

enum celestial_object{PREDICT_SUN, PREDICT_MOON};

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
	struct predict_observation obs;

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
			quit=Print(string,print_mode);
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
			quit=Print(string,print_mode);
			lastel=iel;
		} //will continue until we have elevation 0 at the end of the pass

		quit=Print("\n",'o');
		daynum+=0.4;
		rise=0.0;

	} while (quit==0);
}

char KbEdit(x,y)
int x,y;
{
	/* This function is used when editing QTH
	   and orbital data via the keyboard. */

	char need2save=0, input[25];

	echo();
	move(y-1,x-1);
	wgetnstr(stdscr,input,24);

	if (strlen(input)!=0) {
		need2save=1;  /* Save new data to variables */
		resave=1;     /* Save new data to disk files */
		strncpy(temp,input,24);
	}

	mvprintw(y-1,x-1,"%-25s",temp);

	refresh();
	noecho();

	return need2save;
}

void ShowOrbitData(int num_orbits, predict_orbit_t **orbits)
{
	/* This function permits displays a satellite's orbital
	   data.  The age of the satellite data is also provided. */

	int c, x, namelength, age;
	double an_period, no_period, sma, c1, e2, satepoch;
	char days[5];

	x=Select(num_orbits, orbits);

	while (x!=-1) {
		predict_orbital_elements_t orbital_elements = orbits[x]->orbital_elements;
		if (orbital_elements.mean_motion!=0.0) {
			bkgdset(COLOR_PAIR(2)|A_BOLD);
			clear();
			sma=331.25*exp(log(1440.0/orbital_elements.mean_motion)*(2.0/3.0));
			an_period=1440.0/orbital_elements.mean_motion;
			c1=cos(orbital_elements.inclination*M_PI/180.0);
			e2=1.0-(orbital_elements.eccentricity*orbital_elements.eccentricity);
			no_period=(an_period*360.0)/(360.0+(4.97*pow((xkmper/sma),3.5)*((5.0*c1*c1)-1.0)/(e2*e2))/orbital_elements.mean_motion);
			satepoch=DayNum(1,0,orbital_elements.epoch_year)+orbital_elements.epoch_day;
			age=(int)rint(predict_to_julian(time(NULL))-satepoch);

			if (age==1)
				strcpy(days,"day");
			else
				strcpy(days,"days");

			namelength=strlen(orbits[x]->name);

			printw("\n");

			for (c=41; c>namelength; c-=2)
				printw(" ");

			bkgdset(COLOR_PAIR(3)|A_BOLD);
			attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
			clear();

			mvprintw(0,0,"                                                                                ");
			mvprintw(1,0,"  flyby Orbital Data                                                            ");
			mvprintw(2,0,"                                                                                ");

			mvprintw(1,25,"(%ld) %s", orbital_elements.satellite_number, orbits[x]->name);

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
			mvprintw( 5,25,"%02d %.8f",orbital_elements.epoch_year,orbital_elements.epoch_day);
			mvprintw( 6,25,"%.4f deg",orbital_elements.inclination);
			mvprintw( 7,25,"%.4f deg",orbital_elements.right_ascension);
			mvprintw( 8,25,"%g",orbital_elements.eccentricity);
			mvprintw( 9,25,"%.4f deg",orbital_elements.argument_of_perigee);
			mvprintw(10,25,"%.4f deg",orbital_elements.mean_anomaly);
			mvprintw(11,25,"%.8f rev/day",orbital_elements.mean_motion);
			mvprintw(12,25,"%g rev/day/day",orbital_elements.derivative_mean_motion);
			mvprintw(13,25,"%g rev/day/day/day",orbital_elements.second_derivative_mean_motion);
			mvprintw(14,25,"%g 1/earth radii",orbital_elements.bstar_drag_term);
			mvprintw(15,25,"%.4f km",sma);
			mvprintw(16,25,"%.4f km",sma*(1.0+orbital_elements.eccentricity)-xkmper);
			mvprintw(17,25,"%.4f km",sma*(1.0-orbital_elements.eccentricity)-xkmper);
			mvprintw(18,25,"%.4f mins",an_period);
			mvprintw(19,25,"%.4f mins",no_period);
			mvprintw(20,25,"%ld",orbital_elements.revolutions_at_epoch);
			mvprintw(21,25,"%ld",orbital_elements.element_number);

			attrset(COLOR_PAIR(3)|A_BOLD);
			refresh();
			AnyKey();
		}
		x=Select(num_orbits, orbits);
	 };
}

void QthEdit()
{
	/* This function permits keyboard editing of
	   the ground station's location information. */

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
	mvprintw(15,20,"Timezone Offset   : ");

	attrset(COLOR_PAIR(2)|A_BOLD);
	mvprintw(11,43,"%s",qth.callsign);
	if (io_lat=='N')
		mvprintw(12,43,"%g [DegN]",+qth.stnlat);
	else
		mvprintw(12,43,"%g [DegS]",-qth.stnlat);
	if (io_lon=='W')
		mvprintw(13,43,"%g [DegW]",+qth.stnlong);
	else
		mvprintw(13,43,"%g [DegE]",-qth.stnlong);
	mvprintw(14,43,"%d",qth.stnalt);
	mvprintw(15,43,"%d",qth.tzoffset);

	refresh();

	sprintf(temp,"%s",qth.callsign);
	mvprintw(18,15, " Enter the callsign of your ground station");
	if (KbEdit(44,12))
		strncpy(qth.callsign,temp,16);

	if (io_lat=='N')
		sprintf(temp,"%g [DegN]",+qth.stnlat);
	else
		sprintf(temp,"%g [DegS]",-qth.stnlat);
	if (io_lat=='N')
		mvprintw(18,15," Enter your latitude in degrees NORTH      ");
	else
		mvprintw(18,15," Enter your latitude in degrees SOUTH      ");
	mvprintw(19,15," Decimal (74.2467) or DMS (74 14 48) format allowed");
	if (KbEdit(44,13)) {
		if (io_lat=='N')
			qth.stnlat=+ReadBearing(temp);
		else
			qth.stnlat=-ReadBearing(temp);
	}

	if (io_lon=='W')
		sprintf(temp,"%g [DegW]",+qth.stnlong);
	else
		sprintf(temp,"%g [DegE]",-qth.stnlong);

	if (io_lon=='W')
		mvprintw(18,15," Enter your longitude in degrees WEST      ");
	else
		mvprintw(18,15," Enter your longitude in degrees EAST      ");

	if (KbEdit(44,14)) {
		if (io_lon=='W')
			qth.stnlong=+ReadBearing(temp);
		else
			qth.stnlong=-ReadBearing(temp);
	}
	move(19,15);
	clrtoeol();

	mvprintw(18,15," Enter your altitude above sea level (in meters)      ");
	sprintf(temp,"%d",qth.stnalt);
	if (KbEdit(44,15))
		sscanf(temp,"%d",&qth.stnalt);

	sprintf(temp,"%d",qth.tzoffset);
	mvprintw(18,15," Enter your timezone offset from GMT (hours)          ");
	if (KbEdit(44,16))
		sscanf(temp,"%d",&qth.tzoffset);

	if (resave) {
		SaveQTH();
		resave=0;
	}
}

void SingleTrack(double horizon, predict_orbit_t *orbit, predict_observer_t *qth, struct sat_db_entry sat_db)
{
	/* This function tracks a single satellite in real-time
	   until 'Q' or ESC is pressed.  x represents the index
	   of the satellite being tracked. */

	int	ans, length, xponder=0,
		polarity=0;
	bool	aos_alarm=0;
	double	nextaos=0.0, lostime=0.0, aoslos=0.0,
		downlink=0.0, uplink=0.0, downlink_start=0.0,
		downlink_end=0.0, uplink_start=0.0, uplink_end=0.0;
	bool	downlink_update=true, uplink_update=true, readfreq=false;
	bool once_per_second = true;

	double doppler100, delay;
	double dopp;
	double loss;

	//elevation and azimuth at previous timestep, for checking when to send messages to rotctld
	int prev_elevation = 0;
	int prev_azimuth = 0;
	time_t prev_time = 0;

	char tracking_mode[MAX_NUM_CHARS];
	char ephemeris_string[MAX_NUM_CHARS];
	switch (orbit->ephemeris) {
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

	char time_string[MAX_NUM_CHARS];

	do {
		bool comsat = sat_db.transponders > 0;

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

		bool aos_happens = predict_aos_happens(orbit, qth->latitude);
		bool geostationary = predict_is_geostationary(orbit);

		predict_julian_date_t daynum = predict_to_julian(time(NULL));
		predict_orbit(orbit, daynum);
		bool decayed = predict_decayed(orbit);

		halfdelay(halfdelaytime);
		curs_set(0);
		bkgdset(COLOR_PAIR(3));
		clear();

		attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
		mvprintw(0,0,"                                                                                ");
		mvprintw(1,0,"  flyby Tracking:                                                               ");
		mvprintw(2,0,"                                                                                ");
		mvprintw(1,21,"%-24s (%d)", sat_db.name, orbit->orbital_elements.satellite_number);

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
			if (downlink_socket!=-1 && readfreq)
				downlink=ReadFreqDataNet(downlink_socket,downlink_vfo)/(1+1.0e-08*doppler100);
			if (uplink_socket!=-1 && readfreq)
				uplink=ReadFreqDataNet(uplink_socket,uplink_vfo)/(1-1.0e-08*doppler100);


			//predict and observe satellite orbit
			time_t epoch = time(NULL);
			daynum = predict_to_julian(epoch);
			predict_orbit(orbit, daynum);
			struct predict_observation obs;
			predict_observe_orbit(qth, orbit, &obs);
			double sat_vel = sqrt(pow(orbit->velocity[0], 2.0) + pow(orbit->velocity[1], 2.0) + pow(orbit->velocity[2], 2.0));
			double squint = predict_squint_angle(qth, orbit, sat_db.alon, sat_db.alat);

			//display current time
			attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
			strftime(time_string, MAX_NUM_CHARS, "%a %d%b%y %j.%H:%M:%S", gmtime(&epoch));
			mvprintw(1,54,"%s",time_string);

			attrset(COLOR_PAIR(4)|A_BOLD);
			mvprintw(5,8,"N");
			mvprintw(6,8,"E");

			//display satellite data
			attrset(COLOR_PAIR(2)|A_BOLD);
			mvprintw(5,1,"%-6.2f",orbit->latitude*180.0/M_PI);

			attrset(COLOR_PAIR(2)|A_BOLD);
			mvprintw(5,55,"%0.f ",orbit->altitude*km2mi);
			mvprintw(6,55,"%0.f ",orbit->altitude);
			mvprintw(5,68,"%-5.0f",obs.range*km2mi);
			mvprintw(6,68,"%-5.0f",obs.range);
			mvprintw(6,1,"%-7.2f",orbit->longitude*180.0/M_PI);
			mvprintw(5,15,"%-7.2f",obs.azimuth*180.0/M_PI);
			mvprintw(6,14,"%+-6.2f",obs.elevation*180.0/M_PI);
			mvprintw(5,29,"%0.f ",(3600.0*sat_vel)*km2mi);
			mvprintw(6,29,"%0.f ",3600.0*sat_vel);
			mvprintw(18,3,"%+6.2f deg",orbit->eclipse_depth*180.0/M_PI);
			mvprintw(18,20,"%5.1f",256.0*(orbit->phase/(2*M_PI)));
			mvprintw(18,37,"%s",ephemeris_string);
			mvprintw(18,52,"%+6.2f",squint);
			mvprintw(5,42,"%0.f ",orbit->footprint*km2mi);
			mvprintw(6,42,"%0.f ",orbit->footprint);

			attrset(COLOR_PAIR(1)|A_BOLD);
			mvprintw(20,1,"Orbit Number: %ld", orbit->revolutions);

			mvprintw(22,1,"Spacecraft is currently ");
			if (obs.visible) {
				mvprintw(22,25,"visible    ");
			} else if (!(orbit->eclipsed)) {
				mvprintw(22,25,"in sunlight");
			} else {
				mvprintw(22,25,"in eclipse ");
			}

			//display satellite AOS/LOS information
			if (geostationary && (obs.elevation>=0.0)) {
				mvprintw(21,1,"Satellite orbit is geostationary");
				aoslos=-3651.0;
			} else if ((obs.elevation>=0.0) && !geostationary && !decayed && daynum>lostime) {
				lostime = predict_next_los(qth, orbit, daynum);
				time_t epoch = predict_from_julian(lostime);
				strftime(time_string, MAX_NUM_CHARS, "%a %d%b%y %j.%H:%M:%S", gmtime(&epoch));
				mvprintw(21,1,"LOS at: %s %s  ",time_string, "GMT");
				aoslos=lostime;
			} else if (obs.elevation<0.0 && !geostationary && !decayed && aos_happens && daynum>aoslos) {
				daynum+=0.003;  /* Move ahead slightly... */
				nextaos=predict_next_aos(qth, orbit, daynum);
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
						if (downlink_socket!=-1 && downlink_update)
							FreqDataNet(downlink_socket,downlink_vfo,downlink+dopp);
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
						if (uplink_socket!=-1 && uplink_update)
							FreqDataNet(uplink_socket,uplink_vfo,uplink-dopp);
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
			if (rotctld_socket!=-1) {
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
					if (rotctld_socket!=-1) TrackDataNet(rotctld_socket, obs.elevation*180.0/M_PI, obs.azimuth*180.0/M_PI);
					prev_elevation = elevation;
					prev_azimuth = azimuth;
					prev_time = curr_time;
				}
			}

			/* Get input from keyboard */

			ans=getch();

			if (comsat) {
				if (ans==' ' && sat_db.transponders>1) {
					xponder++;

					if (xponder>=sat_db.transponders)
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

				length=strlen(sat_db.transponder_name[xponder])/2;
	      mvprintw(10,0,"                                                                                ");
				mvprintw(10,40-length,"%s",sat_db.transponder_name[xponder]);

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
					if (downlink_socket!=-1)
						downlink=ReadFreqDataNet(downlink_socket,downlink_vfo)/(1+1.0e-08*doppler100);
					if (uplink_socket!=-1)
						uplink=ReadFreqDataNet(uplink_socket,uplink_vfo)/(1-1.0e-08*doppler100);
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
          if (downlink_socket!=-1 && uplink_socket!=-1)
          {
            char tmp_vfo[30];
            strncpy(tmp_vfo,downlink_vfo,sizeof(tmp_vfo));
            strncpy(downlink_vfo,uplink_vfo,sizeof(downlink_vfo));
            strncpy(uplink_vfo,tmp_vfo,sizeof(uplink_vfo));
          }
        }
			}

			refresh();

			halfdelay(halfdelaytime);

		} while (ans!='q' && ans!='Q' && ans!=27 &&
		 	ans!='+' && ans!='-' &&
		 	ans!=KEY_LEFT && ans!=KEY_RIGHT);

	} while (ans!='q' && ans!=17);

	cbreak();
	sprintf(tracking_mode, "NONE\n%c",0);
}

void MultiColours(scrk, scel)
double scrk, scel;
{
	if (scrk < 8000)
		if (scrk < 4000)
			if (scrk < 2000)
				if (scrk < 1000)
					if (scel > 10)
						attrset(COLOR_PAIR(6)|A_REVERSE); /* red */
					else
						attrset(COLOR_PAIR(3)|A_REVERSE); /* yellow */
				else
					if (scel > 20)
						attrset(COLOR_PAIR(3)|A_REVERSE); /* yellow */
					else
						attrset(COLOR_PAIR(4)|A_REVERSE); /* cyan */
			else
				if (scel > 40)
					attrset(COLOR_PAIR(4)|A_REVERSE); /* cyan */
				else
					attrset(COLOR_PAIR(1)|A_REVERSE); /* white */
		else
			attrset(COLOR_PAIR(1)|A_REVERSE); /* white */
	else
		attrset(COLOR_PAIR(2)|A_REVERSE); /* reverse */
}

NCURSES_ATTR_T MultiColours_retattr(scrk, scel)
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

void MultiTrack(predict_observer_t *qth, int num_orbits, predict_orbit_t **orbits, char multitype, char disttype)
{
	/* This function tracks all satellites in the program's
	   database simultaneously until 'Q' or ESC is pressed.
	   Satellites in range are HIGHLIGHTED.  Coordinates
	   for the Sun and Moon are also displayed. */

	int 		*satindex = (int*)malloc(sizeof(int)*num_orbits);

	double		*aos = (double*)malloc(sizeof(double)*num_orbits);
	double		*los = (double*)malloc(sizeof(double)*num_orbits);
	char ans = '\0';

	struct predict_observation *observations = (struct predict_observation*)malloc(sizeof(struct predict_observation)*num_orbits);
	NCURSES_ATTR_T *attributes = (NCURSES_ATTR_T*)malloc(sizeof(NCURSES_ATTR_T)*num_orbits);
	char **string_lines = (char**)malloc(sizeof(char*)*num_orbits);
	for (int i=0; i < num_orbits; i++) {
		string_lines[i] = (char*)malloc(sizeof(char)*MAX_NUM_CHARS);
	}

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
	getMaidenHead(qth->latitude*180.0/M_PI, -qth->longitude*180.0/M_PI, maidenstr);
	mvprintw(7,70,"%9s",maidenstr);

	predict_julian_date_t daynum = predict_to_julian(time(NULL));

	for (int x=0; x < num_orbits; x++) {
		predict_orbit(orbits[x], daynum);
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
			predict_orbit_t *orbit = orbits[i];

			struct predict_observation obs;
			predict_orbit(orbit, daynum);
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
			bool can_predict = !predict_is_geostationary(orbits[i]) && predict_aos_happens(orbits[i], qth->latitude) && !predict_decayed(orbits[i]);
			char aos_los[MAX_NUM_CHARS] = {0};
			if (obs.elevation >= 0) {
				//different colours according to range and elevation
				attributes[i] = MultiColours_retattr(obs.range, obs.elevation*180/M_PI);

				if (predict_is_geostationary(orbit)){
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
				los[i]= predict_next_los(qth, orbit, daynum);
			}

			if (can_predict && (daynum > aos[i])) {
				if (obs.elevation < 0) {
					aos[i] = predict_next_aos(qth, orbit, daynum);
				}
			}

			//set string to display
			char disp_string[MAX_NUM_CHARS];
			sprintf(disp_string, " %-13s%5.1f  %5.1f %8s  %6.0f %6.0f %c %c %12s ", Abbreviate(orbit->name, 12), obs.azimuth*180.0/M_PI, obs.elevation*180.0/M_PI, abs_pos_string, orbit->altitude, obs.range, sunstat, rangestat, aos_los);

			//overwrite everything if orbit was decayed
			if (predict_decayed(orbit)) {
				attributes[i] = COLOR_PAIR(2);
				sprintf(disp_string, " %-10s   ----------------       Decayed        ---------------", Abbreviate(orbit->name,9));
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
			bool will_be_visible = !predict_decayed(orbits[i]) && predict_aos_happens(orbits[i], qth->latitude) && !predict_is_geostationary(orbits[i]);
			if ((observations[i].elevation <= 0) && will_be_visible) {
				satindex[below_horizon_counter + above_horizon_counter] = i;
				below_horizon_counter++;
			} 
		}
		
		//satellites that will never be visible, with decayed orbits last
		int nevervisible_counter = 0;
		int decayed_counter = 0;
		for (int i=0; i < num_orbits; i++){
			bool never_visible = !predict_aos_happens(orbits[i], qth->latitude) || predict_is_geostationary(orbits[i]);
			if ((observations[i].elevation <= 0) && never_visible && !predict_decayed(orbits[i])) {
				satindex[below_horizon_counter + above_horizon_counter + nevervisible_counter] = i;
				nevervisible_counter++;
			} else if (predict_decayed(orbits[i])) {
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
		daynum=predict_to_julian(time(NULL));
		mvprintw(1,54,"%s",Daynum2String(daynum,24,"%a %d%b%y %j.%H:%M:%S"));
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

		refresh();
		halfdelay(halfdelaytime);  // Increase if CPU load is too high
		ans=tolower(getch());

		if (ans=='m') multitype='m';
		if (ans=='l') multitype='l';
		if (ans=='k') disttype ='k';
		if (ans=='i') disttype ='i';

		// If we receive a RELOAD_TLE command through the
		//   socket connection, or an 'r' through the keyboard,
		//   reload the TLE file.  

		if (reload_tle || ans=='r') {
			ReadDataFiles();
			reload_tle=0;
		}
	} while (ans!='q' && ans!=27);

	cbreak();
	sprintf(tracking_mode, "NONE\n%c",0);

	free(satindex);
	free(aos);
	free(los);
	free(observations);
	free(attributes);
	for (int i=0; i < num_orbits; i++) {
		free(string_lines[i]);
	}
	free(string_lines);
}

void Illumination(predict_orbit_t *orbit)
{
	double startday, oneminute, sunpercent;
	int eclipses, minutes, quit, breakout=0, count;
	char string1[MAX_NUM_CHARS], string[MAX_NUM_CHARS], datestring[MAX_NUM_CHARS];

	oneminute=1.0/(24.0*60.0);

	predict_julian_date_t daynum = floor(GetStartTime(0));
	startday=daynum;
	count=0;

	curs_set(0);
	clear();

	const int NUM_MINUTES = 1440;


	do {
		attrset(COLOR_PAIR(4));
		mvprintw(LINES - 2,6,"                 Calculating... Press [ESC] To Quit");
		refresh();

		count++;
		daynum=startday;

		mvprintw(1,60, "%s (%d)", orbit->name, orbit->orbital_elements.satellite_number);

		for (minutes=0, eclipses=0; minutes<NUM_MINUTES; minutes++) {
			predict_orbit(orbit, daynum);

			if (orbit->eclipsed) {
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
			predict_orbit(orbit, daynum);

			if (orbit->eclipsed) {
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
		quit=Print(string,'s');

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
	while (quit!=1 && breakout!=1 && !predict_decayed(orbit));
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
	mvprintw(21,41," Q ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw(21,45," Exit flyby");

	refresh();

}

void ProgramInfo(double horizon)
{
	Banner();
	attrset(COLOR_PAIR(3)|A_BOLD);

	printw("\n\n\n\n\n\t\tflyby version : %s\n",FLYBY_VERSION);
	printw("\t\tQTH file loaded : %s\n",qthfile);
	printw("\t\tTLE file loaded : %s\n",tlefile);
	printw("\t\tDatabase file   : ");

	if (database)
		printw("Loaded\n");
	else
		printw("Not loaded\n");

	if (rotctld_socket!=-1) {
		printw("\t\tAutoTracking    : Enabled\n");
		if (rotctld_socket!=-1) printw("\t\t - Connected to rotctld: %s:%s\n", rotctld_host, rotctld_port);

		printw("\t\tTracking horizon: %.2f degrees. ", horizon);

		if (once_per_second)
			printw("Update every second");

		printw("\n");
	} else
		printw("\t\tAutoTracking    : Not enabled\n");

	printw("\t\tRunning Mode    : ");

	printw("Standalone\n");

	refresh();
	attrset(COLOR_PAIR(4)|A_BOLD);
	AnyKey();
}

void NewUser()
{
#if !defined (__CYGWIN32__)
	int *mkdir();
#endif

	Banner();
	attrset(COLOR_PAIR(3)|A_BOLD);

	mvprintw(12,2,"Welcome to flyby!  Since you are a new user to the program, default\n");
	printw("  orbital data and ground station location information was copied into\n");
	printw("  your home directory to get you going.  Please select option [G] from\n");
	printw("  flyby's main menu to edit your ground station information, and update\n");
	printw("  your orbital database using option [U] or [E].  Enjoy the program!  :-)");
	refresh();

	/* Make "~/.flyby" subdirectory */

	sprintf(temp,"%s/.flyby",getenv("HOME"));
	mkdir(temp,0777);

	/* Copy default files into ~/.flyby directory */

	sprintf(temp,"%sdefault/flyby.tle",flybypath);

	CopyFile(temp,tlefile);

	sprintf(temp,"%sdefault/flyby.db",flybypath);

	CopyFile(temp,dbfile);

	sprintf(temp,"%sdefault/flyby.qth",flybypath);

	CopyFile(temp,qthfile);

	attrset(COLOR_PAIR(4)|A_BOLD);
	AnyKey();
}

double GetDayNum ( struct timeval *tv )
{
  /* used by PredictAt */
  return ( ( ( (double) (tv->tv_sec) - 0.000001 * ( (double) (tv->tv_usec) ) ) / 86400.0) - 3651.0 );
}

int main(argc,argv)
char argc, *argv[];
{
	int x, y, z, key=0;
	char updatefile[80],
	     outputfile[42],
	     tle_cli[50], qth_cli[50], interactive=0;
	char *env=NULL;
	FILE *db;
	struct addrinfo hints, *servinfo, *servinfop;

	/* Set up translation table for computing TLE checksums */

	for (x=0; x<=255; val[x]=0, x++);
	for (x='0'; x<='9'; val[x]=x-'0', x++);

	val['-']=1;

	updatefile[0]=0;
	outputfile[0]=0;
	temp[0]=0;
	tle_cli[0]=0;
	qth_cli[0]=0;
	dbfile[0]=0;
	netport[0]=0;
	once_per_second=0;

	y=argc-1;
	rotctld_socket=-1;
	uplink_socket=-1;
	downlink_socket=-1;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	double horizon = 0;

	for (x=1; x<=y; x++) {
		if (strcmp(argv[x],"-u")==0) {
			z=x+1;
			while (z<=y && argv[z][0] && argv[z][0]!='-') {
				if ((strlen(updatefile)+strlen(argv[z]))<75) {
					strncat(updatefile,argv[z],75);
					strcat(updatefile,"\n");
					z++;
				}
			}
			z--;
		}

		if (strcmp(argv[x],"-t")==0) {
			z=x+1;
			if (z<=y && argv[z][0] && argv[z][0]!='-')
				strncpy(tle_cli,argv[z],48);
		}

		if (strcmp(argv[x],"-q")==0) {
			z=x+1;
			if (z<=y && argv[z][0] && argv[z][0]!='-')
				strncpy(qth_cli,argv[z],48);
		}

		if (strcmp(argv[x],"-a")==0)
		{
			z=x+1;
			if (z<=y && argv[z][0] && argv[z][0]!='-')
				strncpy(rotctld_host,argv[z],sizeof(rotctld_host)-1);
			rotctld_host[sizeof(rotctld_host)-1] = 0;
		}

		if (strcmp(argv[x],"-a1")==0)
		{
			z=x+1;
			if (z<=y && argv[z][0] && argv[z][0]!='-')
				strncpy(rotctld_host,argv[z],sizeof(rotctld_host)-1);
			rotctld_host[sizeof(rotctld_host)-1] = 0;
			once_per_second=1;
		}

		if (strcmp(argv[x],"-ap")==0)
		{
			z=x+1;
			if (z<=y && argv[z][0] && argv[z][0]!='-')
				strncpy(rotctld_port,argv[z],sizeof(rotctld_port)-1);
			rotctld_port[sizeof(rotctld_port)-1] = 0;
		}

		if (strcmp(argv[x],"-h")==0)
		{
			z=x+1;
			if (z<=y && argv[z][0])
				horizon=strtod(argv[z],NULL);
		}

		if (strcmp(argv[x],"-U")==0)
		{
			z=x+1;
			if (z<=y && argv[z][0] && argv[z][0]!='-')
				strncpy(uplink_host,argv[z],sizeof(uplink_host)-1);
			uplink_host[sizeof(uplink_host)-1] = 0;
		}

		if (strcmp(argv[x],"-UP")==0)
		{
			z=x+1;
			if (z<=y && argv[z][0] && argv[z][0]!='-')
				strncpy(uplink_port,argv[z],sizeof(uplink_port)-1);
			uplink_port[sizeof(uplink_port)-1] = 0;
		}

		if (strcmp(argv[x],"-UV")==0)
		{
			z=x+1;
			if (z<=y && argv[z][0] && argv[z][0]!='-')
				strncpy(uplink_vfo,argv[z],sizeof(uplink_vfo)-1);
			uplink_vfo[sizeof(uplink_vfo)-1] = 0;
		}

		if (strcmp(argv[x],"-D")==0)
		{
			z=x+1;
			if (z<=y && argv[z][0] && argv[z][0]!='-')
				strncpy(downlink_host,argv[z],sizeof(downlink_host)-1);
			downlink_host[sizeof(downlink_host)-1] = 0;
		}

		if (strcmp(argv[x],"-DP")==0)
		{
			z=x+1;
			if (z<=y && argv[z][0] && argv[z][0]!='-')
				strncpy(downlink_port,argv[z],sizeof(downlink_port)-1);
			downlink_port[sizeof(downlink_port)-1] = 0;
		}

		if (strcmp(argv[x],"-DV")==0)
		{
			z=x+1;
			if (z<=y && argv[z][0] && argv[z][0]!='-')
				strncpy(downlink_vfo,argv[z],sizeof(downlink_vfo)-1);
			downlink_vfo[sizeof(downlink_vfo)-1] = 0;
		}

		if (strcmp(argv[x],"-o")==0) {
			z=x+1;
			if (z<=y && argv[z][0] && argv[z][0]!='-')
				strncpy(outputfile,argv[z],40);
		}

		if (strcmp(argv[x],"-n")==0) {
			z=x+1;
			if (z<=y && argv[z][0] && argv[z][0]!='-')
				strncpy(netport,argv[z],5);
		}

		if (strcmp(argv[x],"-north")==0) /* Default */
			io_lat='N';

		if (strcmp(argv[x],"-south")==0)
			io_lat='S';

		if (strcmp(argv[x],"-west")==0)  /* Default */
			io_lon='W';

		if (strcmp(argv[x],"-east")==0)
			io_lon='E';
	}

	/* We're done scanning command-line arguments */

	/* If no command-line (-t or -q) arguments have been passed
	   to PREDICT, create qth and tle filenames based on the
	   default ($HOME) directory. */

	env=getenv("HOME");

	if (qth_cli[0]==0)
		sprintf(qthfile,"%s/.flyby/flyby.qth",env);
	else
		sprintf(qthfile,"%s%c",qth_cli,0);

	if (tle_cli[0]==0)
		sprintf(tlefile,"%s/.flyby/flyby.tle",env);
	else
		sprintf(tlefile,"%s%c",tle_cli,0);

	/* Test for interactive/non-interactive mode of operation
	   based on command-line arguments given to PREDICT. */

	if (updatefile[0])
		interactive=0;
	else
		interactive=1;

	if (interactive) {
		sprintf(dbfile,"%s/.flyby/flyby.db",env);

		/* If the transponder database file doesn't already
		   exist under $HOME/.flyby, and a working environment
		   is available, place a default copy from the PREDICT
		   distribution under $HOME/.flyby. */

		db=fopen(dbfile,"r");

		if (db==NULL) {
			sprintf(temp,"%sdefault/flyby.db",flybypath);
			CopyFile(temp,dbfile);
		} else
			fclose(db);
	}

	x=ReadDataFiles();

	if (x>1)  /* TLE file was loaded successfully */ {
		if (updatefile[0]) {
	    printf("*** flyby: Updating TLE data using file(s) %s", updatefile);
			y=0;
			z=0;
			temp[0]=0;

			while (updatefile[y]!=0) {
				while (updatefile[y]!='\n' && updatefile[y]!=0 && y<79) {
					temp[z]=updatefile[y];
					z++;
					y++;
				}

				temp[z]=0;

				if (temp[0]) {
					AutoUpdate(temp);
					temp[0]=0;
					z=0;
					y++;
				}
			}
			exit(0);
		}
	}

	if (x != 3) {
		if (tle_cli[0] || qth_cli[0]) {
			/* "Houston, we have a problem..." */

			printf("\n%c",7);

			if (x^1)
				printf("*** ERROR!  Your QTH file \"%s\" could not be loaded!\n",qthfile);

			if (x^2)
				printf("*** ERROR!  Your TLE file \"%s\" could not be loaded!\n",tlefile);

			printf("\n");

			exit(-1);
		}
	}

	if (interactive) {
		/* We're in interactive mode.  Prepare the screen */

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

		if (x<3) {
			NewUser();
			x=ReadDataFiles();
			QthEdit();
		}
	}

	if (x==3) {
		/* Create socket and connect to rotctld if present. */

		if (rotctld_host[0]!=0)
		{
			if (getaddrinfo(rotctld_host, rotctld_port, &hints, &servinfo))
			{
				bailout("getaddrinfo error");
				exit(-1);
			}

			for(servinfop = servinfo; servinfop != NULL; servinfop = servinfop->ai_next)
			{
				if ((rotctld_socket = socket(servinfop->ai_family, servinfop->ai_socktype,
					servinfop->ai_protocol)) == -1) {
					continue;
				}
				if (connect(rotctld_socket, servinfop->ai_addr, servinfop->ai_addrlen) == -1)
				{
					close(rotctld_socket);
					continue;
				}

				break;
			}
			if (servinfop == NULL)
			{
				bailout("Unable to connect to rotctld");
				exit(-1);
			}
			freeaddrinfo(servinfo);
			/* TrackDataNet() will wait for confirmation of a command before sending
			   the next so we bootstrap this by asking for the current position */
			send(rotctld_socket, "p\n", 2, 0);
			sock_readline(rotctld_socket, NULL, 256);
		}

		/* Create socket and connect to uplink rigctld. */

		if (uplink_host[0]!=0)
		{
			if (getaddrinfo(uplink_host, uplink_port, &hints, &servinfo))
			{
				bailout("getaddrinfo error");
				exit(-1);
			}

			for(servinfop = servinfo; servinfop != NULL; servinfop = servinfop->ai_next)
			{
				if ((uplink_socket = socket(servinfop->ai_family, servinfop->ai_socktype,
					servinfop->ai_protocol)) == -1)
				{
					continue;
				}
				if (connect(uplink_socket, servinfop->ai_addr, servinfop->ai_addrlen) == -1)
				{
					close(uplink_socket);
					continue;
				}

				break;
			}
			if (servinfop == NULL)
			{
				bailout("Unable to connect to uplink rigctld");
				exit(-1);
			}
			freeaddrinfo(servinfo);
			/* FreqDataNet() will wait for confirmation of a command before sending
			   the next so we bootstrap this by asking for the current frequency */
			send(uplink_socket, "f\n", 2, 0);
		}

		/* Create socket and connect to downlink rigctld. */

		if (downlink_host[0]!=0)
		{
			if (getaddrinfo(downlink_host, downlink_port, &hints, &servinfo))
			{
				bailout("getaddrinfo error");
				exit(-1);
			}

			for(servinfop = servinfo; servinfop != NULL; servinfop = servinfop->ai_next)
			{
				if ((downlink_socket = socket(servinfop->ai_family, servinfop->ai_socktype,
					servinfop->ai_protocol)) == -1)
				{
					continue;
				}
				if (connect(downlink_socket, servinfop->ai_addr, servinfop->ai_addrlen) == -1)
				{
					close(downlink_socket);
					continue;
				}

				break;
			}
			if (servinfop == NULL)
			{
				bailout("Unable to connect to downlink rigctld");
				exit(-1);
			}
			freeaddrinfo(servinfo);
			/* FreqDataNet() will wait for confirmation of a command before sending
			   the next so we bootstrap this by asking for the current frequency */
			send(downlink_socket, "f\n", 2, 0);
		}

		/* Socket activated here.  Remember that
		the socket data is updated only when
		running in the real-time tracking modes. */

		MainMenu();
		int num_sats;
		predict_observer_t *observer = predict_create_observer("test_qth", qth.stnlat*M_PI/180.0, qth.stnlong*M_PI/180.0, qth.stnalt);
		num_sats = totalsats;
		predict_orbit_t **orbits = (predict_orbit_t**)malloc(sizeof(predict_orbit_t*)*num_sats);
		for (int i=0; i < num_sats; i++){
			const char *tle[2] = {sat[i].line1, sat[i].line2};
			orbits[i] = predict_create_orbit(predict_parse_tle(tle));
			memcpy(orbits[i]->name, sat[i].name, 25);
		}

		int indx = 0;

		do {
			key=getch();

			if (key!='T')
				key=tolower(key);

			switch (key) {
				case 'p':
				case 'v':
					Print("",0);
					PrintVisible("");
					indx=Select(num_sats, orbits);

					predict_orbit(orbits[indx], predict_to_julian(time(NULL)));

					if (indx!=-1 && sat[indx].meanmo!=0.0 && !predict_decayed(orbits[indx])) {
						Predict(orbits[indx], observer, key);
					}

					MainMenu();
					break;

				case 'n':
					Print("",0);
					PredictSunMoon(PREDICT_MOON, observer);
					MainMenu();
					break;

				case 'o':
					Print("",0);
					PredictSunMoon(PREDICT_SUN, observer);
					MainMenu();
					break;

				case 'u':
					AutoUpdate("");
					MainMenu();
					break;

				case 'd':
					ShowOrbitData(num_sats, orbits);
					MainMenu();
					break;

				case 'g':
					QthEdit();
					MainMenu();
					break;

				case 't':
				case 'T':
					indx=Select(num_sats, orbits);
					predict_orbit(orbits[indx], predict_to_julian(time(NULL)));

					if (indx!=-1 && sat[indx].meanmo!=0.0 && !predict_decayed(orbits[indx])) {
						SingleTrack(horizon, orbits[indx], observer, sat_db[indx]);
					}

					MainMenu();
					break;

				case 'm':
				case 'l':

					MultiTrack(observer, num_sats, orbits, key, 'k');
					MainMenu();
					break;

				case 'i':
					ProgramInfo(horizon);
					MainMenu();
					break;

				case 's':
					indx=Select(num_sats, orbits);

					predict_orbit(orbits[indx], predict_to_julian(time(NULL)));
					if (indx!=-1 && sat[indx].meanmo!=0.0 && !predict_decayed(orbits[indx]) ) {
						Print("",0);

						Illumination(orbits[indx]);
					}

					MainMenu();
					break;
			}

		} while (key!='q' && key!=27);

		if (rotctld_socket!=-1)
		{
			send(rotctld_socket, "q\n", 2, 0);
			close(rotctld_socket);
		}
		if (uplink_socket!=-1)
		{
			send(uplink_socket, "q\n", 2, 0);
			close(uplink_socket);
		}
		if (downlink_socket!=-1)
		{
			send(downlink_socket, "q\n", 2, 0);
			close(downlink_socket);
		}

		curs_set(1);
		bkgdset(COLOR_PAIR(1));
		clear();
		refresh();
		endwin();

		for (int i=0; i < num_sats; i++){
			predict_destroy_orbit(orbits[i]);
		}
		free(orbits);
		predict_destroy_observer(observer);

	}

	exit(0);
}
