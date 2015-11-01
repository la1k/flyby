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

/* Constants used by SGP4/SDP4 code */

#define deg2rad		1.745329251994330E-2	/* Degrees to radians */
#define pi		3.14159265358979323846	/* Pi */
#define pio2		1.57079632679489656	/* Pi/2 */
#define x3pio2		4.71238898038468967	/* 3*Pi/2 */
#define twopi		6.28318530717958623	/* 2*Pi  */
#define e6a		1.0E-6
#define tothrd		6.6666666666666666E-1	/* 2/3 */
#define xj2		1.0826158E-3		/* J2 Harmonic (WGS '72) */
#define xj3		-2.53881E-6		/* J3 Harmonic (WGS '72) */
#define xj4		-1.65597E-6		/* J4 Harmonic (WGS '72) */
#define xke		7.43669161E-2
#define xkmper		6.378137E3		/* WGS 84 Earth radius km */
#define xmnpda		1.44E3			/* Minutes per day */
#define ae		1.0
#define ck2		5.413079E-4
#define ck4		6.209887E-7
#define f		3.35281066474748E-3	/* Flattening factor */
#define ge		3.986008E5 	/* Earth gravitational constant (WGS '72) */
#define s		1.012229
#define qoms2t		1.880279E-09
#define secday		8.6400E4	/* Seconds per day */
#define omega_E		1.00273790934	/* Earth rotations/siderial day */
#define omega_ER	6.3003879	/* Earth rotations, rads/siderial day */
#define zns		1.19459E-5
#define c1ss		2.9864797E-6
#define zes		1.675E-2
#define znl		1.5835218E-4
#define c1l		4.7968065E-7
#define zel		5.490E-2
#define zcosis		9.1744867E-1
#define zsinis		3.9785416E-1
#define zsings		-9.8088458E-1
#define zcosgs		1.945905E-1
#define zcoshs		1
#define zsinhs		0
#define q22		1.7891679E-6
#define q31		2.1460748E-6
#define q33		2.2123015E-7
#define g22		5.7686396
#define g32		9.5240898E-1
#define g44		1.8014998
#define g52		1.0508330
#define g54		4.4108898
#define root22		1.7891679E-6
#define root32		3.7393792E-7
#define root44		7.3636953E-9
#define root52		1.1428639E-7
#define root54		2.1765803E-9
#define thdt		4.3752691E-3
#define rho		1.5696615E-1
#define mfactor		7.292115E-5
#define sr		6.96000E5	/* Solar radius - km (IAU 76) */
#define AU		1.49597870691E8	/* Astronomical unit - km (IAU 76) */

/* Entry points of Deep() */

#define dpinit   1 /* Deep-space initialization code */
#define dpsec    2 /* Deep-space secular code        */
#define dpper    3 /* Deep-space periodic code       */

/* Flow control flag definitions */

#define ALL_FLAGS              -1
#define SGP_INITIALIZED_FLAG   0x000001	/* not used */
#define SGP4_INITIALIZED_FLAG  0x000002
#define SDP4_INITIALIZED_FLAG  0x000004
#define SGP8_INITIALIZED_FLAG  0x000008	/* not used */
#define SDP8_INITIALIZED_FLAG  0x000010	/* not used */
#define SIMPLE_FLAG            0x000020
#define DEEP_SPACE_EPHEM_FLAG  0x000040
#define LUNAR_TERMS_DONE_FLAG  0x000080
#define NEW_EPHEMERIS_FLAG     0x000100	/* not used */
#define DO_LOOP_FLAG           0x000200
#define RESONANCE_FLAG         0x000400
#define SYNCHRONOUS_FLAG       0x000800
#define EPOCH_RESTART_FLAG     0x001000
#define VISIBLE_FLAG           0x002000
#define SAT_ECLIPSED_FLAG      0x004000

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

double	tsince, jul_epoch, jul_utc, eclipse_depth=0,
	sat_azi, sat_ele, sat_range, sat_range_rate,
	sat_lat, sat_lon, sat_alt, sat_vel, phase,
	sun_azi, sun_ele, daynum, fm, fk, age, aostime,
	lostime, ax, ay, az, rx, ry, rz, squint, alat, alon,
	sun_ra, sun_dec, sun_lat, sun_lon, sun_range, sun_range_rate,
	moon_az, moon_el, moon_dx, moon_ra, moon_dec, moon_gha, moon_dv,
	horizon=0.0;

char	qthfile[50], tlefile[50], dbfile[50], temp[80], output[25],
	rotctld_host[256], rotctld_port[6]="4533\0\0",
	uplink_host[256], uplink_port[6]="4532\0\0", uplink_vfo[30],
	downlink_host[256], downlink_port[6]="4532\0\0", downlink_vfo[30],
	resave=0, reload_tle=0, netport[8],
	once_per_second=0, ephem[5], sat_sun_status, findsun,
	calc_squint, database=0, io_lat='N', io_lon='E', maidenstr[9];

int	indx, iaz, iel, ma256, isplat, isplong,
	Flags=0, rotctld_socket, uplink_socket, downlink_socket, totalsats=0;

long	rv, irk;

unsigned char val[256];

/* The following variables are used by the socket server.  They
	are updated in the MultiTrack() and SingleTrack() functions. */

char	visibility_array[maxsats], tracking_mode[30];

float	az_array[maxsats], el_array[maxsats], long_array[maxsats], lat_array[maxsats],
	footprint_array[maxsats], range_array[maxsats], altitude_array[maxsats],
	velocity_array[maxsats], eclipse_depth_array[maxsats], phase_array[maxsats],
  squint_array[maxsats];

double	doppler[maxsats], nextevent[maxsats];

long	aos_array[maxsats], orbitnum_array[maxsats];

unsigned short portbase=0;

/** Type definitions **/

/* Two-line-element satellite orbital data
	structure used directly by the SGP4/SDP4 code. */

typedef struct	{
		   double  epoch, xndt2o, xndd6o, bstar, xincl,
			   xnodeo, eo, omegao, xmo, xno;
		   int	   catnr, elset, revnum;
		   char	   sat_name[25], idesg[9];
		}  tle_t;

/* Geodetic position structure used by SGP4/SDP4 code. */

typedef struct	{
		   double lat, lon, alt, theta;
		}  geodetic_t;

/* General three-dimensional vector structure used by SGP4/SDP4 code. */

typedef struct	{
		   double x, y, z, w;
		}  vector_t;

/* Common arguments between deep-space functions used by SGP4/SDP4 code. */

typedef struct	{
		   	   /* Used by dpinit part of Deep() */
		   double  eosq, sinio, cosio, betao, aodp, theta2,
			   sing, cosg, betao2, xmdot, omgdot, xnodot, xnodp;

			   /* Used by dpsec and dpper parts of Deep() */
		   double  xll, omgadf, xnode, em, xinc, xn, t;

		 	   /* Used by thetg and Deep() */
		   double  ds50;
		}  deep_arg_t;

/* Global structure used by SGP4/SDP4 code. */

geodetic_t obs_geodetic;

/* Two-line Orbital Elements for the satellite used by SGP4/SDP4 code. */

tle_t tle;

/* Functions for testing and setting/clearing flags used in SGP4/SDP4 code */

int isFlagSet(int flag)
{
	return (Flags&flag);
}

int isFlagClear(int flag)
{
	return (~Flags&flag);
}

void SetFlag(int flag)
{
	Flags|=flag;
}

void ClearFlag(int flag)
{
	Flags&=~flag;
}

/* Remaining SGP4/SDP4 code follows... */

int Sign(double arg)
{
	/* Returns sign of a double */

	if (arg>0)
		return 1;
	else if (arg<0)
		return -1;
	else
		return 0;
}

double Sqr(double arg)
{
	/* Returns square of a double */
	return (arg*arg);
}

double Cube(double arg)
{
	/* Returns cube of a double */
	return (arg*arg*arg);
}

double Radians(double arg)
{
	/* Returns angle in radians from argument in degrees */
	return (arg*deg2rad);
}

double Degrees(double arg)
{
	/* Returns angle in degrees from argument in radians */
	return (arg/deg2rad);
}

double ArcSin(double arg)
{
	/* Returns the arcsine of the argument */

	if (fabs(arg)>=1.0)
		return(Sign(arg)*pio2);
	else

	return(atan(arg/sqrt(1.0-arg*arg)));
}

double ArcCos(double arg)
{
	/* Returns arccosine of argument */
	return(pio2-ArcSin(arg));
}

void Magnitude(vector_t *v)
{
	/* Calculates scalar magnitude of a vector_t argument */
	v->w=sqrt(Sqr(v->x)+Sqr(v->y)+Sqr(v->z));
}

void Vec_Add(vector_t *v1, vector_t *v2, vector_t *v3)
{
	/* Adds vectors v1 and v2 together to produce v3 */
	v3->x=v1->x+v2->x;
	v3->y=v1->y+v2->y;
	v3->z=v1->z+v2->z;
	Magnitude(v3);
}

void Vec_Sub(vector_t *v1, vector_t *v2, vector_t *v3)
{
	/* Subtracts vector v2 from v1 to produce v3 */
	v3->x=v1->x-v2->x;
	v3->y=v1->y-v2->y;
	v3->z=v1->z-v2->z;
	Magnitude(v3);
}

void Scalar_Multiply(double k, vector_t *v1, vector_t *v2)
{
	/* Multiplies the vector v1 by the scalar k to produce the vector v2 */
	v2->x=k*v1->x;
	v2->y=k*v1->y;
	v2->z=k*v1->z;
	v2->w=fabs(k)*v1->w;
}

void Scale_Vector(double k, vector_t *v)
{
	/* Multiplies the vector v1 by the scalar k */
	v->x*=k;
	v->y*=k;
	v->z*=k;
	Magnitude(v);
}

double Dot(vector_t *v1, vector_t *v2)
{
	/* Returns the dot product of two vectors */
	return (v1->x*v2->x+v1->y*v2->y+v1->z*v2->z);
}

double Angle(vector_t *v1, vector_t *v2)
{
	/* Calculates the angle between vectors v1 and v2 */
	Magnitude(v1);
	Magnitude(v2);
	return(ArcCos(Dot(v1,v2)/(v1->w*v2->w)));
}

void Cross(vector_t *v1, vector_t *v2 ,vector_t *v3)
{
	/* Produces cross product of v1 and v2, and returns in v3 */
	v3->x=v1->y*v2->z-v1->z*v2->y;
	v3->y=v1->z*v2->x-v1->x*v2->z;
	v3->z=v1->x*v2->y-v1->y*v2->x;
	Magnitude(v3);
}

void Normalize(vector_t *v)
{
	/* Normalizes a vector */
	v->x/=v->w;
	v->y/=v->w;
	v->z/=v->w;
}

double AcTan(double sinx, double cosx)
{
	/* Four-quadrant arctan function */

	if (cosx==0.0) {
		if (sinx>0.0)
			return (pio2);
		else
			return (x3pio2);
	} else {
		if (cosx>0.0) {
			if (sinx>0.0)
				return (atan(sinx/cosx));
			else
				return (twopi+atan(sinx/cosx));
		} else
			return (pi+atan(sinx/cosx));
	}
}

double FMod2p(double x)
{
	/* Returns mod 2PI of argument */

	int i;
	double ret_val;

	ret_val=x;
	i=ret_val/twopi;
	ret_val-=i*twopi;

	if (ret_val<0.0)
		ret_val+=twopi;

	return ret_val;
}

double Modulus(double arg1, double arg2)
{
	/* Returns arg1 mod arg2 */

	int i;
	double ret_val;

	ret_val=arg1;
	i=ret_val/arg2;
	ret_val-=i*arg2;

	if (ret_val<0.0)
		ret_val+=arg2;

	return ret_val;
}

double Frac(double arg)
{
	/* Returns fractional part of double argument */
	return(arg-floor(arg));
}

int Round(double arg)
{
	/* Returns argument rounded up to nearest integer */
	return((int)floor(arg+0.5));
}

double Int(double arg)
{
	/* Returns the floor integer of a double arguement, as double */
	return(floor(arg));
}

void Convert_Sat_State(vector_t *pos, vector_t *vel)
{
	/* Converts the satellite's position and velocity  */
	/* vectors from normalized values to km and km/sec */
	Scale_Vector(xkmper, pos);
	Scale_Vector(xkmper*xmnpda/secday, vel);
}

double Julian_Date_of_Year(double year)
{
	/* The function Julian_Date_of_Year calculates the Julian Date  */
	/* of Day 0.0 of {year}. This function is used to calculate the */
	/* Julian Date of any date by using Julian_Date_of_Year, DOY,   */
	/* and Fraction_of_Day. */

	/* Astronomical Formulae for Calculators, Jean Meeus, */
	/* pages 23-25. Calculate Julian Date of 0.0 Jan year */

	long A, B, i;
	double jdoy;

	year=year-1;
	i=year/100;
	A=i;
	i=A/4;
	B=2-A+i;
	i=365.25*year;
	i+=30.6001*14;
	jdoy=i+1720994.5+B;

	return jdoy;
}

double Julian_Date_of_Epoch(double epoch)
{
	/* The function Julian_Date_of_Epoch returns the Julian Date of     */
	/* an epoch specified in the format used in the NORAD two-line      */
	/* element sets. It has been modified to support dates beyond       */
	/* the year 1999 assuming that two-digit years in the range 00-56   */
	/* correspond to 2000-2056. Until the two-line element set format   */
	/* is changed, it is only valid for dates through 2056 December 31. */

	double year, day;

	/* Modification to support Y2K */
	/* Valid 1957 through 2056     */

	day=modf(epoch*1E-3, &year)*1E3;

	if (year<57)
		year=year+2000;
	else
		year=year+1900;

	return (Julian_Date_of_Year(year)+day);
}

int DOY (int yr, int mo, int dy)
{
	/* The function DOY calculates the day of the year for the specified */
	/* date. The calculation uses the rules for the Gregorian calendar   */
	/* and is valid from the inception of that calendar system.          */

	const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	int i, day;

	day=0;

	for (i=0; i<mo-1; i++ )
	    day+=days[i];

	day=day+dy;

	/* Leap year correction */

	if ((yr%4==0) && ((yr%100!=0) || (yr%400==0)) && (mo>2))
		day++;

	return day;
}

double Fraction_of_Day(int hr, int mi, double se)
{
	/* Fraction_of_Day calculates the fraction of */
	/* a day passed at the specified input time.  */

	double dhr, dmi;

	dhr=(double)hr;
	dmi=(double)mi;

	return ((dhr+(dmi+se/60.0)/60.0)/24.0);
}

double Julian_Date(struct tm *cdate)
{
	/* The function Julian_Date converts a standard calendar   */
	/* date and time to a Julian Date. The procedure Date_Time */
	/* performs the inverse of this function. */

	double julian_date;

	julian_date=Julian_Date_of_Year(cdate->tm_year)+DOY(cdate->tm_year,cdate->tm_mon,cdate->tm_mday)+Fraction_of_Day(cdate->tm_hour,cdate->tm_min,cdate->tm_sec)+5.787037e-06; /* Round up to nearest 1 sec */

	return julian_date;
}

void Date_Time(double julian_date, struct tm *cdate)
{
	/* The function Date_Time() converts a Julian Date to
	standard calendar date and time. The function
	Julian_Date() performs the inverse of this function. */

	time_t jtime;

	jtime=(julian_date-2440587.5)*86400.0;
	*cdate=*gmtime(&jtime);
}

double Delta_ET(double year)
{
	/* The function Delta_ET has been added to allow calculations on   */
	/* the position of the sun.  It provides the difference between UT */
	/* (approximately the same as UTC) and ET (now referred to as TDT).*/
	/* This function is based on a least squares fit of data from 1950 */
	/* to 1991 and will need to be updated periodically. */

	/* Values determined using data from 1950-1991 in the 1990
	Astronomical Almanac.  See DELTA_ET.WQ1 for details. */

	double delta_et;

	delta_et=26.465+0.747622*(year-1950)+1.886913*sin(twopi*(year-1975)/33);

	return delta_et;
}

double ThetaG(double epoch, deep_arg_t *deep_arg)
{
	/* The function ThetaG calculates the Greenwich Mean Sidereal Time */
	/* for an epoch specified in the format used in the NORAD two-line */
	/* element sets. It has now been adapted for dates beyond the year */
	/* 1999, as described above. The function ThetaG_JD provides the   */
	/* same calculation except that it is based on an input in the     */
	/* form of a Julian Date. */

	/* Reference:  The 1992 Astronomical Almanac, page B6. */

	double year, day, UT, jd, TU, GMST, ThetaG;

	/* Modification to support Y2K */
	/* Valid 1957 through 2056     */

	day=modf(epoch*1E-3,&year)*1E3;

	if (year<57)
		year+=2000;
	else
		year+=1900;

	UT=modf(day,&day);
	jd=Julian_Date_of_Year(year)+day;
	TU=(jd-2451545.0)/36525;
	GMST=24110.54841+TU*(8640184.812866+TU*(0.093104-TU*6.2E-6));
	GMST=Modulus(GMST+secday*omega_E*UT,secday);
	ThetaG=twopi*GMST/secday;
	deep_arg->ds50=jd-2433281.5+UT;
	ThetaG=FMod2p(6.3003880987*deep_arg->ds50+1.72944494);

	return ThetaG;
}

double ThetaG_JD(double jd)
{
	/* Reference:  The 1992 Astronomical Almanac, page B6. */

	double UT, TU, GMST;

	UT=Frac(jd+0.5);
	jd=jd-UT;
	TU=(jd-2451545.0)/36525;
	GMST=24110.54841+TU*(8640184.812866+TU*(0.093104-TU*6.2E-6));
	GMST=Modulus(GMST+secday*omega_E*UT,secday);

	return (twopi*GMST/secday);
}

void Calculate_Solar_Position(double time, vector_t *solar_vector)
{
	/* Calculates solar position vector */

	double mjd, year, T, M, L, e, C, O, Lsa, nu, R, eps;

	mjd=time-2415020.0;
	year=1900+mjd/365.25;
	T=(mjd+Delta_ET(year)/secday)/36525.0;
	M=Radians(Modulus(358.47583+Modulus(35999.04975*T,360.0)-(0.000150+0.0000033*T)*Sqr(T),360.0));
	L=Radians(Modulus(279.69668+Modulus(36000.76892*T,360.0)+0.0003025*Sqr(T),360.0));
	e=0.01675104-(0.0000418+0.000000126*T)*T;
	C=Radians((1.919460-(0.004789+0.000014*T)*T)*sin(M)+(0.020094-0.000100*T)*sin(2*M)+0.000293*sin(3*M));
	O=Radians(Modulus(259.18-1934.142*T,360.0));
	Lsa=Modulus(L+C-Radians(0.00569-0.00479*sin(O)),twopi);
	nu=Modulus(M+C,twopi);
	R=1.0000002*(1.0-Sqr(e))/(1.0+e*cos(nu));
	eps=Radians(23.452294-(0.0130125+(0.00000164-0.000000503*T)*T)*T+0.00256*cos(O));
	R=AU*R;
	solar_vector->x=R*cos(Lsa);
	solar_vector->y=R*sin(Lsa)*cos(eps);
	solar_vector->z=R*sin(Lsa)*sin(eps);
	solar_vector->w=R;
}

int Sat_Eclipsed(vector_t *pos, vector_t *sol, double *depth)
{
	/* Calculates stellite's eclipse status and depth */

	double sd_sun, sd_earth, delta;
	vector_t Rho, earth;

	/* Determine partial eclipse */

	sd_earth=ArcSin(xkmper/pos->w);
	Vec_Sub(sol,pos,&Rho);
	sd_sun=ArcSin(sr/Rho.w);
	Scalar_Multiply(-1,pos,&earth);
	delta=Angle(sol,&earth);
	*depth=sd_earth-sd_sun-delta;

	if (sd_earth<sd_sun)
		return 0;
	else
		if (*depth>=0)
			return 1;
		else
			return 0;
}

void select_ephemeris(tle_t *tle)
{
	/* Selects the apropriate ephemeris type to be used */
	/* for predictions according to the data in the TLE */
	/* It also processes values in the tle set so that  */
	/* they are apropriate for the sgp4/sdp4 routines   */

	double ao, xnodp, dd1, dd2, delo, temp, a1, del1, r1;

	/* Preprocess tle set */
	tle->xnodeo*=deg2rad;
	tle->omegao*=deg2rad;
	tle->xmo*=deg2rad;
	tle->xincl*=deg2rad;
	temp=twopi/xmnpda/xmnpda;
	tle->xno=tle->xno*temp*xmnpda;
	tle->xndt2o*=temp;
	tle->xndd6o=tle->xndd6o*temp/xmnpda;
	tle->bstar/=ae;

	/* Period > 225 minutes is deep space */
	dd1=(xke/tle->xno);
	dd2=tothrd;
	a1=pow(dd1,dd2);
	r1=cos(tle->xincl);
	dd1=(1.0-tle->eo*tle->eo);
	temp=ck2*1.5f*(r1*r1*3.0-1.0)/pow(dd1,1.5);
	del1=temp/(a1*a1);
	ao=a1*(1.0-del1*(tothrd*.5+del1*(del1*1.654320987654321+1.0)));
	delo=temp/(ao*ao);
	xnodp=tle->xno/(delo+1.0);

	/* Select a deep-space/near-earth ephemeris */
	if (twopi/xnodp/xmnpda>=0.15625)
		SetFlag(DEEP_SPACE_EPHEM_FLAG);
	else
		ClearFlag(DEEP_SPACE_EPHEM_FLAG);
}

void SGP4(double tsince, tle_t * tle, vector_t * pos, vector_t * vel)
{
	/* This function is used to calculate the position and velocity */
	/* of near-earth (period < 225 minutes) satellites. tsince is   */
	/* time since epoch in minutes, tle is a pointer to a tle_t     */
	/* structure with Keplerian orbital elements and pos and vel    */
	/* are vector_t structures returning ECI satellite position and */
	/* velocity. Use Convert_Sat_State() to convert to km and km/s. */

	static double aodp, aycof, c1, c4, c5, cosio, d2, d3, d4, delmo,
	omgcof, eta, omgdot, sinio, xnodp, sinmo, t2cof, t3cof, t4cof,
	t5cof, x1mth2, x3thm1, x7thm1, xmcof, xmdot, xnodcf, xnodot, xlcof;

	double cosuk, sinuk, rfdotk, vx, vy, vz, ux, uy, uz, xmy, xmx, cosnok,
	sinnok, cosik, sinik, rdotk, xinck, xnodek, uk, rk, cos2u, sin2u,
	u, sinu, cosu, betal, rfdot, rdot, r, pl, elsq, esine, ecose, epw,
	cosepw, x1m5th, xhdot1, tfour, sinepw, capu, ayn, xlt, aynl, xll,
	axn, xn, beta, xl, e, a, tcube, delm, delomg, templ, tempe, tempa,
	xnode, tsq, xmp, omega, xnoddf, omgadf, xmdf, a1, a3ovk2, ao,
	betao, betao2, c1sq, c2, c3, coef, coef1, del1, delo, eeta, eosq,
	etasq, perigee, pinvsq, psisq, qoms24, s4, temp, temp1, temp2,
	temp3, temp4, temp5, temp6, theta2, theta4, tsi;

	int i;

	/* Initialization */

	if (isFlagClear(SGP4_INITIALIZED_FLAG)) {
		SetFlag(SGP4_INITIALIZED_FLAG);

		/* Recover original mean motion (xnodp) and   */
		/* semimajor axis (aodp) from input elements. */

		a1=pow(xke/tle->xno,tothrd);
		cosio=cos(tle->xincl);
		theta2=cosio*cosio;
		x3thm1=3*theta2-1.0;
		eosq=tle->eo*tle->eo;
		betao2=1.0-eosq;
		betao=sqrt(betao2);
		del1=1.5*ck2*x3thm1/(a1*a1*betao*betao2);
		ao=a1*(1.0-del1*(0.5*tothrd+del1*(1.0+134.0/81.0*del1)));
		delo=1.5*ck2*x3thm1/(ao*ao*betao*betao2);
		xnodp=tle->xno/(1.0+delo);
		aodp=ao/(1.0-delo);

		/* For perigee less than 220 kilometers, the "simple"     */
		/* flag is set and the equations are truncated to linear  */
		/* variation in sqrt a and quadratic variation in mean    */
		/* anomaly.  Also, the c3 term, the delta omega term, and */
		/* the delta m term are dropped.                          */

		if ((aodp*(1-tle->eo)/ae)<(220/xkmper+ae))
		    SetFlag(SIMPLE_FLAG);
		else
		    ClearFlag(SIMPLE_FLAG);

		/* For perigees below 156 km, the      */
		/* values of s and qoms2t are altered. */

		s4=s;
		qoms24=qoms2t;
		perigee=(aodp*(1-tle->eo)-ae)*xkmper;

		if (perigee<156.0) {
			if (perigee<=98.0)
				s4=20;
			else
				s4=perigee-78.0;

			qoms24=pow((120-s4)*ae/xkmper,4);
			s4=s4/xkmper+ae;
		}

		pinvsq=1/(aodp*aodp*betao2*betao2);
		tsi=1/(aodp-s4);
		eta=aodp*tle->eo*tsi;
		etasq=eta*eta;
		eeta=tle->eo*eta;
		psisq=fabs(1-etasq);
		coef=qoms24*pow(tsi,4);
		coef1=coef/pow(psisq,3.5);
		c2=coef1*xnodp*(aodp*(1+1.5*etasq+eeta*(4+etasq))+0.75*ck2*tsi/psisq*x3thm1*(8+3*etasq*(8+etasq)));
		c1=tle->bstar*c2;
		sinio=sin(tle->xincl);
		a3ovk2=-xj3/ck2*pow(ae,3);
		c3=coef*tsi*a3ovk2*xnodp*ae*sinio/tle->eo;
		x1mth2=1-theta2;

		c4=2*xnodp*coef1*aodp*betao2*(eta*(2+0.5*etasq)+tle->eo*(0.5+2*etasq)-2*ck2*tsi/(aodp*psisq)*(-3*x3thm1*(1-2*eeta+etasq*(1.5-0.5*eeta))+0.75*x1mth2*(2*etasq-eeta*(1+etasq))*cos(2*tle->omegao)));
		c5=2*coef1*aodp*betao2*(1+2.75*(etasq+eeta)+eeta*etasq);

		theta4=theta2*theta2;
		temp1=3*ck2*pinvsq*xnodp;
		temp2=temp1*ck2*pinvsq;
		temp3=1.25*ck4*pinvsq*pinvsq*xnodp;
		xmdot=xnodp+0.5*temp1*betao*x3thm1+0.0625*temp2*betao*(13-78*theta2+137*theta4);
		x1m5th=1-5*theta2;
		omgdot=-0.5*temp1*x1m5th+0.0625*temp2*(7-114*theta2+395*theta4)+temp3*(3-36*theta2+49*theta4);
		xhdot1=-temp1*cosio;
		xnodot=xhdot1+(0.5*temp2*(4-19*theta2)+2*temp3*(3-7*theta2))*cosio;
		omgcof=tle->bstar*c3*cos(tle->omegao);
		xmcof=-tothrd*coef*tle->bstar*ae/eeta;
		xnodcf=3.5*betao2*xhdot1*c1;
		t2cof=1.5*c1;
		xlcof=0.125*a3ovk2*sinio*(3+5*cosio)/(1+cosio);
		aycof=0.25*a3ovk2*sinio;
		delmo=pow(1+eta*cos(tle->xmo),3);
		sinmo=sin(tle->xmo);
		x7thm1=7*theta2-1;

		if (isFlagClear(SIMPLE_FLAG)) {
			c1sq=c1*c1;
			d2=4*aodp*tsi*c1sq;
			temp=d2*tsi*c1/3;
			d3=(17*aodp+s4)*temp;
			d4=0.5*temp*aodp*tsi*(221*aodp+31*s4)*c1;
			t3cof=d2+2*c1sq;
			t4cof=0.25*(3*d3+c1*(12*d2+10*c1sq));
			t5cof=0.2*(3*d4+12*c1*d3+6*d2*d2+15*c1sq*(2*d2+c1sq));
		}
	}

	/* Update for secular gravity and atmospheric drag. */
	xmdf=tle->xmo+xmdot*tsince;
	omgadf=tle->omegao+omgdot*tsince;
	xnoddf=tle->xnodeo+xnodot*tsince;
	omega=omgadf;
	xmp=xmdf;
	tsq=tsince*tsince;
	xnode=xnoddf+xnodcf*tsq;
	tempa=1-c1*tsince;
	tempe=tle->bstar*c4*tsince;
	templ=t2cof*tsq;

	if (isFlagClear(SIMPLE_FLAG)) {
		delomg=omgcof*tsince;
		delm=xmcof*(pow(1+eta*cos(xmdf),3)-delmo);
		temp=delomg+delm;
		xmp=xmdf+temp;
		omega=omgadf-temp;
		tcube=tsq*tsince;
		tfour=tsince*tcube;
		tempa=tempa-d2*tsq-d3*tcube-d4*tfour;
		tempe=tempe+tle->bstar*c5*(sin(xmp)-sinmo);
		templ=templ+t3cof*tcube+tfour*(t4cof+tsince*t5cof);
	}

	a=aodp*pow(tempa,2);
	e=tle->eo-tempe;
	xl=xmp+omega+xnode+xnodp*templ;
	beta=sqrt(1-e*e);
	xn=xke/pow(a,1.5);

	/* Long period periodics */
	axn=e*cos(omega);
	temp=1/(a*beta*beta);
	xll=temp*xlcof*axn;
	aynl=temp*aycof;
	xlt=xl+xll;
	ayn=e*sin(omega)+aynl;

	/* Solve Kepler's Equation */
	capu=FMod2p(xlt-xnode);
	temp2=capu;
	i=0;

	do {
		sinepw=sin(temp2);
		cosepw=cos(temp2);
		temp3=axn*sinepw;
		temp4=ayn*cosepw;
		temp5=axn*cosepw;
		temp6=ayn*sinepw;
		epw=(capu-temp4+temp3-temp2)/(1-temp5-temp6)+temp2;

		if (fabs(epw-temp2)<= e6a)
			break;

		temp2=epw;

	} while (i++<10);

	/* Short period preliminary quantities */
	ecose=temp5+temp6;
	esine=temp3-temp4;
	elsq=axn*axn+ayn*ayn;
	temp=1-elsq;
	pl=a*temp;
	r=a*(1-ecose);
	temp1=1/r;
	rdot=xke*sqrt(a)*esine*temp1;
	rfdot=xke*sqrt(pl)*temp1;
	temp2=a*temp1;
	betal=sqrt(temp);
	temp3=1/(1+betal);
	cosu=temp2*(cosepw-axn+ayn*esine*temp3);
	sinu=temp2*(sinepw-ayn-axn*esine*temp3);
	u=AcTan(sinu,cosu);
	sin2u=2*sinu*cosu;
	cos2u=2*cosu*cosu-1;
	temp=1/pl;
	temp1=ck2*temp;
	temp2=temp1*temp;

	/* Update for short periodics */
	rk=r*(1-1.5*temp2*betal*x3thm1)+0.5*temp1*x1mth2*cos2u;
	uk=u-0.25*temp2*x7thm1*sin2u;
	xnodek=xnode+1.5*temp2*cosio*sin2u;
	xinck=tle->xincl+1.5*temp2*cosio*sinio*cos2u;
	rdotk=rdot-xn*temp1*x1mth2*sin2u;
	rfdotk=rfdot+xn*temp1*(x1mth2*cos2u+1.5*x3thm1);

	/* Orientation vectors */
	sinuk=sin(uk);
	cosuk=cos(uk);
	sinik=sin(xinck);
	cosik=cos(xinck);
	sinnok=sin(xnodek);
	cosnok=cos(xnodek);
	xmx=-sinnok*cosik;
	xmy=cosnok*cosik;
	ux=xmx*sinuk+cosnok*cosuk;
	uy=xmy*sinuk+sinnok*cosuk;
	uz=sinik*sinuk;
	vx=xmx*cosuk-cosnok*sinuk;
	vy=xmy*cosuk-sinnok*sinuk;
	vz=sinik*cosuk;

	/* Position and velocity */
	pos->x=rk*ux;
	pos->y=rk*uy;
	pos->z=rk*uz;
	vel->x=rdotk*ux+rfdotk*vx;
	vel->y=rdotk*uy+rfdotk*vy;
	vel->z=rdotk*uz+rfdotk*vz;

	/* Phase in radians */
	phase=xlt-xnode-omgadf+twopi;

	if (phase<0.0)
		phase+=twopi;

	phase=FMod2p(phase);
}

void Deep(int ientry, tle_t * tle, deep_arg_t * deep_arg)
{
	/* This function is used by SDP4 to add lunar and solar */
	/* perturbation effects to deep-space orbit objects.    */

	static double thgr, xnq, xqncl, omegaq, zmol, zmos, savtsn, ee2, e3,
	xi2, xl2, xl3, xl4, xgh2, xgh3, xgh4, xh2, xh3, sse, ssi, ssg, xi3,
	se2, si2, sl2, sgh2, sh2, se3, si3, sl3, sgh3, sh3, sl4, sgh4, ssl,
	ssh, d3210, d3222, d4410, d4422, d5220, d5232, d5421, d5433, del1,
	del2, del3, fasx2, fasx4, fasx6, xlamo, xfact, xni, atime, stepp,
	stepn, step2, preep, pl, sghs, xli, d2201, d2211, sghl, sh1, pinc,
	pe, shs, zsingl, zcosgl, zsinhl, zcoshl, zsinil, zcosil;

	double a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, ainv2, alfdp, aqnv,
	sgh, sini2, sinis, sinok, sh, si, sil, day, betdp, dalf, bfact, c,
	cc, cosis, cosok, cosq, ctem, f322, zx, zy, dbet, dls, eoc, eq, f2,
	f220, f221, f3, f311, f321, xnoh, f330, f441, f442, f522, f523,
	f542, f543, g200, g201, g211, pgh, ph, s1, s2, s3, s4, s5, s6, s7,
	se, sel, ses, xls, g300, g310, g322, g410, g422, g520, g521, g532,
	g533, gam, sinq, sinzf, sis, sl, sll, sls, stem, temp, temp1, x1,
	x2, x2li, x2omi, x3, x4, x5, x6, x7, x8, xl, xldot, xmao, xnddt,
	xndot, xno2, xnodce, xnoi, xomi, xpidot, z1, z11, z12, z13, z2,
	z21, z22, z23, z3, z31, z32, z33, ze, zf, zm, zn, zsing,
	zsinh, zsini, zcosg, zcosh, zcosi, delt=0, ft=0;

	switch (ientry) {
		case dpinit:  /* Entrance for deep space initialization */
		thgr=ThetaG(tle->epoch,deep_arg);
		eq=tle->eo;
		xnq=deep_arg->xnodp;
		aqnv=1/deep_arg->aodp;
		xqncl=tle->xincl;
		xmao=tle->xmo;
		xpidot=deep_arg->omgdot+deep_arg->xnodot;
		sinq=sin(tle->xnodeo);
		cosq=cos(tle->xnodeo);
		omegaq=tle->omegao;

		/* Initialize lunar solar terms */
		day=deep_arg->ds50+18261.5;  /* Days since 1900 Jan 0.5 */

		if (day!=preep) {
			preep=day;
			xnodce=4.5236020-9.2422029E-4*day;
			stem=sin(xnodce);
			ctem=cos(xnodce);
			zcosil=0.91375164-0.03568096*ctem;
			zsinil=sqrt(1-zcosil*zcosil);
			zsinhl=0.089683511*stem/zsinil;
			zcoshl=sqrt(1-zsinhl*zsinhl);
			c=4.7199672+0.22997150*day;
			gam=5.8351514+0.0019443680*day;
			zmol=FMod2p(c-gam);
			zx=0.39785416*stem/zsinil;
			zy=zcoshl*ctem+0.91744867*zsinhl*stem;
			zx=AcTan(zx,zy);
			zx=gam+zx-xnodce;
			zcosgl=cos(zx);
			zsingl=sin(zx);
			zmos=6.2565837+0.017201977*day;
			zmos=FMod2p(zmos);
		}

		/* Do solar terms */
		savtsn=1E20;
		zcosg=zcosgs;
		zsing=zsings;
		zcosi=zcosis;
		zsini=zsinis;
		zcosh=cosq;
		zsinh= sinq;
		cc=c1ss;
		zn=zns;
		ze=zes;
		xnoi=1/xnq;

		/* Loop breaks when Solar terms are done a second */
		/* time, after Lunar terms are initialized        */

		for (;;) {
			/* Solar terms done again after Lunar terms are done */
			a1=zcosg*zcosh+zsing*zcosi*zsinh;
			a3=-zsing*zcosh+zcosg*zcosi*zsinh;
			a7=-zcosg*zsinh+zsing*zcosi*zcosh;
			a8=zsing*zsini;
			a9=zsing*zsinh+zcosg*zcosi*zcosh;
			a10=zcosg*zsini;
			a2=deep_arg->cosio*a7+deep_arg->sinio*a8;
			a4=deep_arg->cosio*a9+deep_arg->sinio*a10;
			a5=-deep_arg->sinio*a7+deep_arg->cosio*a8;
			a6=-deep_arg->sinio*a9+deep_arg->cosio*a10;
			x1=a1*deep_arg->cosg+a2*deep_arg->sing;
			x2=a3*deep_arg->cosg+a4*deep_arg->sing;
			x3=-a1*deep_arg->sing+a2*deep_arg->cosg;
			x4=-a3*deep_arg->sing+a4*deep_arg->cosg;
			x5=a5*deep_arg->sing;
			x6=a6*deep_arg->sing;
			x7=a5*deep_arg->cosg;
			x8=a6*deep_arg->cosg;
			z31=12*x1*x1-3*x3*x3;
			z32=24*x1*x2-6*x3*x4;
			z33=12*x2*x2-3*x4*x4;
			z1=3*(a1*a1+a2*a2)+z31*deep_arg->eosq;
			z2=6*(a1*a3+a2*a4)+z32*deep_arg->eosq;
			z3=3*(a3*a3+a4*a4)+z33*deep_arg->eosq;
			z11=-6*a1*a5+deep_arg->eosq*(-24*x1*x7-6*x3*x5);
			z12=-6*(a1*a6+a3*a5)+deep_arg->eosq*(-24*(x2*x7+x1*x8)-6*(x3*x6+x4*x5));
			z13=-6*a3*a6+deep_arg->eosq*(-24*x2*x8-6*x4*x6);
			z21=6*a2*a5+deep_arg->eosq*(24*x1*x5-6*x3*x7);
			z22=6*(a4*a5+a2*a6)+deep_arg->eosq*(24*(x2*x5+x1*x6)-6*(x4*x7+x3*x8));
			z23=6*a4*a6+deep_arg->eosq*(24*x2*x6-6*x4*x8);
			z1=z1+z1+deep_arg->betao2*z31;
			z2=z2+z2+deep_arg->betao2*z32;
			z3=z3+z3+deep_arg->betao2*z33;
			s3=cc*xnoi;
			s2=-0.5*s3/deep_arg->betao;
			s4=s3*deep_arg->betao;
			s1=-15*eq*s4;
			s5=x1*x3+x2*x4;
			s6=x2*x3+x1*x4;
			s7=x2*x4-x1*x3;
			se=s1*zn*s5;
			si=s2*zn*(z11+z13);
			sl=-zn*s3*(z1+z3-14-6*deep_arg->eosq);
			sgh=s4*zn*(z31+z33-6);
			sh=-zn*s2*(z21+z23);

			if (xqncl<5.2359877E-2)
				sh=0;

			ee2=2*s1*s6;
			e3=2*s1*s7;
			xi2=2*s2*z12;
			xi3=2*s2*(z13-z11);
			xl2=-2*s3*z2;
			xl3=-2*s3*(z3-z1);
			xl4=-2*s3*(-21-9*deep_arg->eosq)*ze;
			xgh2=2*s4*z32;
			xgh3=2*s4*(z33-z31);
			xgh4=-18*s4*ze;
			xh2=-2*s2*z22;
			xh3=-2*s2*(z23-z21);

			if (isFlagSet(LUNAR_TERMS_DONE_FLAG))
				break;

			/* Do lunar terms */
			sse=se;
			ssi=si;
			ssl=sl;
			ssh=sh/deep_arg->sinio;
			ssg=sgh-deep_arg->cosio*ssh;
			se2=ee2;
			si2=xi2;
			sl2=xl2;
			sgh2=xgh2;
			sh2=xh2;
			se3=e3;
			si3=xi3;
			sl3=xl3;
			sgh3=xgh3;
			sh3=xh3;
			sl4=xl4;
			sgh4=xgh4;
			zcosg=zcosgl;
			zsing=zsingl;
			zcosi=zcosil;
			zsini=zsinil;
			zcosh=zcoshl*cosq+zsinhl*sinq;
			zsinh=sinq*zcoshl-cosq*zsinhl;
			zn=znl;
			cc=c1l;
			ze=zel;
			SetFlag(LUNAR_TERMS_DONE_FLAG);
		}

		sse=sse+se;
		ssi=ssi+si;
		ssl=ssl+sl;
		ssg=ssg+sgh-deep_arg->cosio/deep_arg->sinio*sh;
		ssh=ssh+sh/deep_arg->sinio;

		/* Geopotential resonance initialization for 12 hour orbits */
		ClearFlag(RESONANCE_FLAG);
		ClearFlag(SYNCHRONOUS_FLAG);

		if (!((xnq<0.0052359877) && (xnq>0.0034906585))) {
			if ((xnq<0.00826) || (xnq>0.00924))
			    return;

			if (eq<0.5)
			    return;

			SetFlag(RESONANCE_FLAG);
			eoc=eq*deep_arg->eosq;
			g201=-0.306-(eq-0.64)*0.440;

			if (eq<=0.65) {
				g211=3.616-13.247*eq+16.290*deep_arg->eosq;
				g310=-19.302+117.390*eq-228.419*deep_arg->eosq+156.591*eoc;
				g322=-18.9068+109.7927*eq-214.6334*deep_arg->eosq+146.5816*eoc;
				g410=-41.122+242.694*eq-471.094*deep_arg->eosq+313.953*eoc;
				g422=-146.407+841.880*eq-1629.014*deep_arg->eosq+1083.435 * eoc;
				g520=-532.114+3017.977*eq-5740*deep_arg->eosq+3708.276*eoc;
			} else {
				g211=-72.099+331.819*eq-508.738*deep_arg->eosq+266.724*eoc;
				g310=-346.844+1582.851*eq-2415.925*deep_arg->eosq+1246.113*eoc;
				g322=-342.585+1554.908*eq-2366.899*deep_arg->eosq+1215.972*eoc;
				g410=-1052.797+4758.686*eq-7193.992*deep_arg->eosq+3651.957*eoc;
				g422=-3581.69+16178.11*eq-24462.77*deep_arg->eosq+12422.52*eoc;

				if (eq<=0.715)
					g520=1464.74-4664.75*eq+3763.64*deep_arg->eosq;
				else
					g520=-5149.66+29936.92*eq-54087.36*deep_arg->eosq+31324.56*eoc;
			}

			if (eq<0.7) {
				g533=-919.2277+4988.61*eq-9064.77*deep_arg->eosq+5542.21*eoc;
				g521=-822.71072+4568.6173*eq-8491.4146*deep_arg->eosq+5337.524*eoc;
				g532=-853.666+4690.25*eq-8624.77*deep_arg->eosq+5341.4*eoc;
			} else {
				g533=-37995.78+161616.52*eq-229838.2*deep_arg->eosq+109377.94*eoc;
				g521 =-51752.104+218913.95*eq-309468.16*deep_arg->eosq+146349.42*eoc;
				g532 =-40023.88+170470.89*eq-242699.48*deep_arg->eosq+115605.82*eoc;
			}

			sini2=deep_arg->sinio*deep_arg->sinio;
			f220=0.75*(1+2*deep_arg->cosio+deep_arg->theta2);
			f221=1.5*sini2;
			f321=1.875*deep_arg->sinio*(1-2*deep_arg->cosio-3*deep_arg->theta2);
			f322=-1.875*deep_arg->sinio*(1+2*deep_arg->cosio-3*deep_arg->theta2);
			f441=35*sini2*f220;
			f442=39.3750*sini2*sini2;
			f522=9.84375*deep_arg->sinio*(sini2*(1-2*deep_arg->cosio-5*deep_arg->theta2)+0.33333333*(-2+4*deep_arg->cosio+6*deep_arg->theta2));
			f523=deep_arg->sinio*(4.92187512*sini2*(-2-4*deep_arg->cosio+10*deep_arg->theta2)+6.56250012*(1+2*deep_arg->cosio-3*deep_arg->theta2));
			f542=29.53125*deep_arg->sinio*(2-8*deep_arg->cosio+deep_arg->theta2*(-12+8*deep_arg->cosio+10*deep_arg->theta2));
			f543=29.53125*deep_arg->sinio*(-2-8*deep_arg->cosio+deep_arg->theta2*(12+8*deep_arg->cosio-10*deep_arg->theta2));
			xno2=xnq*xnq;
			ainv2=aqnv*aqnv;
			temp1=3*xno2*ainv2;
			temp=temp1*root22;
			d2201=temp*f220*g201;
			d2211=temp*f221*g211;
			temp1=temp1*aqnv;
			temp=temp1*root32;
			d3210=temp*f321*g310;
			d3222=temp*f322*g322;
			temp1=temp1*aqnv;
			temp=2*temp1*root44;
			d4410=temp*f441*g410;
			d4422=temp*f442*g422;
			temp1=temp1*aqnv;
			temp=temp1*root52;
			d5220=temp*f522*g520;
			d5232=temp*f523*g532;
			temp=2*temp1*root54;
			d5421=temp*f542*g521;
			d5433=temp*f543*g533;
			xlamo=xmao+tle->xnodeo+tle->xnodeo-thgr-thgr;
			bfact=deep_arg->xmdot+deep_arg->xnodot+deep_arg->xnodot-thdt-thdt;
			bfact=bfact+ssl+ssh+ssh;
		} else {
			SetFlag(RESONANCE_FLAG);
			SetFlag(SYNCHRONOUS_FLAG);

			/* Synchronous resonance terms initialization */
			g200=1+deep_arg->eosq*(-2.5+0.8125*deep_arg->eosq);
			g310=1+2*deep_arg->eosq;
			g300=1+deep_arg->eosq*(-6+6.60937*deep_arg->eosq);
			f220=0.75*(1+deep_arg->cosio)*(1+deep_arg->cosio);
			f311=0.9375*deep_arg->sinio*deep_arg->sinio*(1+3*deep_arg->cosio)-0.75*(1+deep_arg->cosio);
			f330=1+deep_arg->cosio;
			f330=1.875*f330*f330*f330;
			del1=3*xnq*xnq*aqnv*aqnv;
			del2=2*del1*f220*g200*q22;
			del3=3*del1*f330*g300*q33*aqnv;
			del1=del1*f311*g310*q31*aqnv;
			fasx2=0.13130908;
			fasx4=2.8843198;
			fasx6=0.37448087;
			xlamo=xmao+tle->xnodeo+tle->omegao-thgr;
			bfact=deep_arg->xmdot+xpidot-thdt;
			bfact=bfact+ssl+ssg+ssh;
		}

		xfact=bfact-xnq;

		/* Initialize integrator */
		xli=xlamo;
		 xni=xnq;
		atime=0;
		stepp=720;
		stepn=-720;
		step2=259200;

		return;

		case dpsec:  /* Entrance for deep space secular effects */
		deep_arg->xll=deep_arg->xll+ssl*deep_arg->t;
		deep_arg->omgadf=deep_arg->omgadf+ssg*deep_arg->t;
		deep_arg->xnode=deep_arg->xnode+ssh*deep_arg->t;
		deep_arg->em=tle->eo+sse*deep_arg->t;
		deep_arg->xinc=tle->xincl+ssi*deep_arg->t;

		if (deep_arg->xinc<0) {
			deep_arg->xinc=-deep_arg->xinc;
			deep_arg->xnode=deep_arg->xnode+pi;
			deep_arg->omgadf=deep_arg->omgadf-pi;
		}

		if (isFlagClear(RESONANCE_FLAG))
		      return;

		do {
			if ((atime==0) || ((deep_arg->t>=0) && (atime<0)) || ((deep_arg->t<0) && (atime>=0))) {
				/* Epoch restart */

				if (deep_arg->t>=0)
					delt=stepp;
				else
					delt=stepn;

				atime=0;
				xni=xnq;
				xli=xlamo;
			} else {
				if (fabs(deep_arg->t)>=fabs(atime)) {
					if (deep_arg->t>0)
						delt=stepp;
					else
						delt=stepn;
				}
			}

			do {
				if (fabs(deep_arg->t-atime)>=stepp) {
					SetFlag(DO_LOOP_FLAG);
					ClearFlag(EPOCH_RESTART_FLAG);
				} else {
					ft=deep_arg->t-atime;
					ClearFlag(DO_LOOP_FLAG);
				}

				if (fabs(deep_arg->t)<fabs(atime)) {
					if (deep_arg->t>=0)
						delt=stepn;
					else
						delt=stepp;

					SetFlag(DO_LOOP_FLAG | EPOCH_RESTART_FLAG);
				}

				/* Dot terms calculated */
				if (isFlagSet(SYNCHRONOUS_FLAG)) {
					xndot=del1*sin(xli-fasx2)+del2*sin(2*(xli-fasx4))+del3*sin(3*(xli-fasx6));
					xnddt=del1*cos(xli-fasx2)+2*del2*cos(2*(xli-fasx4))+3*del3*cos(3*(xli-fasx6));
				} else {
					xomi=omegaq+deep_arg->omgdot*atime;
					x2omi=xomi+xomi;
					x2li=xli+xli;
					xndot=d2201*sin(x2omi+xli-g22)+d2211*sin(xli-g22)+d3210*sin(xomi+xli-g32)+d3222*sin(-xomi+xli-g32)+d4410*sin(x2omi+x2li-g44)+d4422*sin(x2li-g44)+d5220*sin(xomi+xli-g52)+d5232*sin(-xomi+xli-g52)+d5421*sin(xomi+x2li-g54)+d5433*sin(-xomi+x2li-g54);
					xnddt=d2201*cos(x2omi+xli-g22)+d2211*cos(xli-g22)+d3210*cos(xomi+xli-g32)+d3222*cos(-xomi+xli-g32)+d5220*cos(xomi+xli-g52)+d5232*cos(-xomi+xli-g52)+2*(d4410*cos(x2omi+x2li-g44)+d4422*cos(x2li-g44)+d5421*cos(xomi+x2li-g54)+d5433*cos(-xomi+x2li-g54));
				}

				xldot=xni+xfact;
				xnddt=xnddt*xldot;

				if (isFlagSet(DO_LOOP_FLAG)) {
					xli=xli+xldot*delt+xndot*step2;
					xni=xni+xndot*delt+xnddt*step2;
					atime=atime+delt;
				}
			} while (isFlagSet(DO_LOOP_FLAG) && isFlagClear(EPOCH_RESTART_FLAG));
		} while (isFlagSet(DO_LOOP_FLAG) && isFlagSet(EPOCH_RESTART_FLAG));

		deep_arg->xn=xni+xndot*ft+xnddt*ft*ft*0.5;
		xl=xli+xldot*ft+xndot*ft*ft*0.5;
		temp=-deep_arg->xnode+thgr+deep_arg->t*thdt;

		if (isFlagClear(SYNCHRONOUS_FLAG))
			deep_arg->xll=xl+temp+temp;
		else
			deep_arg->xll=xl-deep_arg->omgadf+temp;

		return;

		case dpper:	 /* Entrance for lunar-solar periodics */
		sinis=sin(deep_arg->xinc);
		cosis=cos(deep_arg->xinc);

		if (fabs(savtsn-deep_arg->t)>=30) {
			savtsn=deep_arg->t;
			zm=zmos+zns*deep_arg->t;
			zf=zm+2*zes*sin(zm);
			sinzf=sin(zf);
			f2=0.5*sinzf*sinzf-0.25;
			f3=-0.5*sinzf*cos(zf);
			ses=se2*f2+se3*f3;
			sis=si2*f2+si3*f3;
			sls=sl2*f2+sl3*f3+sl4*sinzf;
			sghs=sgh2*f2+sgh3*f3+sgh4*sinzf;
			shs=sh2*f2+sh3*f3;
			zm=zmol+znl*deep_arg->t;
			zf=zm+2*zel*sin(zm);
			sinzf=sin(zf);
			f2=0.5*sinzf*sinzf-0.25;
			f3=-0.5*sinzf*cos(zf);
			sel=ee2*f2+e3*f3;
			sil=xi2*f2+xi3*f3;
			sll=xl2*f2+xl3*f3+xl4*sinzf;
			sghl=xgh2*f2+xgh3*f3+xgh4*sinzf;
			sh1=xh2*f2+xh3*f3;
			pe=ses+sel;
			pinc=sis+sil;
			pl=sls+sll;
		}

		pgh=sghs+sghl;
		ph=shs+sh1;
		deep_arg->xinc=deep_arg->xinc+pinc;
		deep_arg->em=deep_arg->em+pe;

		if (xqncl>=0.2) {
			/* Apply periodics directly */
			ph=ph/deep_arg->sinio;
			pgh=pgh-deep_arg->cosio*ph;
			deep_arg->omgadf=deep_arg->omgadf+pgh;
			deep_arg->xnode=deep_arg->xnode+ph;
			deep_arg->xll=deep_arg->xll+pl;
		} else {
			/* Apply periodics with Lyddane modification */
			sinok=sin(deep_arg->xnode);
			cosok=cos(deep_arg->xnode);
			alfdp=sinis*sinok;
			betdp=sinis*cosok;
			dalf=ph*cosok+pinc*cosis*sinok;
			dbet=-ph*sinok+pinc*cosis*cosok;
			alfdp=alfdp+dalf;
			betdp=betdp+dbet;
			deep_arg->xnode=FMod2p(deep_arg->xnode);
			xls=deep_arg->xll+deep_arg->omgadf+cosis*deep_arg->xnode;
			dls=pl+pgh-pinc*deep_arg->xnode*sinis;
			xls=xls+dls;
			xnoh=deep_arg->xnode;
			deep_arg->xnode=AcTan(alfdp,betdp);

			/* This is a patch to Lyddane modification */
			/* suggested by Rob Matson. */

			if (fabs(xnoh-deep_arg->xnode)>pi) {
			      if (deep_arg->xnode<xnoh)
				  deep_arg->xnode+=twopi;
			      else
				  deep_arg->xnode-=twopi;
			}

			deep_arg->xll=deep_arg->xll+pl;
			deep_arg->omgadf=xls-deep_arg->xll-cos(deep_arg->xinc)*deep_arg->xnode;
		}
		return;
	}
}

void SDP4(double tsince, tle_t * tle, vector_t * pos, vector_t * vel)
{
	/* This function is used to calculate the position and velocity */
	/* of deep-space (period > 225 minutes) satellites. tsince is   */
	/* time since epoch in minutes, tle is a pointer to a tle_t     */
	/* structure with Keplerian orbital elements and pos and vel    */
	/* are vector_t structures returning ECI satellite position and */
	/* velocity. Use Convert_Sat_State() to convert to km and km/s. */

	int i;

	static double x3thm1, c1, x1mth2, c4, xnodcf, t2cof, xlcof,
	aycof, x7thm1;

	double a, axn, ayn, aynl, beta, betal, capu, cos2u, cosepw, cosik,
	cosnok, cosu, cosuk, ecose, elsq, epw, esine, pl, theta4, rdot,
	rdotk, rfdot, rfdotk, rk, sin2u, sinepw, sinik, sinnok, sinu,
	sinuk, tempe, templ, tsq, u, uk, ux, uy, uz, vx, vy, vz, xinck, xl,
	xlt, xmam, xmdf, xmx, xmy, xnoddf, xnodek, xll, a1, a3ovk2, ao, c2,
	coef, coef1, x1m5th, xhdot1, del1, r, delo, eeta, eta, etasq,
	perigee, psisq, tsi, qoms24, s4, pinvsq, temp, tempa, temp1,
	temp2, temp3, temp4, temp5, temp6, bx, by, bz, cx, cy, cz;

	static deep_arg_t deep_arg;

	/* Initialization */

	if (isFlagClear(SDP4_INITIALIZED_FLAG)) {
		SetFlag(SDP4_INITIALIZED_FLAG);

		/* Recover original mean motion (xnodp) and   */
		/* semimajor axis (aodp) from input elements. */

		a1=pow(xke/tle->xno,tothrd);
		deep_arg.cosio=cos(tle->xincl);
		deep_arg.theta2=deep_arg.cosio*deep_arg.cosio;
		x3thm1=3*deep_arg.theta2-1;
		deep_arg.eosq=tle->eo*tle->eo;
		deep_arg.betao2=1-deep_arg.eosq;
		deep_arg.betao=sqrt(deep_arg.betao2);
		del1=1.5*ck2*x3thm1/(a1*a1*deep_arg.betao*deep_arg.betao2);
		ao=a1*(1-del1*(0.5*tothrd+del1*(1+134/81*del1)));
		delo=1.5*ck2*x3thm1/(ao*ao*deep_arg.betao*deep_arg.betao2);
		deep_arg.xnodp=tle->xno/(1+delo);
		deep_arg.aodp=ao/(1-delo);

		/* For perigee below 156 km, the values */
		/* of s and qoms2t are altered.         */

		s4=s;
		qoms24=qoms2t;
		perigee=(deep_arg.aodp*(1-tle->eo)-ae)*xkmper;

		if (perigee<156.0) {
			if (perigee<=98.0)
				s4=20.0;
			else
				s4=perigee-78.0;

			qoms24=pow((120-s4)*ae/xkmper,4);
			s4=s4/xkmper+ae;
		}

		pinvsq=1/(deep_arg.aodp*deep_arg.aodp*deep_arg.betao2*deep_arg.betao2);
		deep_arg.sing=sin(tle->omegao);
		deep_arg.cosg=cos(tle->omegao);
		tsi=1/(deep_arg.aodp-s4);
		eta=deep_arg.aodp*tle->eo*tsi;
		etasq=eta*eta;
		eeta=tle->eo*eta;
		psisq=fabs(1-etasq);
		coef=qoms24*pow(tsi,4);
		coef1=coef/pow(psisq,3.5);
		c2=coef1*deep_arg.xnodp*(deep_arg.aodp*(1+1.5*etasq+eeta*(4+etasq))+0.75*ck2*tsi/psisq*x3thm1*(8+3*etasq*(8+etasq)));
		c1=tle->bstar*c2;
		deep_arg.sinio=sin(tle->xincl);
		a3ovk2=-xj3/ck2*pow(ae,3);
		x1mth2=1-deep_arg.theta2;
		c4=2*deep_arg.xnodp*coef1*deep_arg.aodp*deep_arg.betao2*(eta*(2+0.5*etasq)+tle->eo*(0.5+2*etasq)-2*ck2*tsi/(deep_arg.aodp*psisq)*(-3*x3thm1*(1-2*eeta+etasq*(1.5-0.5*eeta))+0.75*x1mth2*(2*etasq-eeta*(1+etasq))*cos(2*tle->omegao)));
		theta4=deep_arg.theta2*deep_arg.theta2;
		temp1=3*ck2*pinvsq*deep_arg.xnodp;
		temp2=temp1*ck2*pinvsq;
		temp3=1.25*ck4*pinvsq*pinvsq*deep_arg.xnodp;
		deep_arg.xmdot=deep_arg.xnodp+0.5*temp1*deep_arg.betao*x3thm1+0.0625*temp2*deep_arg.betao*(13-78*deep_arg.theta2+137*theta4);
		x1m5th=1-5*deep_arg.theta2;
		deep_arg.omgdot=-0.5*temp1*x1m5th+0.0625*temp2*(7-114*deep_arg.theta2+395*theta4)+temp3*(3-36*deep_arg.theta2+49*theta4);
		xhdot1=-temp1*deep_arg.cosio;
		deep_arg.xnodot=xhdot1+(0.5*temp2*(4-19*deep_arg.theta2)+2*temp3*(3-7*deep_arg.theta2))*deep_arg.cosio;
		xnodcf=3.5*deep_arg.betao2*xhdot1*c1;
		t2cof=1.5*c1;
		xlcof=0.125*a3ovk2*deep_arg.sinio*(3+5*deep_arg.cosio)/(1+deep_arg.cosio);
		aycof=0.25*a3ovk2*deep_arg.sinio;
		x7thm1=7*deep_arg.theta2-1;

		/* initialize Deep() */

		Deep(dpinit,tle,&deep_arg);
	}

	/* Update for secular gravity and atmospheric drag */
	xmdf=tle->xmo+deep_arg.xmdot*tsince;
	deep_arg.omgadf=tle->omegao+deep_arg.omgdot*tsince;
	xnoddf=tle->xnodeo+deep_arg.xnodot*tsince;
	tsq=tsince*tsince;
	deep_arg.xnode=xnoddf+xnodcf*tsq;
	tempa=1-c1*tsince;
	tempe=tle->bstar*c4*tsince;
	templ=t2cof*tsq;
	deep_arg.xn=deep_arg.xnodp;

	/* Update for deep-space secular effects */
	deep_arg.xll=xmdf;
	deep_arg.t=tsince;

	Deep(dpsec, tle, &deep_arg);

	xmdf=deep_arg.xll;
	a=pow(xke/deep_arg.xn,tothrd)*tempa*tempa;
	deep_arg.em=deep_arg.em-tempe;
	xmam=xmdf+deep_arg.xnodp*templ;

	/* Update for deep-space periodic effects */
	deep_arg.xll=xmam;

	Deep(dpper,tle,&deep_arg);

	xmam=deep_arg.xll;
	xl=xmam+deep_arg.omgadf+deep_arg.xnode;
	beta=sqrt(1-deep_arg.em*deep_arg.em);
	deep_arg.xn=xke/pow(a,1.5);

	/* Long period periodics */
	axn=deep_arg.em*cos(deep_arg.omgadf);
	temp=1/(a*beta*beta);
	xll=temp*xlcof*axn;
	aynl=temp*aycof;
	xlt=xl+xll;
	ayn=deep_arg.em*sin(deep_arg.omgadf)+aynl;

	/* Solve Kepler's Equation */
	capu=FMod2p(xlt-deep_arg.xnode);
	temp2=capu;
	i=0;

	do {
		sinepw=sin(temp2);
		cosepw=cos(temp2);
		temp3=axn*sinepw;
		temp4=ayn*cosepw;
		temp5=axn*cosepw;
		temp6=ayn*sinepw;
		epw=(capu-temp4+temp3-temp2)/(1-temp5-temp6)+temp2;

		if (fabs(epw-temp2)<=e6a)
			break;

		temp2=epw;

	} while (i++<10);

	/* Short period preliminary quantities */
	ecose=temp5+temp6;
	esine=temp3-temp4;
	elsq=axn*axn+ayn*ayn;
	temp=1-elsq;
	pl=a*temp;
	r=a*(1-ecose);
	temp1=1/r;
	rdot=xke*sqrt(a)*esine*temp1;
	rfdot=xke*sqrt(pl)*temp1;
	temp2=a*temp1;
	betal=sqrt(temp);
	temp3=1/(1+betal);
	cosu=temp2*(cosepw-axn+ayn*esine*temp3);
	sinu=temp2*(sinepw-ayn-axn*esine*temp3);
	u=AcTan(sinu,cosu);
	sin2u=2*sinu*cosu;
	cos2u=2*cosu*cosu-1;
	temp=1/pl;
	temp1=ck2*temp;
	temp2=temp1*temp;

	/* Update for short periodics */
	rk=r*(1-1.5*temp2*betal*x3thm1)+0.5*temp1*x1mth2*cos2u;
	uk=u-0.25*temp2*x7thm1*sin2u;
	xnodek=deep_arg.xnode+1.5*temp2*deep_arg.cosio*sin2u;
	xinck=deep_arg.xinc+1.5*temp2*deep_arg.cosio*deep_arg.sinio*cos2u;
	rdotk=rdot-deep_arg.xn*temp1*x1mth2*sin2u;
	rfdotk=rfdot+deep_arg.xn*temp1*(x1mth2*cos2u+1.5*x3thm1);

	/* Orientation vectors */
	sinuk=sin(uk);
	cosuk=cos(uk);
	sinik=sin(xinck);
	cosik=cos(xinck);
	sinnok=sin(xnodek);
	cosnok=cos(xnodek);
	xmx=-sinnok*cosik;
	xmy=cosnok*cosik;
	ux=xmx*sinuk+cosnok*cosuk;
	uy=xmy*sinuk+sinnok*cosuk;
	uz=sinik*sinuk;
	vx=xmx*cosuk-cosnok*sinuk;
	vy=xmy*cosuk-sinnok*sinuk;
	vz=sinik*cosuk;

	/* Position and velocity */
	pos->x=rk*ux;
	pos->y=rk*uy;
	pos->z=rk*uz;
	vel->x=rdotk*ux+rfdotk*vx;
	vel->y=rdotk*uy+rfdotk*vy;
	vel->z=rdotk*uz+rfdotk*vz;

	/* Calculations for squint angle begin here... */

	if (calc_squint) {
		bx=cos(alat)*cos(alon+deep_arg.omgadf);
		by=cos(alat)*sin(alon+deep_arg.omgadf);
		bz=sin(alat);
		cx=bx;
		cy=by*cos(xinck)-bz*sin(xinck);
		cz=by*sin(xinck)+bz*cos(xinck);
		ax=cx*cos(xnodek)-cy*sin(xnodek);
		ay=cx*sin(xnodek)+cy*cos(xnodek);
		az=cz;
	}

	/* Phase in radians */
	phase=xlt-deep_arg.xnode-deep_arg.omgadf+twopi;

	if (phase<0.0)
		phase+=twopi;

	phase=FMod2p(phase);
}

void Calculate_User_PosVel(double time, geodetic_t *geodetic, vector_t *obs_pos, vector_t *obs_vel)
{
	/* Calculate_User_PosVel() passes the user's geodetic position
	   and the time of interest and returns the ECI position and
	   velocity of the observer.  The velocity calculation assumes
	   the geodetic position is stationary relative to the earth's
	   surface. */

	/* Reference:  The 1992 Astronomical Almanac, page K11. */

	double c, sq, achcp;

	geodetic->theta=FMod2p(ThetaG_JD(time)+geodetic->lon); /* LMST */
	c=1/sqrt(1+f*(f-2)*Sqr(sin(geodetic->lat)));
	sq=Sqr(1-f)*c;
	achcp=(xkmper*c+geodetic->alt)*cos(geodetic->lat);
	obs_pos->x=achcp*cos(geodetic->theta); /* kilometers */
	obs_pos->y=achcp*sin(geodetic->theta);
	obs_pos->z=(xkmper*sq+geodetic->alt)*sin(geodetic->lat);
	obs_vel->x=-mfactor*obs_pos->y; /* kilometers/second */
	obs_vel->y=mfactor*obs_pos->x;
	obs_vel->z=0;
	Magnitude(obs_pos);
	Magnitude(obs_vel);
}

void Calculate_LatLonAlt(double time, vector_t *pos,  geodetic_t *geodetic)
{
	/* Procedure Calculate_LatLonAlt will calculate the geodetic  */
	/* position of an object given its ECI position pos and time. */
	/* It is intended to be used to determine the ground track of */
	/* a satellite.  The calculations  assume the earth to be an  */
	/* oblate spheroid as defined in WGS '72.                     */

	/* Reference:  The 1992 Astronomical Almanac, page K12. */

	double r, e2, phi, c;

	geodetic->theta=AcTan(pos->y,pos->x); /* radians */
	geodetic->lon=FMod2p(geodetic->theta-ThetaG_JD(time)); /* radians */
	r=sqrt(Sqr(pos->x)+Sqr(pos->y));
	e2=f*(2-f);
	geodetic->lat=AcTan(pos->z,r); /* radians */

	do {
		phi=geodetic->lat;
		c=1/sqrt(1-e2*Sqr(sin(phi)));
		geodetic->lat=AcTan(pos->z+xkmper*c*e2*sin(phi),r);

	} while (fabs(geodetic->lat-phi)>=1E-10);

	geodetic->alt=r/cos(geodetic->lat)-xkmper*c; /* kilometers */

	if (geodetic->lat>pio2)
		geodetic->lat-=twopi;
}

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

void Calculate_Obs(double time, vector_t *pos, vector_t *vel, geodetic_t *geodetic, vector_t *obs_set)
{
	/* The procedures Calculate_Obs and Calculate_RADec calculate         */
	/* the *topocentric* coordinates of the object with ECI position,     */
	/* {pos}, and velocity, {vel}, from location {geodetic} at {time}.    */
	/* The {obs_set} returned for Calculate_Obs consists of azimuth,      */
	/* elevation, range, and range rate (in that order) with units of     */
	/* radians, radians, kilometers, and kilometers/second, respectively. */
	/* The WGS '72 geoid is used and the effect of atmospheric refraction */
	/* (under standard temperature and pressure) is incorporated into the */
	/* elevation calculation; the effect of atmospheric refraction on     */
	/* range and range rate has not yet been quantified.                  */

	/* The {obs_set} for Calculate_RADec consists of right ascension and  */
	/* declination (in that order) in radians.  Again, calculations are   */
	/* based on *topocentric* position using the WGS '72 geoid and        */
	/* incorporating atmospheric refraction.                              */

	double sin_lat, cos_lat, sin_theta, cos_theta, el, azim, top_s, top_e, top_z;

	vector_t obs_pos, obs_vel, range, rgvel;

	Calculate_User_PosVel(time, geodetic, &obs_pos, &obs_vel);

	range.x=pos->x-obs_pos.x;
	range.y=pos->y-obs_pos.y;
	range.z=pos->z-obs_pos.z;

	/* Save these values globally for calculating squint angles later... */

	rx=range.x;
	ry=range.y;
	rz=range.z;

	rgvel.x=vel->x-obs_vel.x;
	rgvel.y=vel->y-obs_vel.y;
	rgvel.z=vel->z-obs_vel.z;

	Magnitude(&range);

	sin_lat=sin(geodetic->lat);
	cos_lat=cos(geodetic->lat);
	sin_theta=sin(geodetic->theta);
	cos_theta=cos(geodetic->theta);
	top_s=sin_lat*cos_theta*range.x+sin_lat*sin_theta*range.y-cos_lat*range.z;
	top_e=-sin_theta*range.x+cos_theta*range.y;
	top_z=cos_lat*cos_theta*range.x+cos_lat*sin_theta*range.y+sin_lat*range.z;
	azim=atan(-top_e/top_s); /* Azimuth */

	if (top_s>0.0)
		azim=azim+pi;

	if (azim<0.0)
		azim=azim+twopi;

	el=ArcSin(top_z/range.w);
	obs_set->x=azim;	/* Azimuth (radians)   */
	obs_set->y=el;		/* Elevation (radians) */
	obs_set->z=range.w;	/* Range (kilometers)  */

	/* Range Rate (kilometers/second) */

	obs_set->w=Dot(&range,&rgvel)/range.w;

	/* Corrections for atmospheric refraction */
	/* Reference:  Astronomical Algorithms by Jean Meeus, pp. 101-104    */
	/* Correction is meaningless when apparent elevation is below horizon */

	/*** Temporary bypass for PREDICT-2.2.0 ***/

	/* obs_set->y=obs_set->y+Radians((1.02/tan(Radians(Degrees(el)+10.3/(Degrees(el)+5.11))))/60); */

	obs_set->y=el;

	/**** End bypass ****/

	if (obs_set->y>=0.0)
		SetFlag(VISIBLE_FLAG);
	else {
		obs_set->y=el;  /* Reset to true elevation */
		ClearFlag(VISIBLE_FLAG);
	}
}

void Calculate_RADec(double time, vector_t *pos, vector_t *vel, geodetic_t *geodetic, vector_t *obs_set)
{
	/* Reference:  Methods of Orbit Determination by  */
	/*             Pedro Ramon Escobal, pp. 401-402   */

	double	phi, theta, sin_theta, cos_theta, sin_phi, cos_phi, az, el,
		Lxh, Lyh, Lzh, Sx, Ex, Zx, Sy, Ey, Zy, Sz, Ez, Zz, Lx, Ly,
		Lz, cos_delta, sin_alpha, cos_alpha;

	Calculate_Obs(time,pos,vel,geodetic,obs_set);

	if (isFlagSet(VISIBLE_FLAG)) {
		az=obs_set->x;
		el=obs_set->y;
		phi=geodetic->lat;
		theta=FMod2p(ThetaG_JD(time)+geodetic->lon);
		sin_theta=sin(theta);
		cos_theta=cos(theta);
		sin_phi=sin(phi);
		cos_phi=cos(phi);
		Lxh=-cos(az)*cos(el);
		Lyh=sin(az)*cos(el);
		Lzh=sin(el);
		Sx=sin_phi*cos_theta;
		Ex=-sin_theta;
		Zx=cos_theta*cos_phi;
		Sy=sin_phi*sin_theta;
		Ey=cos_theta;
		Zy=sin_theta*cos_phi;
		Sz=-cos_phi;
		Ez=0.0;
		Zz=sin_phi;
		Lx=Sx*Lxh+Ex*Lyh+Zx*Lzh;
		Ly=Sy*Lxh+Ey*Lyh+Zy*Lzh;
		Lz=Sz*Lxh+Ez*Lyh+Zz*Lzh;
		obs_set->y=ArcSin(Lz);  /* Declination (radians) */
		cos_delta=sqrt(1.0-Sqr(Lz));
		sin_alpha=Ly/cos_delta;
		cos_alpha=Lx/cos_delta;
		obs_set->x=AcTan(sin_alpha,cos_alpha); /* Right Ascension (radians) */
		obs_set->x=FMod2p(obs_set->x);
	}
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

double FixAngle(x)
double x;
{
	/* This function reduces angles greater than
	   two pi by subtracting two pi from the angle */

	while (x>twopi)
		x-=twopi;

	return x;
}

double PrimeAngle(x)
double x;
{
	/* This function is used in the FindMoon() function. */

	x=x-360.0*floor(x/360.0);
	return x;
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

		obs_geodetic.lat=qth.stnlat*deg2rad;
		obs_geodetic.lon=-qth.stnlong*deg2rad;
		obs_geodetic.alt=((double)qth.stnalt)/1000.0;
		obs_geodetic.theta=0.0;

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

int Select()
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

	if (totalsats >= maxsats)
		mvprintw(LINES-3,46,"Truncated to %d satellites",maxsats);
	else
		mvprintw(LINES-3,46,"%d satellites",totalsats);

	/* Create items */
	n_choices = ARRAY_SIZE(sat);
	my_items = (ITEM **)calloc(n_choices, sizeof(ITEM *));
	for(i = 0; i < n_choices; ++i)
		my_items[i] = new_item(sat[i].name, sat[i].designator);

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

	for (i=0, j=0; i<totalsats; i++)
		if (strcmp(item_name(current_item(my_menu)),sat[i].name)==0)
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

double CurrentDaynum()
{
        /* Read the system clock and return the number
	 *            of days since 31Dec79 00:00:00 UTC (daynum 0) */

        struct timeb tptr;

        ftime(&tptr);

        return ((((double)tptr.time+0.001*(double)tptr.millitm)/86400.0)-3651.0);
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

double GetStartTime(mode)
char mode;
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

		if (mode=='m')
				printw("the Moon\n\n");

		if (mode=='o')
				printw("the Sun\n\n");

		if (mode!='m' && mode!='o')
				printw("%-15s\n\n",sat[indx].name);

		bozo_count++;

		strcpy(string,Daynum2String(CurrentDaynum(),20,"%a %d%b%y %H:%M:%S"));

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
			return(CurrentDaynum());

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

void FindMoon(daynum)
double daynum;
{
	/* This function determines the position of the moon, including
	the azimuth and elevation headings, relative to the latitude
	and longitude of the tracking station.  This code was derived
	from a Javascript implementation of the Meeus method for
	determining the exact position of the Moon found at:
	http://www.geocities.com/s_perona/ingles/poslun.htm. */

	double  jd, ss, t, t1, t2, t3, d, ff, l1, m, m1, ex, om, l,
	b, w1, w2, bt, p, lm, h, ra, dec, z, ob, n, el,
	az, teg, th, mm, dv;

	jd=daynum+2444238.5;

	t=(jd-2415020.0)/36525.0;
	t2=t*t;
	t3=t2*t;
	l1=270.434164+481267.8831*t-0.001133*t2+0.0000019*t3;
	m=358.475833+35999.0498*t-0.00015*t2-0.0000033*t3;
	m1=296.104608+477198.8491*t+0.009192*t2+0.0000144*t3;
	d=350.737486+445267.1142*t-0.001436*t2+0.0000019*t3;
	ff=11.250889+483202.0251*t-0.003211*t2-0.0000003*t3;
	om=259.183275-1934.142*t+0.002078*t2+0.0000022*t3;
	om=om*deg2rad;

	/* Additive terms */

	l1=l1+0.000233*sin((51.2+20.2*t)*deg2rad);
	ss=0.003964*sin((346.56+132.87*t-0.0091731*t2)*deg2rad);
	l1=l1+ss+0.001964*sin(om);
	m=m-0.001778*sin((51.2+20.2*t)*deg2rad);
	m1=m1+0.000817*sin((51.2+20.2*t)*deg2rad);
	m1=m1+ss+0.002541*sin(om);
	d=d+0.002011*sin((51.2+20.2*t)*deg2rad);
	d=d+ss+0.001964*sin(om);
	ff=ff+ss-0.024691*sin(om);
	ff=ff-0.004328*sin(om+(275.05-2.3*t)*deg2rad);
	ex=1.0-0.002495*t-0.00000752*t2;
	om=om*deg2rad;

	l1=PrimeAngle(l1);
	m=PrimeAngle(m);
	m1=PrimeAngle(m1);
	d=PrimeAngle(d);
	ff=PrimeAngle(ff);
	om=PrimeAngle(om);

	m=m*deg2rad;
	m1=m1*deg2rad;
	d=d*deg2rad;
	ff=ff*deg2rad;

	/* Ecliptic Longitude */

	l=l1+6.28875*sin(m1)+1.274018*sin(2.0*d-m1)+0.658309*sin(2.0*d);
	l=l+0.213616*sin(2.0*m1)-ex*0.185596*sin(m)-0.114336*sin(2.0*ff);
	l=l+0.058793*sin(2.0*d-2.0*m1)+ex*0.057212*sin(2.0*d-m-m1)+0.05332*sin(2.0*d+m1);
	l=l+ex*0.045874*sin(2.0*d-m)+ex*0.041024*sin(m1-m)-0.034718*sin(d);
	l=l-ex*0.030465*sin(m+m1)+0.015326*sin(2.0*d-2.0*ff)-0.012528*sin(2.0*ff+m1);

	l=l-0.01098*sin(2.0*ff-m1)+0.010674*sin(4.0*d-m1)+0.010034*sin(3.0*m1);
	l=l+0.008548*sin(4.0*d-2.0*m1)-ex*0.00791*sin(m-m1+2.0*d)-ex*0.006783*sin(2.0*d+m);

	l=l+0.005162*sin(m1-d)+ex*0.005*sin(m+d)+ex*0.004049*sin(m1-m+2.0*d);
	l=l+0.003996*sin(2.0*m1+2.0*d)+0.003862*sin(4.0*d)+0.003665*sin(2.0*d-3.0*m1);

	l=l+ex*0.002695*sin(2.0*m1-m)+0.002602*sin(m1-2.0*ff-2.0*d)+ex*0.002396*sin(2.0*d-m-2.0*m1);

	l=l-0.002349*sin(m1+d)+ex*ex*0.002249*sin(2.0*d-2.0*m)-ex*0.002125*sin(2.0*m1+m);

	l=l-ex*ex*0.002079*sin(2.0*m)+ex*ex*0.002059*sin(2.0*d-m1-2.0*m)-0.001773*sin(m1+2.0*d-2.0*ff);

	l=l+ex*0.00122*sin(4.0*d-m-m1)-0.00111*sin(2.0*m1+2.0*ff)+0.000892*sin(m1-3.0*d);

	l=l-ex*0.000811*sin(m+m1+2.0*d)+ex*0.000761*sin(4.0*d-m-2.0*m1)+ex*ex*.000717*sin(m1-2.0*m);

	l=l+ex*ex*0.000704*sin(m1-2.0*m-2.0*d)+ex*0.000693*sin(m-2.0*m1+2.0*d)+ex*0.000598*sin(2.0*d-m-2.0*ff)+0.00055*sin(m1+4.0*d);

	l=l+0.000538*sin(4.0*m1)+ex*0.000521*sin(4.0*d-m)+0.000486*sin(2.0*m1-d);

	l=l-0.001595*sin(2.0*ff+2.0*d);

	/* Ecliptic latitude */

	b=5.128189*sin(ff)+0.280606*sin(m1+ff)+0.277693*sin(m1-ff)+0.173238*sin(2.0*d-ff);
	b=b+0.055413*sin(2.0*d+ff-m1)+0.046272*sin(2.0*d-ff-m1)+0.032573*sin(2.0*d+ff);

	b=b+0.017198*sin(2.0*m1+ff)+9.266999e-03*sin(2.0*d+m1-ff)+0.008823*sin(2.0*m1-ff);
	b=b+ex*0.008247*sin(2.0*d-m-ff)+0.004323*sin(2.0*d-ff-2.0*m1)+0.0042*sin(2.0*d+ff+m1);

	b=b+ex*0.003372*sin(ff-m-2.0*d)+ex*0.002472*sin(2.0*d+ff-m-m1)+ex*0.002222*sin(2.0*d+ff-m);

	b=b+0.002072*sin(2.0*d-ff-m-m1)+ex*0.001877*sin(ff-m+m1)+0.001828*sin(4.0*d-ff-m1);

	b=b-ex*0.001803*sin(ff+m)-0.00175*sin(3.0*ff)+ex*0.00157*sin(m1-m-ff)-0.001487*sin(ff+d)-ex*0.001481*sin(ff+m+m1)+ex*0.001417*sin(ff-m-m1)+ex*0.00135*sin(ff-m)+0.00133*sin(ff-d);

	b=b+0.001106*sin(ff+3.0*m1)+0.00102*sin(4.0*d-ff)+0.000833*sin(ff+4.0*d-m1);

	b=b+0.000781*sin(m1-3.0*ff)+0.00067*sin(ff+4.0*d-2.0*m1)+0.000606*sin(2.0*d-3.0*ff);

	b=b+0.000597*sin(2.0*d+2.0*m1-ff)+ex*0.000492*sin(2.0*d+m1-m-ff)+0.00045*sin(2.0*m1-ff-2.0*d);

	b=b+0.000439*sin(3.0*m1-ff)+0.000423*sin(ff+2.0*d+2.0*m1)+0.000422*sin(2.0*d-ff-3.0*m1);

	b=b-ex*0.000367*sin(m+ff+2.0*d-m1)-ex*0.000353*sin(m+ff+2.0*d)+0.000331*sin(ff+4.0*d);

	b=b+ex*0.000317*sin(2.0*d+ff-m+m1)+ex*ex*0.000306*sin(2.0*d-2.0*m-ff)-0.000283*sin(m1+3.0*ff);

	w1=0.0004664*cos(om*deg2rad);
	w2=0.0000754*cos((om+275.05-2.3*t)*deg2rad);
	bt=b*(1.0-w1-w2);

	/* Parallax calculations */

	p=0.950724+0.051818*cos(m1)+0.009531*cos(2.0*d-m1)+0.007843*cos(2.0*d)+0.002824*cos(2.0*m1)+0.000857*cos(2.0*d+m1)+ex*0.000533*cos(2.0*d-m)+ex*0.000401*cos(2.0*d-m-m1);

	p=p+0.000173*cos(3.0*m1)+0.000167*cos(4.0*d-m1)-ex*0.000111*cos(m)+0.000103*cos(4.0*d-2.0*m1)-0.000084*cos(2.0*m1-2.0*d)-ex*0.000083*cos(2.0*d+m)+0.000079*cos(2.0*d+2.0*m1);

	p=p+0.000072*cos(4.0*d)+ex*0.000064*cos(2.0*d-m+m1)-ex*0.000063*cos(2.0*d+m-m1);

	p=p+ex*0.000041*cos(m+d)+ex*0.000035*cos(2.0*m1-m)-0.000033*cos(3.0*m1-2.0*d);

	p=p-0.00003*cos(m1+d)-0.000029*cos(2.0*ff-2.0*d)-ex*0.000029*cos(2.0*m1+m);

	p=p+ex*ex*0.000026*cos(2.0*d-2.0*m)-0.000023*cos(2.0*ff-2.0*d+m1)+ex*0.000019*cos(4.0*d-m-m1);

	b=bt*deg2rad;
	lm=l*deg2rad;
	moon_dx=3.0/(pi*p);

	/* Semi-diameter calculation */
	/* sem=10800.0*asin(0.272488*p*deg2rad)/pi; */

	/* Convert ecliptic coordinates to equatorial coordinates */

	z=(jd-2415020.5)/365.2422;
	ob=23.452294-(0.46845*z+5.9e-07*z*z)/3600.0;
	ob=ob*deg2rad;
	dec=asin(sin(b)*cos(ob)+cos(b)*sin(ob)*sin(lm));
	ra=acos(cos(b)*cos(lm)/cos(dec));

	if (lm>pi)
		ra=twopi-ra;

	/* ra = right ascension */
	/* dec = declination */

	n=qth.stnlat*deg2rad;    /* North latitude of tracking station */

	/* Find siderial time in radians */

	t=(jd-2451545.0)/36525.0;
	teg=280.46061837+360.98564736629*(jd-2451545.0)+(0.000387933*t-t*t/38710000.0)*t;

	while (teg>360.0)
		teg-=360.0;

	th=FixAngle((teg-qth.stnlong)*deg2rad);
	h=th-ra;

	az=atan2(sin(h),cos(h)*sin(n)-tan(dec)*cos(n))+pi;
	el=asin(sin(n)*sin(dec)+cos(n)*cos(dec)*cos(h));

	moon_az=az/deg2rad;
	moon_el=el/deg2rad;

	attrset(COLOR_PAIR(4)|A_REVERSE|A_BOLD);
	mvprintw(20,70,"   Moon  ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	if (moon_el > 0.0)
		attrset(COLOR_PAIR(1)|A_BOLD);
	else
		attrset(COLOR_PAIR(1));
	mvprintw(21,70,"%-7.2fAz",moon_az);
	mvprintw(22,70,"%+-6.2f El",moon_el);

	/* Radial velocity approximation.  This code was derived
	from "Amateur Radio Software", by John Morris, GM4ANB,
	published by the RSGB in 1985. */

	mm=FixAngle(1.319238+daynum*0.228027135);  /* mean moon position */
	t2=0.10976;
	t1=mm+t2*sin(mm);
	dv=0.01255*moon_dx*moon_dx*sin(t1)*(1.0+t2*cos(mm));
	dv=dv*4449.0;
	t1=6378.0;
	t2=384401.0;
	t3=t1*t2*(cos(dec)*cos(n)*sin(h));
	t3=t3/sqrt(t2*t2-t2*t1*sin(el));
	moon_dv=dv+t3*0.0753125;

	moon_dec=dec/deg2rad;
	moon_ra=ra/deg2rad;
	moon_gha=teg-moon_ra;

	if (moon_gha<0.0)
		moon_gha+=360.0;
}

void FindSun(daynum)
double daynum;
{
	/* This function finds the position of the Sun */

	/* Zero vector for initializations */
	vector_t zero_vector={0,0,0,0};

	/* Solar ECI position vector  */
	vector_t solar_vector=zero_vector;

	/* Solar observed azi and ele vector  */
	vector_t solar_set=zero_vector;

	/* Solar right ascension and declination vector */
	vector_t solar_rad=zero_vector;

	/* Solar lat, long, alt vector */
	geodetic_t solar_latlonalt;

	jul_utc=daynum+2444238.5;

	Calculate_Solar_Position(jul_utc, &solar_vector);
	Calculate_Obs(jul_utc, &solar_vector, &zero_vector, &obs_geodetic, &solar_set);
	sun_azi=Degrees(solar_set.x);
	sun_ele=Degrees(solar_set.y);
	sun_range=1.0+((solar_set.z-AU)/AU);
	sun_range_rate=1000.0*solar_set.w;

	Calculate_LatLonAlt(jul_utc, &solar_vector, &solar_latlonalt);

	sun_lat=Degrees(solar_latlonalt.lat);
	sun_lon=360.0-Degrees(solar_latlonalt.lon);

	Calculate_RADec(jul_utc, &solar_vector, &zero_vector, &obs_geodetic, &solar_rad);

	sun_ra=Degrees(solar_rad.x);
	sun_dec=Degrees(solar_rad.y);
}


char AosHappens(x)
int x;
{
	/* This function returns a 1 if the satellite pointed to by
	   "x" can ever rise above the horizon of the ground station. */

	double lin, sma, apogee;

	if (sat[x].meanmo==0.0)
		return 0;
	else {
		lin=sat[x].incl;

		if (lin>=90.0)
			lin=180.0-lin;

		sma=331.25*exp(log(1440.0/sat[x].meanmo)*(2.0/3.0));
		apogee=sma*(1.0+sat[x].eccn)-xkmper;

		if ((acos(xkmper/(apogee+xkmper))+(lin*deg2rad)) > fabs(qth.stnlat*deg2rad))
			return 1;
		else
			return 0;
	}
}

char Decayed(x,time)
int x;
double time;
{
	/* This function returns a 1 if it appears that the
	   satellite pointed to by 'x' has decayed at the
	   time of 'time'.  If 'time' is 0.0, then the
	   current date/time is used. */

	double satepoch;

	if (time==0.0)
		time=CurrentDaynum();

	satepoch=DayNum(1,0,sat[x].year)+sat[x].refepoch;

	if (satepoch+((16.666666-sat[x].meanmo)/(10.0*fabs(sat[x].drag))) < time)
		return 1;
	else
		return 0;
}

char Geostationary(x)
int x;
{
	/* This function returns a 1 if the satellite pointed
	   to by "x" appears to be in a geostationary orbit */

	if (fabs(sat[x].meanmo-1.0027)<0.0002)

		return 1;
	else
		return 0;
}

#include "removed_functions.c"


int Print(string,mode)
char *string, mode;
{
	/* This function buffers and displays orbital predictions
	   and allows screens to be saved to a disk file. */

	char type[20], spaces[80], head1[80], head2[81];
	int key, ans=0, l, x, t;
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
			sprintf(head1,"                    %s's Orbit Calendar for the %s",qth.callsign,type);
			sprintf(head2,"           Date     Time    El   Az   RA     Dec    GHA     Vel   Range         ");
		}

		if (mode!='m' && mode!='o') {

			l=strlen(qth.callsign)+strlen(sat[indx].name)+strlen(type);

			spaces[0]=0;

			for (x=l; x<60; x+=2)
				strcat(spaces," ");

			sprintf(head1,"%s%s's %s Calendar for %s", spaces, qth.callsign, type, sat[indx].name);

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
			if (mode == 'p' || mode == 'v' || mode == 's') {
				mvprintw(1,60, "%s (%d)", sat[indx].name, sat[indx].catnum);
			}
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

				/* 'L' logs output to "satname.txt" */

				if (key=='L' && fd==NULL && buffer[0]) {
					sprintf(temp,"%s.txt",sat[indx].name);

					l=strlen(temp)-4;

					for (x=0; x<l; x++) {
						t=temp[x];

						if (t==32 || t==17 || t==92 || t==42 || t==46 || t==47)
							t='_';

						temp[x]=t;
					}

					fd=fopen(temp,"a");
					fprintf(fd,"%s%s\n",head1,head2);
					fprintf(fd,"%s",buffer);
					mvprintw(LINES-2,63,"Log = ON");
					move(LINES-2,21);
					refresh();
				} else if (fd!=NULL) {
					if (key=='L' || key=='N') {
						fprintf(fd,"%s\n\n",buffer);
						fclose(fd);
						fd=NULL;
						mvprintw(LINES-2,63,"        ");
						move(LINES-2,21);
						refresh();
					} else
						fprintf(fd,"%s",buffer);
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
#define MAX_NUM_CHARS 80

#define MAX_NUM_CHARS 80
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

	predict_julian_date_t curr_time = GetStartTime(mode);
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
				curr_time += cos((obs.elevation*180/M_PI-1.0)*deg2rad)*sqrt(orbit->altitude)/25000.0; //predict's magic time increment formula
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

			//move to the next orbit
			daynum = predict_next_aos(qth, orbit, daynum);

		} while (!should_quit && !should_break && !predict_decayed(orbit));
	} else {
		//display warning that passes are impossible
		bkgdset(COLOR_PAIR(5)|A_BOLD);
		clear();

		if (!predict_aos_happens(orbit, qth->latitude) || predict_decayed(orbit)) {
			mvprintw(12,5,"*** Passes for %s cannot occur for your ground station! ***\n",sat[indx].name);
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
	switch (object){
		case PREDICT_SUN:
			print_mode='o';
		break;
		case PREDICT_MOON:
			print_mode='m';
		break;
	}

	int iaz, iel, lastel=0;
	char string[MAX_NUM_CHARS], quit=0;
	double lastdaynum, rise=0.0;
	char time_string[MAX_NUM_CHARS];

	daynum=GetStartTime(print_mode);
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
			daynum+=0.04*(cos(deg2rad*(obs.elevation*180.0/M_PI+0.5)));
			celestial_predict(object, qth, daynum, &obs);
			iaz=(int)rint(obs.azimuth*180.0/M_PI);
			iel=(int)rint(obs.elevation*180.0/M_PI);
		} while (iel>3 && quit==0);

		//end the pass
		while (lastel!=0 && quit==0) {
			daynum=lastdaynum;

			//find sun/moon set
			do {
				daynum+=0.004*(sin(deg2rad*(obs.elevation*180.0/M_PI+0.5)));
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

void ShowOrbitData()
{
	/* This function permits displays a satellite's orbital
	   data.  The age of the satellite data is also provided. */

	int c, x, namelength, age;
	double an_period, no_period, sma, c1, e2, satepoch;
	char days[5];

	x=Select();

	while (x!=-1) {
		if (sat[x].meanmo!=0.0) {
			bkgdset(COLOR_PAIR(2)|A_BOLD);
			clear();
			sma=331.25*exp(log(1440.0/sat[x].meanmo)*(2.0/3.0));
			an_period=1440.0/sat[x].meanmo;
			c1=cos(sat[x].incl*deg2rad);
			e2=1.0-(sat[x].eccn*sat[x].eccn);
			no_period=(an_period*360.0)/(360.0+(4.97*pow((xkmper/sma),3.5)*((5.0*c1*c1)-1.0)/(e2*e2))/sat[x].meanmo);
			satepoch=DayNum(1,0,sat[x].year)+sat[x].refepoch;
			age=(int)rint(CurrentDaynum()-satepoch);

			if (age==1)
				strcpy(days,"day");
			else
				strcpy(days,"days");

			namelength=strlen(sat[x].name);

			printw("\n");

			for (c=41; c>namelength; c-=2)
				printw(" ");

			bkgdset(COLOR_PAIR(3)|A_BOLD);
			attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
			clear();

			mvprintw(0,0,"                                                                                ");
			mvprintw(1,0,"  flyby Orbital Data                                                            ");
			mvprintw(2,0,"                                                                                ");

			mvprintw(1,25,"(%ld) %s", sat[x].catnum, sat[x].name);

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
			mvprintw( 5,25,"%02d %.8f",sat[x].year,sat[x].refepoch);
			mvprintw( 6,25,"%.4f deg",sat[x].incl);
			mvprintw( 7,25,"%.4f deg",sat[x].raan);
			mvprintw( 8,25,"%g",sat[x].eccn);
			mvprintw( 9,25,"%.4f deg",sat[x].argper);
			mvprintw(10,25,"%.4f deg",sat[x].meanan);
			mvprintw(11,25,"%.8f rev/day",sat[x].meanmo);
			mvprintw(12,25,"%g rev/day/day",sat[x].drag);
			mvprintw(13,25,"%g rev/day/day/day",sat[x].nddot6);
			mvprintw(14,25,"%g 1/earth radii",sat[x].bstar);
			mvprintw(15,25,"%.4f km",sma);
			mvprintw(16,25,"%.4f km",sma*(1.0+sat[x].eccn)-xkmper);
			mvprintw(17,25,"%.4f km",sma*(1.0-sat[x].eccn)-xkmper);
			mvprintw(18,25,"%.4f mins",an_period);
			mvprintw(19,25,"%.4f mins",no_period);
			mvprintw(20,25,"%ld",sat[x].orbitnum);
			mvprintw(21,25,"%ld",sat[x].setnum);

			attrset(COLOR_PAIR(3)|A_BOLD);
			refresh();
			AnyKey();
		}
		x=Select();
	 };
}

void KepEdit()
{
	/* This function permits keyboard editing of the orbital database. */

	int x;

	do {
		x=Select();

		if (x!=-1) {
			bkgdset(COLOR_PAIR(3)|A_BOLD);
			attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
			clear();

			mvprintw(0,0,"                                                                                ");
			mvprintw(1,0,"  flyby Orbital Database Editing                                                ");
			mvprintw(2,0,"                                                                                ");

			attrset(COLOR_PAIR(4)|A_BOLD);

			mvprintw( 7,20,"Spacecraft Name :");
			mvprintw( 8,20,"Catalog Number  :");
			mvprintw( 9,20,"Designator      :");
			mvprintw(10,20,"Reference Epoch :");
			mvprintw(11,20,"Inclination     :");
			mvprintw(12,20,"RAAN            :");
			mvprintw(13,20,"Eccentricity    :");
			mvprintw(14,20,"Arg of Perigee  :");
			mvprintw(15,20,"Mean Anomaly    :");
			mvprintw(16,20,"Mean Motion     :");
			mvprintw(17,20,"Decay Rate      :");
			mvprintw(18,20,"Nddot/6         :");
			mvprintw(19,20,"Bstar Drag Term :");
			mvprintw(20,20,"Orbit Number    :");
			mvprintw(21,20,"Element Set No. :");

			attrset(COLOR_PAIR(2)|A_BOLD);

			mvprintw( 7,38,"%s",sat[x].name);
			mvprintw( 8,38,"%ld",sat[x].catnum);
			mvprintw( 9,38,"%s",sat[x].designator);
			mvprintw(10,38,"%02d %.8f",sat[x].year,sat[x].refepoch);
			mvprintw(11,38,"%.4f",sat[x].incl);
			mvprintw(12,38,"%.4f",sat[x].raan);
			mvprintw(13,38,"%g",sat[x].eccn);
			mvprintw(14,38,"%.4f",sat[x].argper);
			mvprintw(15,38,"%.4f",sat[x].meanan);
			mvprintw(16,38,"%.8f",sat[x].meanmo);
			mvprintw(17,38,"%g",sat[x].drag);
			mvprintw(18,38,"%g",sat[x].nddot6);
			mvprintw(19,38,"%g",sat[x].bstar);
			mvprintw(20,38,"%ld",sat[x].orbitnum);
			mvprintw(21,38,"%ld",sat[x].setnum);

			curs_set(1);
			refresh();

			sprintf(temp,"%s",sat[x].name);

			if (KbEdit(39,8))
				strncpy(sat[x].name,temp,24);

			sprintf(temp,"%ld",sat[x].catnum);

			if (KbEdit(39,9))
				sscanf(temp,"%ld",&sat[x].catnum);

			sprintf(temp,"%s",sat[x].designator);

			if (KbEdit(39,10))
				sscanf(temp,"%s",sat[x].designator);

			sprintf(temp,"%02d %4.8f",sat[x].year,sat[x].refepoch);

			if (KbEdit(39,11))
				sscanf(temp,"%d %lf",&sat[x].year,&sat[x].refepoch);

			sprintf(temp,"%4.4f",sat[x].incl);

			if (KbEdit(39,12))
				sscanf(temp,"%lf",&sat[x].incl);

			sprintf(temp,"%4.4f",sat[x].raan);

			if (KbEdit(39,13))
				sscanf(temp,"%lf",&sat[x].raan);

			sprintf(temp,"%g",sat[x].eccn);

			if (KbEdit(39,14))
				sscanf(temp,"%lf",&sat[x].eccn);

			sprintf(temp,"%4.4f",sat[x].argper);

			if (KbEdit(39,15))
				sscanf(temp,"%lf",&sat[x].argper);

			sprintf(temp,"%4.4f",sat[x].meanan);

			if (KbEdit(39,16))
				sscanf(temp,"%lf",&sat[x].meanan);

			sprintf(temp,"%4.8f",sat[x].meanmo);

			if (KbEdit(39,17))
				sscanf(temp,"%lf",&sat[x].meanmo);

			sprintf(temp,"%g",sat[x].drag);

			if (KbEdit(39,18))
				sscanf(temp,"%lf",&sat[x].drag);

			sprintf(temp,"%g",sat[x].nddot6);

			if (KbEdit(39,19))
				sscanf(temp,"%lf",&sat[x].nddot6);

			sprintf(temp,"%g",sat[x].bstar);

			if (KbEdit(39,20))
				sscanf(temp,"%lf",&sat[x].bstar);

			sprintf(temp,"%ld",sat[x].orbitnum);

			if (KbEdit(39,21))
				sscanf(temp,"%ld",&sat[x].orbitnum);

			sprintf(temp,"%ld",sat[x].setnum);

			if (KbEdit(39,22))
				sscanf(temp,"%ld",&sat[x].setnum);

			curs_set(0);
		}

	} while (x!=-1);

	if (resave) {
		SaveTLE();
		resave=0;
	}
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
	bool	comsat, aos_alarm=0;
	double	nextaos=0.0, lostime=0.0, aoslos=0.0,
		downlink=0.0, uplink=0.0, downlink_start=0.0,
		downlink_end=0.0, uplink_start=0.0, uplink_end=0.0,
		shift;
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
		mvprintw(1,21,"%-24s (%d)", sat_db.name, orbit->orbital_elements.element_number);

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
					if (rotctld_socket!=-1) TrackDataNet(rotctld_socket,sat_ele,sat_azi);
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
		daynum=CurrentDaynum();
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

	daynum=floor(GetStartTime(0));
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
	while (quit!=1 && breakout!=1 && Decayed(indx,daynum)==0);
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
	mvprintw(17,41," E ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw(17,45," Manually Edit Orbital Database");


	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(21,41," Q ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw(21,45," Exit flyby");

	refresh();

}

void ProgramInfo()
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

int QuickFind(string, outputfile)
char *string, *outputfile;
{
	int x, y, z, step=1;
	long start, now, end, count;
	char satname[50], startstr[20], endstr[20];
	time_t t;
	FILE *fd;

	if (outputfile[0])
		fd=fopen(outputfile,"w");
	else
		fd=stdout;

	startstr[0]=0;
	endstr[0]=0;

	ReadDataFiles();

	for (x=0; x<48 && string[x]!=0 && string[x]!='\n'; x++)
		satname[x]=string[x];

	satname[x]=0;
	x++;

	for (y=0; string[x+y]!=0 && string[x+y]!='\n'; y++)
		startstr[y]=string[x+y];

	startstr[y]=0;
	y++;

	for (z=0; string[x+y+z]!=0 && string[x+y+z]!='\n'; z++)
		endstr[z]=string[x+y+z];

	endstr[z]=0;

	/* Do a simple search for the matching satellite name */

	for (z=0; z<maxsats; z++) {
		if ((strcmp(sat[z].name,satname)==0) || (atol(satname)==sat[z].catnum)) {
			start=atol(startstr);

			if (endstr[strlen(endstr)-1]=='m') {
				step=60;
				endstr[strlen(endstr)-1]=0;
			}

			if (endstr[0]=='+')
				end=start+((long)step)*atol(endstr);
			else
				end=atol(endstr);

			indx=z;

			t=time(NULL);
			now=(long)t;

			if (start==0)
				start=now;

			if (startstr[0]=='+') {
				start=now;

				if (startstr[strlen(startstr)-1]=='m') {
					step=60;
					startstr[strlen(startstr)-1]=0;
				}

				end=start+((long)step)*atol(startstr);

				/* Prevent a list greater than
				   24 hours from being produced */

				if ((end-start)>86400) {
					start=now;
					end=now-1;
				}
			}

			if ((start>=now-31557600) && (start<=now+31557600) && end==0) {
				/* Start must be one year from now */
				/* Display a single position */
				daynum=((start/86400.0)-3651.0);
				PreCalc(indx);
				Calc();

				if (Decayed(indx,daynum)==0)
					fprintf(fd,"%ld %s %4d %4d %4d %4d %4d %6ld %6ld %c\n",start,Daynum2String(daynum,20,"%a %d%b%y %H:%M:%S"),iel,iaz,ma256,isplat,isplong,irk,rv,findsun);
				break;
			} else {
				/* Display a whole list */
				for (count=start; count<=end; count+=step) {
					daynum=((count/86400.0)-3651.0);
					PreCalc(indx);
					Calc();

					if (Decayed(indx,daynum)==0)
						fprintf(fd,"%ld %s %4d %4d %4d %4d %4d %6ld %6ld %c\n",count,Daynum2String(daynum,20,"%a %d%b%y %H:%M:%S"),iel,iaz,ma256,isplat,isplong,irk,rv,findsun);
				}
				break;
			}
		}
	}

	if (outputfile[0])
		fclose(fd);

	return 0;
}

int QuickPredict(string, outputfile)
char *string, *outputfile;
{
	int x, y, z, lastel=0;
	long start, now;
	char satname[50], startstr[20];
	time_t t;
	FILE *fd;

	if (outputfile[0])
		fd=fopen(outputfile,"w");
	else
		fd=stdout;

	startstr[0]=0;

	ReadDataFiles();

	for (x=0; x<48 && string[x]!=0 && string[x]!='\n'; x++)
		satname[x]=string[x];

	satname[x]=0;
	x++;

	for (y=0; string[x+y]!=0 && string[x+y]!='\n'; y++)
		startstr[y]=string[x+y];

	startstr[y]=0;
	y++;

	/* Do a simple search for the matching satellite name */

	for (z=0; z<maxsats; z++) {
		if ((strcmp(sat[z].name,satname)==0) || (atol(satname)==sat[z].catnum)) {
			start=atol(startstr);
			indx=z;

			t=time(NULL);
			now=(long)t;

			if (start==0)
				start=now;

			if ((start>=now-31557600) && (start<=now+31557600)) {
				/* Start must within one year of now */
				daynum=((start/86400.0)-3651.0);
				PreCalc(indx);
				Calc();

				if (AosHappens(indx) && Geostationary(indx)==0 && Decayed(indx,daynum)==0) {
					/* Make Predictions */
					daynum=FindAOS();

					/* Display the pass */

					while (iel>=0) {
						fprintf(fd,"%.0f %s %4d %4d %4d %4d %4d %6ld %6ld %c\n",floor(86400.0*(3651.0+daynum)),Daynum2String(daynum,20,"%a %d%b%y %H:%M:%S"),iel,iaz,ma256,isplat,isplong,irk,rv,findsun);
						lastel=iel;
						daynum+=cos((sat_ele-1.0)*deg2rad)*sqrt(sat_alt)/25000.0;
						Calc();
					}

					if (lastel!=0) {
						daynum=FindLOS();
						Calc();
						fprintf(fd,"%.0f %s %4d %4d %4d %4d %4d %6ld %6ld %c\n",floor(86400.0*(3651.0+daynum)),Daynum2String(daynum,20,"%a %d%b%y %H:%M:%S"),iel,iaz,ma256,isplat,isplong,irk,rv,findsun);
					}
				}
				break;
			}
		}
	}

	if (outputfile[0])
		fclose(fd);

	return 0;
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
	char updatefile[80], quickfind=0, quickpredict=0,
	     quickstring[40], outputfile[42],
	     tle_cli[50], qth_cli[50], interactive=0;
	char *env=NULL;
	FILE *db;
	struct addrinfo hints, *servinfo, *servinfop;

	/* Set up translation table for computing TLE checksums */

	for (x=0; x<=255; val[x]=0, x++);
	for (x='0'; x<='9'; val[x]=x-'0', x++);

	val['-']=1;

	updatefile[0]=0;
	quickstring[0]=0;
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

	for (x=1; x<=y; x++) {
		if (strcmp(argv[x],"-f")==0) {
			quickfind=1;
			z=x+1;
			while (z<=y && argv[z][0] && argv[z][0]!='-') {
				if ((strlen(quickstring)+strlen(argv[z]))<37) {
					strncat(quickstring,argv[z],15);
					strcat(quickstring,"\n");
					z++;
				}
			}
			z--;
		}

		if (strcmp(argv[x],"-p")==0) {
			quickpredict=1;
			z=x+1;
			while (z<=y && argv[z][0] && argv[z][0]!='-') {
				if ((strlen(quickstring)+strlen(argv[z]))<37) {
					strncat(quickstring,argv[z],15);
					strcat(quickstring,"\n");
					z++;
				}
			}
			z--;
		}

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

	if (updatefile[0] || quickfind || quickpredict)
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

	if (x==3)  /* Both TLE and QTH files were loaded successfully */ {
		if (quickfind)
			exit(QuickFind(quickstring,outputfile));

		if (quickpredict)
			exit(QuickPredict(quickstring,outputfile));
	} else {
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

		do {
			key=getch();

			if (key!='T')
				key=tolower(key);

			switch (key) {
				case 'p':
				case 'v':
					Print("",0);
					PrintVisible("");
					indx=Select();

					if (indx!=-1 && sat[indx].meanmo!=0.0 && Decayed(indx,0.0)==0) {
						const char *tle[2] = {sat[indx].line1, sat[indx].line2};
						predict_orbit_t *orbit = predict_create_orbit(predict_parse_tle(tle));
						predict_observer_t *observer = predict_create_observer("test_qth", qth.stnlat*M_PI/180.0, qth.stnlong*M_PI/180.0, qth.stnalt);
						Predict(orbit, observer, key);
					}

					MainMenu();
					break;

				case 'n': {
					Print("",0);
					predict_observer_t *observer = predict_create_observer("test_qth", qth.stnlat*M_PI/180.0, -qth.stnlong*M_PI/180.0, qth.stnalt);
					PredictSunMoon(PREDICT_MOON, observer);
					predict_destroy_observer(observer);
					MainMenu();
					break;
					  }

				case 'o': {
					Print("",0);
					predict_observer_t *observer = predict_create_observer("test_qth", qth.stnlat*M_PI/180.0, -qth.stnlong*M_PI/180.0, qth.stnalt);
					PredictSunMoon(PREDICT_SUN, observer);
					predict_destroy_observer(observer);
					MainMenu();
					break;
					  }

				case 'u':
					AutoUpdate("");
					MainMenu();
					break;

				case 'e':
					KepEdit();
					MainMenu();
					break;

				case 'd':
					ShowOrbitData();
					MainMenu();
					break;

				case 'g':
					QthEdit();
					MainMenu();
					break;

				case 't':
				case 'T':
					indx=Select();

					if (indx!=-1 && sat[indx].meanmo!=0.0 && Decayed(indx,0.0)==0) {
						const char *tle[2] = {sat[indx].line1, sat[indx].line2};
						predict_orbit_t *orbit = predict_create_orbit(predict_parse_tle(tle));
						predict_observer_t *observer = predict_create_observer("test_qth", qth.stnlat*M_PI/180.0, -qth.stnlong*M_PI/180.0, qth.stnalt);
						SingleTrack(horizon, orbit, observer, sat_db[indx]);
					}

					MainMenu();
					break;

				case 'm':
				case 'l':
					num_sats = totalsats;
					printf("%d, %d\n", ARRAY_SIZE(sat), num_sats);
					predict_orbit_t **orbits = (predict_orbit_t**)malloc(sizeof(predict_orbit_t*)*num_sats);
					for (int i=0; i < num_sats; i++){
						const char *tle[2] = {sat[i].line1, sat[i].line2};
						orbits[i] = predict_create_orbit(predict_parse_tle(tle));
						memcpy(orbits[i]->name, sat[i].name, 25);
					}
					predict_observer_t *observer = predict_create_observer("test_qth", qth.stnlat*M_PI/180.0, -qth.stnlong*M_PI/180.0, qth.stnalt);

					MultiTrack(observer, num_sats, orbits, key, 'k');
					for (int i=0; i < num_sats; i++){
						predict_destroy_orbit(orbits[i]);
					}
					free(orbits);

					MainMenu();
					break;

				case 'i':
					ProgramInfo();
					MainMenu();
					break;

				case 's':
					indx=Select();
					if (indx!=-1 && sat[indx].meanmo!=0.0 && Decayed(indx,0.0)==0) {
						Print("",0);
						const char *tle[2] = {sat[indx].line1, sat[indx].line2};
						predict_orbit_t *orbit = predict_create_orbit(predict_parse_tle(tle));
						
						Illumination(orbit);

						predict_destroy_orbit(orbit);
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
	}

	exit(0);
}
