#include "prediction_schedules.h"
#include "ui.h"
#include <math.h>

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

///////////////////////////////////////////////////////////////////////////////////////////
// Functions related to general pass predictions for sun, moon and satellites.           //
// More or less the same code as was present in PREDICT (Predict(), PredictSun(), ...),  //
// but with satellite calculations replaced by libpredict calls and some global variable //
// avoidance.                                                                            //
///////////////////////////////////////////////////////////////////////////////////////////

long date_to_daynumber(int m, int d, int y)
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

double prompt_user_for_time(const char* info_str)
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

	return ((double)date_to_daynumber(mm,dd,yy)+((hr/24.0)+(min/1440.0)+(sec/86400.0)));
}

/* This function buffers and displays orbital predictions.
 *
 * \param title Title to show on top of the screen
 * \param string Data string to display
 * \param mode Display mode for changing the UI according to the type
 * of data to show: 'p' for satellite passes, 'v' for visible passes, 's' for solar illumination prediction,
 * 'm' for moon passes, 's' for sun passes
 * \return 1 if user wants to quit, 0 otherwise
 **/
int schedule_print(const char *title, const char *string, char mode)
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
			int title_col = 79-strlen(title);
			mvprintw(1,title_col, "%s", title);
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

/* This function acts as a filter to display passes that could
 * possibly be visible to the ground station.  It works by first
 * buffering prediction data generated by the Predict() function
 * and then checking it to see if at least a part of the pass
 * is visible.  If it is, then the buffered prediction data
 * is sent to the Print() function so it can be displayed
 * to the user.
 *
 * \param title Title to show on top of the screen
 * \param string Data to display
 * \return 1 if user wants to quit, 0 otherwise
 **/
int visible_schedule_print(const char *title, const char *string)
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
						quit=schedule_print(title,line,'v');
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

void satellite_pass_display_schedule(const char *name, predict_orbital_elements_t *orbital_elements, predict_observer_t *qth, char mode)
{
	schedule_print("","",0);
	visible_schedule_print("","");
	bool should_quit = false;
	bool should_break = false;
	char data_string[MAX_NUM_CHARS];
	char time_string[MAX_NUM_CHARS];

	predict_julian_date_t curr_time = prompt_user_for_time(name);

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
					should_quit=schedule_print(title,data_string,'p');
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

					should_quit=visible_schedule_print(title,data_string);
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
				should_quit=schedule_print(title,"\n",'p');
			}

			if (mode=='v') {
				should_quit=visible_schedule_print(title,"\n");
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
		any_key();
		bkgdset(COLOR_PAIR(1));
		refresh();
	}
}

void sun_moon_pass_display_schedule(enum astronomical_body object, predict_observer_t *qth)
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
		default:
		break;
	}
	schedule_print("","",0);

	int iaz, iel, lastel=0;
	char string[MAX_NUM_CHARS], quit=0;
	double lastdaynum, rise=0.0;
	char time_string[MAX_NUM_CHARS];

	predict_julian_date_t daynum = prompt_user_for_time(name_str);
	clear();
	struct predict_observation obs = {0};

	const double HORIZON_THRESHOLD = 0.03;
	const double REDUCTION_FACTOR = 0.004;

	double right_ascension = 0;
	double declination = 0;
	double longitude = 0;

	do {
		//determine sun- or moonrise
		observe_astronomical_body(object, qth, daynum, &obs);

		while (rise==0.0) {
			if (fabs(obs.elevation*180.0/M_PI)<HORIZON_THRESHOLD) {
				rise=daynum;
			} else {
				daynum-=(REDUCTION_FACTOR*obs.elevation*180.0/M_PI);
				observe_astronomical_body(object, qth, daynum, &obs);
			}
		}

		observe_astronomical_body(object, qth, rise, &obs);
		daynum=rise;

		iaz=(int)rint(obs.azimuth*180.0/M_PI);
		iel=(int)rint(obs.elevation*180.0/M_PI);

		//display pass of sun or moon from rise
		do {
			//display data
			time_t epoch = predict_from_julian(daynum);
			strftime(time_string, MAX_NUM_CHARS, "%a %d%b%y %H:%M:%S", gmtime(&epoch));
			sprintf(string,"      %s%4d %4d  %5.1f  %5.1f  %5.1f  %6.1f%7.3f\n",time_string, iel, iaz, right_ascension, declination, longitude, obs.range_rate, obs.range);
			quit=schedule_print("",string,print_mode);
			lastel=iel;
			lastdaynum=daynum;

			//calculate data
			daynum+=0.04*(cos(M_PI/180.0*(obs.elevation*180.0/M_PI+0.5)));
			observe_astronomical_body(object, qth, daynum, &obs);
			iaz=(int)rint(obs.azimuth*180.0/M_PI);
			iel=(int)rint(obs.elevation*180.0/M_PI);
		} while (iel>3 && quit==0);

		//end the pass
		while (lastel!=0 && quit==0) {
			daynum=lastdaynum;

			//find sun/moon set
			do {
				daynum+=0.004*(sin(M_PI/180.0*(obs.elevation*180.0/M_PI+0.5)));
				observe_astronomical_body(object, qth, daynum, &obs);
				iaz=(int)rint(obs.azimuth*180.0/M_PI);
				iel=(int)rint(obs.elevation*180.0/M_PI);
			} while (iel>0);

			time_t epoch = predict_from_julian(daynum);
			strftime(time_string, MAX_NUM_CHARS, "%a %d%b%y %H:%M:%S", gmtime(&epoch));

			sprintf(string,"      %s%4d %4d  %5.1f  %5.1f  %5.1f  %6.1f%7.3f\n",time_string, iel, iaz, right_ascension, declination, longitude, obs.range_rate, obs.range);
			quit=schedule_print("",string,print_mode);
			lastel=iel;
		} //will continue until we have elevation 0 at the end of the pass

		quit=schedule_print("","\n",'o');
		daynum+=0.4;
		rise=0.0;

	} while (quit==0);
}

void solar_illumination_display_predictions(const char *name, predict_orbital_elements_t *orbital_elements)
{
	double startday, oneminute, sunpercent;
	int eclipses, minutes, quit, breakout=0, count;
	char string1[MAX_NUM_CHARS], string[MAX_NUM_CHARS], datestring[MAX_NUM_CHARS];

	schedule_print("","",0);

	oneminute=1.0/(24.0*60.0);

	predict_julian_date_t daynum = floor(prompt_user_for_time(name));
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

		quit=schedule_print(title,string,'s');

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
