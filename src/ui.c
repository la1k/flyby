#include "singletrack.h"
#include "xdg_basedirs.h"
#include "config.h"
#include <math.h>
#include <time.h>
#include <menu.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "filtered_menu.h"
#include "ui.h"
#include "qth_config.h"
#include "transponder_editor.h"
#include "multitrack.h"
#include "locator.h"

void bailout(const char *string)
{
	beep();
	curs_set(1);
	bkgdset(COLOR_PAIR(1));
	clear();
	refresh();
	endwin();
	fprintf(stderr,"*** flyby: %s\n",string);
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

void AutoUpdate(const char *string, struct tle_db *tle_db)
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
	int *update_status = (int*)calloc(tle_db->num_tles, sizeof(int));
	tle_db_update(filename, tle_db, update_status);

	if (interactive_mode) {
		move(12, 0);
	}

	int num_updated = 0;
	bool in_new_file = false;
	bool not_written = false;
	char new_file[MAX_NUM_CHARS] = {0};
	for (int i=0; i < tle_db->num_tles; i++) {
		if (update_status[i] & TLE_DB_UPDATED) {
			//print updated entries
			if (interactive_mode) {
				printw("Updated %s (%ld)", tle_db->tles[i].name, tle_db->tles[i].satellite_number);
			} else {
				printf("Updated %s (%ld)", tle_db->tles[i].name, tle_db->tles[i].satellite_number);
			}
			if (update_status[i] & TLE_IN_NEW_FILE) {
				if (!in_new_file) {
					strncpy(new_file, tle_db->tles[i].filename, MAX_NUM_CHARS);
				}

				in_new_file = true;
				if (interactive_mode) {
					printw(" (*)");
				} else {
					printf(" (*)");
				}
			}
			if (!(update_status[i] & TLE_IN_NEW_FILE) && !(update_status[i] & TLE_FILE_UPDATED)) {
				not_written = true;
				if (interactive_mode) {
					printw(" (X)");
				} else {
					printf(" (X)");
				}
			}
			if (interactive_mode) {
				printw("\n");
			} else {
				printf("\n");
			}
			num_updated++;
		}
	}
	free(update_status);

	//print file information
	if (interactive_mode) {
		if (in_new_file) {
			printw("\nSatellites marked with (*) were put in a new file (%s).\n", new_file);
		}
		if (not_written) {
			printw("\nSatellites marked with (X) were not written to file.");
		}
	} else {
		if (in_new_file) {
			printf("\nSatellites marked with (*) were put in a new file (%s).\n", new_file);
		}
		if (not_written) {
			printf("\nSatellites marked with (X) were not written to file.");
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

long DayNum(int m, int d, int y)
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
	Print("","",0);
	PrintVisible("","");
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
		bkgdset(COLOR_PAIR(1));
		refresh();
	}
}

void celestial_predict(enum celestial_object object, predict_observer_t *qth, predict_julian_date_t time, struct predict_observation *obs)
{
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
	Print("","",0);

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

void ShowOrbitData(const char *name, predict_orbital_elements_t *orbital_elements)
{
	int c, namelength, age;
	double an_period, no_period, sma, c1, e2, satepoch;
	char days[5];

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

		namelength=strlen(name);

		printw("\n");

		for (c=41; c>namelength; c-=2)
			printw(" ");

		bkgdset(COLOR_PAIR(3)|A_BOLD);
		attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
		clear();

		mvprintw(0,0,"                                                                                ");
		mvprintw(1,0,"  flyby Orbital Data                                                            ");
		mvprintw(2,0,"                                                                                ");

		mvprintw(1,25,"(%ld) %s", orbital_elements->satellite_number, name);

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
}

void update_latlon_fields_from_locator(FIELD *locator, FIELD *longitude, FIELD *latitude)
{
	double longitude_new = 0, latitude_new = 0;
	maidenhead_to_latlon(field_buffer(locator, 0), &longitude_new, &latitude_new);
	char temp[MAX_NUM_CHARS];
	snprintf(temp, MAX_NUM_CHARS, "%f", longitude_new);
	set_field_buffer(longitude, 0, temp);
	snprintf(temp, MAX_NUM_CHARS, "%f", latitude_new);
	set_field_buffer(latitude, 0, temp);
}

void update_locator_field_from_latlon(FIELD *longitude, FIELD *latitude, FIELD *locator)
{
	double curr_longitude = strtod(field_buffer(longitude, 0), NULL);
	double curr_latitude = strtod(field_buffer(latitude, 0), NULL);
	char locator_str[MAX_NUM_CHARS];
	latlon_to_maidenhead(curr_latitude, curr_longitude, locator_str);
	set_field_buffer(locator, 0, locator_str);
}

#define QTH_FIELD_LENGTH 10
#define NUM_QTH_FIELDS 5
#define INPUT_NUM_CHARS 128
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

	//set up form for QTH editing
	FIELD *fields[NUM_QTH_FIELDS+1] = {
		new_field(1, QTH_FIELD_LENGTH, 0, 0, 0, 0),
		new_field(1, QTH_FIELD_LENGTH, 1, 0, 0, 0),
		new_field(1, QTH_FIELD_LENGTH, 2, 0, 0, 0),
		new_field(1, QTH_FIELD_LENGTH, 3, 0, 0, 0),
		new_field(1, QTH_FIELD_LENGTH, 4, 0, 0, 0),
		NULL};
	FIELD *name = fields[0];
	FIELD *locator = fields[1];
	FIELD *latitude = fields[2];
	FIELD *longitude = fields[3];
	FIELD *altitude = fields[4];
	FORM *form = new_form(fields);
	for (int i=0; i < NUM_QTH_FIELDS; i++) {
		set_field_fore(fields[i], COLOR_PAIR(2)|A_BOLD);
		field_opts_off(fields[i], O_STATIC);
		set_max_field(fields[i], INPUT_NUM_CHARS);
	}

	//set up windows
	int win_height = NUM_QTH_FIELDS+1;
	int win_width = QTH_FIELD_LENGTH;
	int win_row = 8;
	int win_col = 40;
	WINDOW *form_win = newwin(win_height, win_width, win_row, win_col);
	int rows, cols;
	scale_form(form, &rows, &cols);
	set_form_win(form, form_win);
	set_form_sub(form, derwin(form_win, rows, cols, 0, 0));
	keypad(form_win, TRUE);
	post_form(form);

	//display headers
	attrset(COLOR_PAIR(4)|A_BOLD);
	int info_col = 20;
	mvprintw(win_row,info_col,"Station Callsign  : ");
	mvprintw(win_row+1,info_col,"Station Locator   : ");
	mvprintw(win_row+2,info_col,"Station Latitude  : ");
	mvprintw(win_row+3,info_col,"Station Longitude : ");
	mvprintw(win_row+4,info_col,"Station Altitude  : ");
	mvprintw(win_row+6,info_col-5," Only decimal notation (e.g. 74.2467) allowed");
	mvprintw(win_row+7,info_col-5," for longitude and latitude.");
	mvprintw(win_row+9,info_col-5," Navigate using keypad or ENTER. Press ENTER");
	mvprintw(win_row+10,info_col-5," on last field or ESC to save and exit.");

	//print units
	attrset(COLOR_PAIR(2)|A_BOLD);
	mvprintw(win_row+2,win_col+win_width+1,"[DegN]",qth->latitude*180.0/M_PI);
	mvprintw(win_row+3,win_col+win_width+1,"[DegE]",qth->longitude*180.0/M_PI);
	mvprintw(win_row+4,win_col+win_width+1,"[m]",qth->altitude);

	//fill form with QTH contents
	set_field_buffer(name, 0, qth->name);
	char temp[MAX_NUM_CHARS] = {0};
	snprintf(temp, MAX_NUM_CHARS, "%f", qth->latitude*180.0/M_PI);
	set_field_buffer(latitude, 0, temp);
	snprintf(temp, MAX_NUM_CHARS, "%f", qth->longitude*180.0/M_PI);
	set_field_buffer(longitude, 0, temp);
	snprintf(temp, MAX_NUM_CHARS, "%f", qth->altitude);
	set_field_buffer(altitude, 0, temp);
	update_locator_field_from_latlon(longitude, latitude, locator);

	refresh();
	wrefresh(form_win);

	//handle input characters to QTH form
	bool run_form = true;
	while (run_form) {
		int key = wgetch(form_win);

		switch (key) {
			case KEY_UP:
				form_driver(form, REQ_UP_FIELD);
				break;
			case KEY_DOWN:
				form_driver(form, REQ_DOWN_FIELD);
				break;
			case KEY_LEFT:
				form_driver(form, REQ_PREV_CHAR);
				break;
			case KEY_RIGHT:
				form_driver(form, REQ_NEXT_CHAR);
				break;
			case 10:
				if (current_field(form) == fields[NUM_QTH_FIELDS-1]) {
					run_form = false;
				} else {
					form_driver(form, REQ_NEXT_FIELD);
				}
				break;
			case KEY_BACKSPACE:
				form_driver(form, REQ_DEL_PREV);
				form_driver(form, REQ_VALIDATION);
				if (current_field(form) == locator) {
					update_latlon_fields_from_locator(locator, longitude, latitude);
				} else {
					update_locator_field_from_latlon(longitude, latitude, locator);
				}
				break;
			case KEY_DC:
				form_driver(form, REQ_DEL_CHAR);
				break;
			case 27:
				run_form = false;
				break;
			default:
				form_driver(form, key);
				form_driver(form, REQ_VALIDATION); //update buffer with field contents
				if (current_field(form) == locator) {
					update_latlon_fields_from_locator(locator, longitude, latitude);
				} else {
					update_locator_field_from_latlon(longitude, latitude, locator);
				}

				break;
		}
	}

	//copy field contents to predict_observer
	strncpy(qth->name, field_buffer(name, 0), INPUT_NUM_CHARS);
	trim_whitespaces_from_end(qth->name);
	qth->latitude = strtod(field_buffer(latitude, 0), NULL)*M_PI/180.0;
	qth->longitude = strtod(field_buffer(longitude, 0), NULL)*M_PI/180.0;
	qth->altitude = strtod(field_buffer(altitude, 0), NULL);

	//write to file
	qth_to_file(qthfile, qth);
	curs_set(0);

	//free form
	unpost_form(form);
	free_field(name);
	free_field(latitude);
	free_field(longitude);
	free_field(altitude);
	free_field(locator);
	free_form(form);
	delwin(form_win);
}

void Illumination(const char *name, predict_orbital_elements_t *orbital_elements)
{
	double startday, oneminute, sunpercent;
	int eclipses, minutes, quit, breakout=0, count;
	char string1[MAX_NUM_CHARS], string[MAX_NUM_CHARS], datestring[MAX_NUM_CHARS];

	Print("","",0);

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

void trim_whitespaces_from_end(char *string)
{
	//trim whitespaces from end
	for (int i=strlen(string)-1; i >= 0; i--) {
		if (string[i] == ' ') {
			string[i] = '\0';
		} else if (isdigit(string[i]) || isalpha(string[i])) {
			break;
		}
	}
}

void prepare_pattern(char *string)
{
	trim_whitespaces_from_end(string);

	//lowercase to uppercase
	for (int i=0; i < strlen(string); i++) {
		string[i] = toupper(string[i]);
	}
}

//column at which to print keyhints in whitelist
#define WHITELIST_KEYHINT_COL 42

//row at which to print info whether only entries with transponders are displayed
#define WHITELIST_TRANSPONDER_TOGGLE_INFO_ROW 24

void EditWhitelist(struct tle_db *tle_db, const struct transponder_db *transponder_db)
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

	if (tle_db->num_tles > 0) {
		attrset(COLOR_PAIR(3)|A_BOLD);
		mvprintw(LINES-3,46,"%d satellites",tle_db->num_tles);
	}

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
	WINDOW *subwin = derwin(form_win, rows, cols, 2, 2);
	set_form_sub(form, subwin);

	post_form(form);
	wrefresh(form_win);

	/* Create the window to be associated with the menu */
	int window_width = 35;
	int window_ypos = row;
	my_menu_win = newwin(LINES-window_ypos-1, window_width, window_ypos, 5);

	keypad(my_menu_win, TRUE);
	wattrset(my_menu_win, COLOR_PAIR(4));

	if (tle_db->num_tles > 0) {
		box(my_menu_win, 0, 0);
	}

	if (tle_db->num_tles > 0) {
		/* Print description */
		attrset(COLOR_PAIR(3)|A_BOLD);
		int col = WHITELIST_KEYHINT_COL;
		row = 5;
		mvprintw( 6,col,"Use upper-case characters to ");
		mvprintw( 7,col,"filter satellites by name,");
		mvprintw( 8,col,"satellite number or TLE filename.");


		mvprintw( 10,col,"Use cursor keys to move up/down");
		mvprintw( 11,col,"the list and then disable/enable");
		mvprintw( 12,col,"with        .");

		mvprintw( 14,col,"Press  q  to return to menu or");
		mvprintw( 15,col,"wipe query field if filled.");
		mvprintw( 17,col,"Press  a  to toggle visible entries.");
		mvprintw( 19,col,"Press  w  to wipe query field.");
		mvprintw( 21,col,"Press  t  to enable/disable");
		mvprintw( 22,col,"transponder filter.");
		mvprintw(5, 6, "Filter TLEs by string:");
		row = 18;

		/* Print keyboard bindings in special format */
		attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
		mvprintw( 12,col+5," SPACE ");
		mvprintw( 14,col+6," q ");
		mvprintw( 17,col+6," a ");
		mvprintw( 19,col+6," w ");
		mvprintw( 21,col+6," t ");
	}

	refresh();

	struct filtered_menu menu = {0};
	filtered_menu_from_tle_db(&menu, tle_db, my_menu_win);

	char field_contents[MAX_NUM_CHARS] = {0};

	refresh();
	wrefresh(my_menu_win);
	form_driver(form, REQ_VALIDATION);
	if (tle_db->num_tles > 0) {
		wrefresh(form_win);
	}
	bool run_menu = true;

	while (run_menu) {
		if (tle_db->num_tles == 0) {
			char *data_home = xdg_data_home();
			char *data_dirs = xdg_data_dirs();
			string_array_t data_dirs_list = {0};
			stringsplit(data_dirs, &data_dirs_list);

			int row = 5;
			int col = 10;
			attrset(COLOR_PAIR(1));
			mvprintw(row++, col, "No TLEs found.");
			row++;
			mvprintw(row++, col, "TLE files can be placed in ");
			mvprintw(row++, col, "the following locations:");
			row++;
			mvprintw(row++, col, "* %s%s", data_home, TLE_RELATIVE_DIR_PATH);

			for (int i=0; i < string_array_size(&data_dirs_list); i++) {
				mvprintw(row++, col, "* %s%s", string_array_get(&data_dirs_list, i), TLE_RELATIVE_DIR_PATH);
			}
			row++;
			mvprintw(row++, col, "Files will be loaded on program restart.");

			free(data_home);
			free(data_dirs);
			string_array_free(&data_dirs_list);
			refresh();
		}

		//handle keyboard
		c = wgetch(my_menu_win);
		bool handled = false;

		handled = filtered_menu_handle(&menu, c);

		wrefresh(my_menu_win);

		if (!handled) {
			switch (c) {
				case 't':
					filtered_menu_only_comsats(&menu, !menu.display_only_entries_with_transponders);
					filtered_menu_pattern_match(&menu, tle_db, transponder_db, field_contents);
					if (menu.display_only_entries_with_transponders) {
						attrset(COLOR_PAIR(1));
						int row = WHITELIST_TRANSPONDER_TOGGLE_INFO_ROW;
						mvprintw(row++, WHITELIST_KEYHINT_COL, "Only entries with transponders");
						mvprintw(row, WHITELIST_KEYHINT_COL, "are shown.");
						refresh();
					} else {
						int row = WHITELIST_TRANSPONDER_TOGGLE_INFO_ROW;
						move(row++, WHITELIST_KEYHINT_COL);
						clrtoeol();
						move(row, WHITELIST_KEYHINT_COL);
						clrtoeol();
						refresh();
					}

					break;
				case 'q':
					strncpy(field_contents, field_buffer(field[0], 0), MAX_NUM_CHARS);
					prepare_pattern(field_contents);

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
					prepare_pattern(field_contents);

					filtered_menu_pattern_match(&menu, tle_db, transponder_db, field_contents);

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

	delwin(subwin);
	delwin(my_menu_win);
	delwin(form_win);

	free_field(field[0]);
}

#define MAIN_MENU_BACKGROUND_STYLE COLOR_PAIR(4)|A_REVERSE
int PrintMainMenuOption(WINDOW *window, int row, int col, char key, const char *description)
{
	wattrset(window, COLOR_PAIR(1));
	mvwprintw(window, row,col,"%c", key);
	wattrset(window, MAIN_MENU_BACKGROUND_STYLE);
	mvwprintw(window, row,col+1,"%s", description);
	return col + 1 + strlen(description);
}

/**
 * Print global main menu options to specified window. Display format is inspired by htop. :-)
 *
 * \param window Window for printing
 **/
void PrintMainMenu(WINDOW *window)
{
	int row = 0;
	int column = 0;

	column = PrintMainMenuOption(window, row, column, 'W', "Enable/Disable Satellites");
	column = PrintMainMenuOption(window, row, column, 'G', "Edit Ground Station      ");
	column = PrintMainMenuOption(window, row, column, 'E', "Edit Transponder Database");
	column = 0;
	row++;
	column = PrintMainMenuOption(window, row, column, 'I', "Program Information      ");
	column = PrintMainMenuOption(window, row, column, 'O', "Solar Pass Predictions   ");
	column = PrintMainMenuOption(window, row, column, 'N', "Lunar Pass Predictions   ");
	column = 0;
	row++;
	column = PrintMainMenuOption(window, row, column, 'U', "Update Sat Elements      ");
	column = PrintMainMenuOption(window, row, column, 'M', "Multitrack settings      ");
	column = PrintMainMenuOption(window, row, column, 'Q', "Exit flyby               ");

	wrefresh(window);
}

/**
 * Print rigctld info to terminal.
 *
 * \param name Name used for printing
 * \param rigctld Rigctl info.
 **/
void print_rigctld_info(const char *name, rigctld_info_t *rigctld)
{
	if (rigctld->connected) {
		printw("\n");
		printw("\t\t%s VFO\t: Enabled\n", name);
		printw("\t\t - Connected to rigctld: %s:%s\n", rigctld->host, rigctld->port);

		printw("\t\t - VFO name: ");
		if (strlen(rigctld->vfo_name) > 0) {
			printw("%s\n", rigctld->vfo_name);
		} else {
			printw("Undefined\n");
		}
	} else {
		printw("\t\t%s VFO\t: Not enabled\n", name);
	}
}

void ProgramInfo(const char *qthfile, struct tle_db *tle_db, struct transponder_db *transponder_db, rotctld_info_t *rotctld, rigctld_info_t* downlink, rigctld_info_t *uplink)
{
	Banner();
	attrset(COLOR_PAIR(3)|A_BOLD);

	printw("\n\n\n\n\n\t\tflyby version : %s\n",FLYBY_VERSION);
	printw("\t\tQTH file        : %s\n", qthfile);
	printw("\t\tTLE file        : ");
	if (tle_db->num_tles > 0) {
		string_array_t tle_db_files = tle_db_filenames(tle_db);
		printw("%d TLEs loaded from %d files\n", tle_db->num_tles, string_array_size(&tle_db_files));
		string_array_free(&tle_db_files);
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
		printw("\n");
		printw("\t\tAutoTracking    : Enabled\n");
		printw("\t\t - Connected to rotctld: %s:%s\n", rotctld->host, rotctld->port);

		printw("\t\t - Tracking horizon: %.2f degrees. ", rotctld->tracking_horizon);

		if (rotctld->update_time_interval > 0)
			printw("Update every %d seconds", rotctld->update_time_interval);

		printw("\n");
	} else {
		printw("\t\tAutoTracking    : Not enabled\n");
	}

	print_rigctld_info("Uplink", uplink);
	print_rigctld_info("Downlink", downlink);

	refresh();
	attrset(COLOR_PAIR(4)|A_BOLD);
	AnyKey();
}

void EditTransponderDatabaseField(const struct tle_db_entry *sat_info, WINDOW *form_win, struct sat_db_entry *sat_entry)
{
	struct transponder_editor *transponder_editor = transponder_editor_create(sat_info, form_win, sat_entry);

	wrefresh(form_win);
	bool run_form = true;
	while (run_form) {
		int c = wgetch(form_win);
		if ((c == 27) || ((c == 10) && (transponder_editor->curr_selected_field == transponder_editor->last_field_in_form))) {
			run_form = false;
		} else if (c == 18) { //CTRL + R
			transponder_editor_sysdefault(transponder_editor, sat_entry);
		} else {
			transponder_editor_handle(transponder_editor, c);
		}

		wrefresh(form_win);
	}

	struct sat_db_entry new_entry;
	transponder_db_entry_copy(&new_entry, sat_entry);

	transponder_editor_to_db_entry(transponder_editor, &new_entry);

	//ensure that we don't write an empty entry (or the same system database entry) to the file database unless we are actually trying to override a system database entry
	if (!transponder_db_entry_equal(&new_entry, sat_entry)) {
		transponder_db_entry_copy(sat_entry, &new_entry);
	}

	transponder_editor_destroy(&transponder_editor);

	delwin(form_win);
}

void DisplayTransponderEntry(const char *name, struct sat_db_entry *entry, WINDOW *display_window)
{
	werase(display_window);

	//display satellite name
	wattrset(display_window, A_BOLD);

	int data_col = 15;
	int info_col = 1;
	int start_row = 0;
	int row = start_row;

	//file location information
	wattrset(display_window, COLOR_PAIR(4)|A_BOLD);
	if (entry->location & LOCATION_TRANSIENT) {
		mvwprintw(display_window, row++, info_col, "To be written to user database.");
	} else if (entry->location & LOCATION_DATA_HOME) {
		mvwprintw(display_window, row++, info_col, "Loaded from user database.");
	} else if (entry->location & LOCATION_DATA_DIRS) {
		mvwprintw(display_window, row++, info_col, "Loaded from system dirs.");
	}

	if ((entry->location & LOCATION_DATA_DIRS) && ((entry->location & LOCATION_DATA_HOME) || (entry->location & LOCATION_TRANSIENT))) {
		mvwprintw(display_window, row++, info_col, "A system default exists.");
	}

	row = start_row+3;

	//display squint angle information
	wattrset(display_window, COLOR_PAIR(4)|A_BOLD);
	mvwprintw(display_window, row, info_col, "Squint angle:");

	wattrset(display_window, COLOR_PAIR(2)|A_BOLD);
	if (entry->squintflag) {
		mvwprintw(display_window, row++, data_col, "Enabled.");

		wattrset(display_window, COLOR_PAIR(4)|A_BOLD);
		mvwprintw(display_window, row++, info_col, "alat: ");
		mvwprintw(display_window, row, info_col, "alon: ");

		wattrset(display_window, COLOR_PAIR(2)|A_BOLD);
		mvwprintw(display_window, row-1, data_col, "%f", entry->alat);
		mvwprintw(display_window, row, data_col, "%f", entry->alon);
	} else {
		mvwprintw(display_window, row, data_col, "Disabled");
	}
	row = start_row+6;

	//display transponder information
	int rows_per_entry = 3;
	int prev_row_diff = 0;
	for (int i=0; i < entry->num_transponders; i++) {
		int display_row = row;
		struct transponder transponder = entry->transponders[i];

		if (display_row + rows_per_entry < LINES-8) {
			int start_row = display_row;
			int info_col = 1;
			int data_col = 4;
			if ((i % 2) == 1) {
				info_col = 25;
				data_col = info_col+3;
			}

			wattrset(display_window, A_BOLD);
			mvwprintw(display_window, ++display_row, info_col, "%.20s", transponder.name);

			//uplink
			if (transponder.uplink_start != 0.0) {
				wattrset(display_window, COLOR_PAIR(4)|A_BOLD);
				mvwprintw(display_window, ++display_row, info_col, "U:");

				wattrset(display_window, COLOR_PAIR(2)|A_BOLD);
				mvwprintw(display_window, display_row, data_col, "%.2f-%.2f", transponder.uplink_start, transponder.uplink_end);
			}

			//downlink
			if (transponder.downlink_start != 0.0) {
				wattrset(display_window, COLOR_PAIR(4)|A_BOLD);
				mvwprintw(display_window, ++display_row, info_col, "D:");

				wattrset(display_window, COLOR_PAIR(2)|A_BOLD);
				mvwprintw(display_window, display_row, data_col, "%.2f-%.2f", transponder.downlink_start, transponder.downlink_end);
			}

			//no uplink/downlink defined
			if ((transponder.uplink_start == 0.0) && (transponder.downlink_start == 0.0)) {
				wattrset(display_window, COLOR_PAIR(2)|A_BOLD);
				mvwprintw(display_window, ++display_row, info_col, "Neither downlink or");
				mvwprintw(display_window, ++display_row, info_col, "uplink is defined.");
				mvwprintw(display_window, ++display_row, info_col, "(Will be ignored)");
			}
			display_row++;

			int diff = display_row-start_row;
			if ((i % 2) == 1) {
				if (diff < prev_row_diff) {
					diff = prev_row_diff;
				}
				row += diff;
			} else {
				prev_row_diff = diff;
			}
		} else {
			wattrset(display_window, COLOR_PAIR(4)|A_BOLD);
			mvwprintw(display_window, ++display_row, info_col, "Truncated to %d of %d transponder entries.", i, entry->num_transponders);
			break;
		}
	}

	//default text when no transponders are defined
	if (entry->num_transponders <= 0) {
		wattrset(display_window, COLOR_PAIR(4)|A_BOLD);
		mvwprintw(display_window, ++row, info_col, "No transponders defined.");
	}
}

void EditTransponderDatabase(int start_index, struct tle_db *tle_db, struct transponder_db *sat_db)
{
	//print header
	clear();
	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);

	int header_height = 3;
	int win_width = 251;
	mvprintw(0,0,"                                                                                ");
	mvprintw(1,0,"  flyby Transponder Database Editor                                             ");
	mvprintw(2,0,"                                                                                ");

	//prepare the other windows
	WINDOW *main_win = newwin(LINES-header_height, win_width, header_height, 0);
	int window_width = 25;
	int window_ypos = header_height+1;
	WINDOW *menu_win = subwin(main_win, LINES-window_ypos-1, window_width, window_ypos, 1);
	WINDOW *display_win = subwin(main_win, LINES-window_ypos-1, 50, window_ypos, window_width+5);
	WINDOW *editor_win = newwin(LINES, 500, 4, 1);

	keypad(menu_win, TRUE);
	wattrset(menu_win, COLOR_PAIR(4));

	//prepare menu
	struct filtered_menu menu = {0};
	filtered_menu_from_tle_db(&menu, tle_db, menu_win);
	filtered_menu_set_multimark(&menu, false);
	filtered_menu_show_whitelisted(&menu, tle_db);

	if (start_index < 0) {
		start_index = 0;
	}
	if ((start_index == 0) && (menu.inverse_entry_mapping[0] == -1)) {
		start_index = menu.entry_mapping[0];
	}

	if (menu.num_displayed_entries > 0) {
		int tle_index = start_index;
		DisplayTransponderEntry(tle_db->tles[tle_index].name, &(sat_db->sats[tle_index]), display_win);
	}

	filtered_menu_select_index(&menu, start_index);

	box(menu_win, 0, 0);

	refresh();
	wrefresh(display_win);
	wrefresh(menu_win);

	bool run_menu = true;
	while (run_menu) {
		//handle keyboard
		int c = wgetch(menu_win);
		filtered_menu_handle(&menu, c);
		int menu_index = filtered_menu_current_index(&menu);

		if ((c == 10) && (menu.num_displayed_entries > 0)) { //enter
			EditTransponderDatabaseField(&(tle_db->tles[menu_index]), editor_win, &(sat_db->sats[menu_index]));

			//clear leftovers from transponder editor
			wclear(main_win);
			wrefresh(main_win);
			refresh();

			//force menu update
			unpost_menu(menu.menu);
			post_menu(menu.menu);

			//refresh the rest and redraw window boxes
			box(menu_win, 0, 0);
			wrefresh(menu_win);
		} else if (c == 'q') {
			run_menu = false;
		}

		//display/refresh transponder entry displayer
		if (menu.num_displayed_entries > 0) {
			DisplayTransponderEntry(tle_db->tles[menu_index].name, &(sat_db->sats[menu_index]), display_win);
		}
		wrefresh(display_win);
	}
	filtered_menu_free(&menu);

	//write transponder database to file
	if (tle_db->num_tles > 0) {
		transponder_db_write_to_default(tle_db, sat_db);
	}

	//read transponder database from file again in order to set the flags correctly
	transponder_db_from_search_paths(tle_db, sat_db);

	delwin(display_win);
	delwin(main_win);
	delwin(menu_win);
	delwin(editor_win);
}

/**
 * Print sun/moon azimuth/elevation to infoboxes on the standard screen. Uses 9 columns and 7 rows.
 *
 * \param row Start row for printing
 * \param col Start column for printing
 * \param qth QTH coordinates
 * \param daynum Time for calculation
 **/
void PrintSunMoon(int row, int col, predict_observer_t *qth, predict_julian_date_t daynum)
{
	struct predict_observation sun;
	predict_observe_sun(qth, daynum, &sun);

	struct predict_observation moon;
	predict_observe_moon(qth, daynum, &moon);

	attrset(COLOR_PAIR(4)|A_REVERSE|A_BOLD);
	mvprintw(row,col,"   Sun   ");
	mvprintw(row+4,col,"   Moon  ");
	if (sun.elevation > 0.0)
		attrset(COLOR_PAIR(3)|A_BOLD);
	else
		attrset(COLOR_PAIR(2));
	mvprintw(row+1,col,"%-7.2fAz",sun.azimuth*180.0/M_PI);
	mvprintw(row+2,col,"%+-6.2f El",sun.elevation*180.0/M_PI);

	attrset(COLOR_PAIR(3)|A_BOLD);
	if (moon.elevation > 0.0)
		attrset(COLOR_PAIR(1)|A_BOLD);
	else
		attrset(COLOR_PAIR(1));
	mvprintw(row+5,col,"%-7.2fAz",moon.azimuth*180.0/M_PI);
	mvprintw(row+6,col,"%+-6.2f El",moon.elevation*180.0/M_PI);
}

/**
 * Print QTH coordinates in infobox on standard screen. Uses 9 columns and 3 rows.
 *
 * \param row Start row for printing
 * \param col Start column for printing
 * \param qth QTH coordinates
 **/
void PrintQth(int row, int col, predict_observer_t *qth)
{
	attrset(COLOR_PAIR(4)|A_REVERSE|A_BOLD);
	mvprintw(row++,col,"   QTH   ");
	attrset(COLOR_PAIR(2));
	mvprintw(row++,col,"%9s",qth->name);
	char maidenstr[9];
	latlon_to_maidenhead(qth->latitude*180.0/M_PI, qth->longitude*180.0/M_PI, maidenstr);
	mvprintw(row++,col,"%9s",maidenstr);
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
	init_pair(8,COLOR_RED,COLOR_YELLOW);

	if (new_user) {
		QthEdit(qthfile, observer);
		clear();
	}

	predict_julian_date_t curr_time = predict_to_julian(time(NULL));

	//prepare multitrack window
	multitrack_listing_t *listing = multitrack_create_listing(observer, tle_db);

	//window for printing main menu options
	WINDOW *main_menu_win = newwin(MAIN_MENU_OPTS_WIN_HEIGHT, COLS, LINES-MAIN_MENU_OPTS_WIN_HEIGHT, 0);

	refresh();

	/* Display main menu and handle keyboard input */
	int key = 0;
	bool should_run = true;
	int terminal_lines = LINES;
	int terminal_columns = COLS;
	while (should_run) {
		if ((terminal_lines != LINES) || (terminal_columns != COLS)) {
			//force full redraw
			clear();

			//update main menu option window
			mvwin(main_menu_win, LINES-MAIN_MENU_OPTS_WIN_HEIGHT, 0);
			wresize(main_menu_win, MAIN_MENU_OPTS_WIN_HEIGHT, COLS);
			wrefresh(main_menu_win);

			terminal_lines = LINES;
			terminal_columns = COLS;
		}

		curr_time = predict_to_julian(time(NULL));

		//refresh satellite list
		multitrack_update_listing_data(listing, curr_time);
		multitrack_display_listing(listing);
		
		if (!multitrack_search_field_visible(listing->search_field)) {
			PrintMainMenu(main_menu_win);
		}
		PrintSunMoon(listing->window_height + listing->window_row - 7, listing->window_width+1, observer, curr_time);
		PrintQth(listing->window_row, listing->window_width+1, observer);

		//get input character
		refresh();
		halfdelay(HALF_DELAY_TIME);  // Increase if CPU load is too high
		key = getch();
		if (key != -1) {
			cbreak(); //turn off halfdelay

			//handle input to satellite list
			bool handled = multitrack_handle_listing(listing, key);

			//option in submenu has been selected, run satellite specific options
			if (multitrack_option_selector_pop(listing->option_selector)) {
				int option = multitrack_option_selector_get_option(listing->option_selector);
				int satellite_index = multitrack_selected_entry(listing);
				predict_orbital_elements_t *orbital_elements = tle_db_entry_to_orbital_elements(tle_db, satellite_index);
				const char *sat_name = tle_db->tles[satellite_index].name;
				switch (option) {
					case OPTION_SINGLETRACK:
						singletrack(satellite_index, observer, sat_db, tle_db, rotctld, downlink, uplink);
						break;
					case OPTION_PREDICT_VISIBLE:
						Predict(sat_name, orbital_elements, observer, 'v');
						break;
					case OPTION_PREDICT:
						Predict(sat_name, orbital_elements, observer, 'p');
						break;
					case OPTION_DISPLAY_ORBITAL_DATA:
						ShowOrbitData(sat_name, orbital_elements);
						break;
					case OPTION_EDIT_TRANSPONDER:
						EditTransponderDatabase(satellite_index, tle_db, sat_db);
						break;
					case OPTION_SOLAR_ILLUMINATION:
						Illumination(sat_name, orbital_elements);
						break;
				}
				predict_destroy_orbital_elements(orbital_elements);
				clear();
				refresh();
			}

			//handle all other, global options
			if (!handled) {
				if (!handled) {
					switch (key) {
						case 'N':
						case 'n':
							PredictSunMoon(PREDICT_MOON, observer);
							break;

						case 'O':
						case 'o':
							PredictSunMoon(PREDICT_SUN, observer);
							break;

						case 'U':
						case 'u':
							AutoUpdate("", tle_db);
							break;

						case 'M':
						case 'm':
							multitrack_edit_settings(listing);
							break;

						case 'H':
						case 'h':
							multitrack_show_help();
							break;

						case 'G':
						case 'g':
							QthEdit(qthfile, observer);
							multitrack_refresh_tles(listing, tle_db);
							break;

						case 'I':
						case 'i':
							ProgramInfo(qthfile, tle_db, sat_db, rotctld, downlink, uplink);
							break;

						case 'w':
						case 'W':
							EditWhitelist(tle_db, sat_db);
							multitrack_refresh_tles(listing, tle_db);
							break;
						case 'E':
						case 'e':
							EditTransponderDatabase(0, tle_db, sat_db);
							break;
						case 27:
						case 'q':
							should_run = false;
							break;
					}
					clear();
					refresh();
				}
			}
		}
	}

	curs_set(1);
	bkgdset(COLOR_PAIR(1));
	clear();
	refresh();
	endwin();

	delwin(main_menu_win);
	multitrack_destroy_listing(&listing);
}
