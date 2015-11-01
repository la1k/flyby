



void PreCalc(x)
int x;
{
	/* This function copies TLE data from flyby's sat structure
	   to the SGP4/SDP4's single dimensioned tle structure, and
	   prepares the tracking code for the update. */

	strcpy(tle.sat_name,sat[x].name);
	strcpy(tle.idesg,sat[x].designator);
	tle.catnr=sat[x].catnum;
	tle.epoch=(1000.0*(double)sat[x].year)+sat[x].refepoch;
	tle.xndt2o=sat[x].drag;
	tle.xndd6o=sat[x].nddot6;
	tle.bstar=sat[x].bstar;
	tle.xincl=sat[x].incl;
	tle.xnodeo=sat[x].raan;
	tle.eo=sat[x].eccn;
	tle.omegao=sat[x].argper;
	tle.xmo=sat[x].meanan;
	tle.xno=sat[x].meanmo;
	tle.revnum=sat[x].orbitnum;

	if (sat_db[x].squintflag) {
		calc_squint=1;
		alat=deg2rad*sat_db[x].alat;
		alon=deg2rad*sat_db[x].alon;
	} else
		calc_squint=0;

	/* Clear all flags */

	ClearFlag(ALL_FLAGS);

	/* Select ephemeris type.  This function will set or clear the
	   DEEP_SPACE_EPHEM_FLAG depending on the TLE parameters of the
	   satellite.  It will also pre-process tle members for the
	   ephemeris functions SGP4 or SDP4, so this function must
	   be called each time a new tle set is used. */

	select_ephemeris(&tle);
}

void Calc()
{
	/* This is the stuff we need to do repetitively... */

	/* Zero vector for initializations */
	vector_t zero_vector={0,0,0,0};

	/* Satellite position and velocity vectors */
	vector_t vel=zero_vector;
	vector_t pos=zero_vector;

	/* Satellite Az, El, Range, Range rate */
	vector_t obs_set;

	/* Solar ECI position vector  */
	vector_t solar_vector=zero_vector;

	/* Solar observed azi and ele vector  */
	vector_t solar_set;

	/* Satellite's predicted geodetic position */
	geodetic_t sat_geodetic;

	jul_utc=daynum+2444238.5;

	/* Convert satellite's epoch time to Julian  */
	/* and calculate time since epoch in minutes */

	jul_epoch=Julian_Date_of_Epoch(tle.epoch);
	tsince=(jul_utc-jul_epoch)*xmnpda;
	age=jul_utc-jul_epoch;

	/* Copy the ephemeris type in use to ephem string. */

		if (isFlagSet(DEEP_SPACE_EPHEM_FLAG))
			strcpy(ephem,"SDP4");
		else
			strcpy(ephem,"SGP4");

	/* Call NORAD routines according to deep-space flag. */

	if (isFlagSet(DEEP_SPACE_EPHEM_FLAG))
		SDP4(tsince, &tle, &pos, &vel);
	else
		SGP4(tsince, &tle, &pos, &vel);

	/* Scale position and velocity vectors to km and km/sec */

	Convert_Sat_State(&pos, &vel);

	/* Calculate velocity of satellite */

	Magnitude(&vel);
	sat_vel=vel.w;

	/** All angles in rads. Distance in km. Velocity in km/s **/
	/* Calculate satellite Azi, Ele, Range and Range-rate */

	Calculate_Obs(jul_utc, &pos, &vel, &obs_geodetic, &obs_set);

	/* Calculate satellite Lat North, Lon East and Alt. */

	Calculate_LatLonAlt(jul_utc, &pos, &sat_geodetic);

	/* Calculate squint angle */

	if (calc_squint)
		squint=(acos(-(ax*rx+ay*ry+az*rz)/obs_set.z))/deg2rad;

	/* Calculate solar position and satellite eclipse depth. */
	/* Also set or clear the satellite eclipsed flag accordingly. */

	Calculate_Solar_Position(jul_utc, &solar_vector);
	Calculate_Obs(jul_utc, &solar_vector, &zero_vector, &obs_geodetic, &solar_set);

	if (Sat_Eclipsed(&pos, &solar_vector, &eclipse_depth))
		SetFlag(SAT_ECLIPSED_FLAG);
	else
		ClearFlag(SAT_ECLIPSED_FLAG);

	if (isFlagSet(SAT_ECLIPSED_FLAG))
		sat_sun_status=0;  /* Eclipse */
	else
		sat_sun_status=1; /* In sunlight */

	/* Convert satellite and solar data */
	sat_azi=Degrees(obs_set.x);
	sat_ele=Degrees(obs_set.y);
	sat_range=obs_set.z;
	sat_range_rate=obs_set.w;
	sat_lat=Degrees(sat_geodetic.lat);
	sat_lon=Degrees(sat_geodetic.lon);
	sat_alt=sat_geodetic.alt;

	fk=12756.33*acos(xkmper/(xkmper+sat_alt));
	fm=fk/1.609344;

	rv=(long)floor((tle.xno*xmnpda/twopi+age*tle.bstar*ae)*age+tle.xmo/twopi)+tle.revnum;

	sun_azi=Degrees(solar_set.x);
	sun_ele=Degrees(solar_set.y);

	irk=(long)rint(sat_range);
	isplat=(int)rint(sat_lat);
	isplong=(int)rint(360.0-sat_lon);
	iaz=(int)rint(sat_azi);
	iel=(int)rint(sat_ele);
	ma256=(int)rint(256.0*(phase/twopi));

	if (sat_sun_status) {
		if (sun_ele<=-12.0 && rint(sat_ele)>=0.0)
			findsun='+';
		else
			findsun='*';
	} else
		findsun=' ';
}

double FindAOS()
{
	/* This function finds and returns the time of AOS (aostime). */

	aostime=0.0;

	if (AosHappens(indx) && Geostationary(indx)==0 && Decayed(indx,daynum)==0) {
		Calc();

		/* Get the satellite in range */

		while (sat_ele<-1.0) {
			daynum-=0.00035*(sat_ele*((sat_alt/8400.0)+0.46)-2.0);
			Calc();
		}

		/* Find AOS */

		while (aostime==0.0) {
			if (fabs(sat_ele)<0.03)
				aostime=daynum;
			else {
				daynum-=sat_ele*sqrt(sat_alt)/530000.0;
				Calc();
			}
		}
	}

	return aostime;
}

double FindLOS()
{
	lostime=0.0;

	if (Geostationary(indx)==0 && AosHappens(indx)==1 && Decayed(indx,daynum)==0) {
		Calc();

		do {
			daynum+=sat_ele*sqrt(sat_alt)/502500.0;
			Calc();

			if (fabs(sat_ele) < 0.03)
				lostime=daynum;

		} while (lostime==0.0);
	}

	return lostime;
}

double FindLOS2()
{
	/* This function steps through the pass to find LOS.
	   FindLOS() is called to "fine tune" and return the result. */

	do {
		daynum+=cos((sat_ele-1.0)*deg2rad)*sqrt(sat_alt)/25000.0;
		Calc();

	} while (sat_ele>=0.0);

	return(FindLOS());
}

double NextAOS()
{
	/* This function finds and returns the time of the next
	   AOS for a satellite that is currently in range. */

	aostime=0.0;

	if (AosHappens(indx) && Geostationary(indx)==0 && Decayed(indx,daynum)==0)
		daynum=FindLOS2()+0.014;  /* Move to LOS + 20 minutes */

	return (FindAOS());
}
