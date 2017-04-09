#include "defines.h"
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>

double reduce(double value, double rangeMin, double rangeMax)
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

void latlon_to_maidenhead(double mLtd, double mLng, char* mStr)
{
	int i, j, k, l, m, n;
	mLng = -mLng; //convert to N/W

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
}

void maidenhead_to_latlon(const char *locator, double *ret_longitude, double *ret_latitude)
{
	//using the same setup as in getMaidenhead defined above, and applying the algorithm in reverse
	int i, j, k, l, m, n;

	double mLng = 0, mLtd = 0;
	int pos = 0;
	if (strlen(locator) > pos) {
		i = (int)(toupper(locator[pos]) - 'A');
		mLng = i*20.0;
	}
	pos++;
	if (strlen(locator) > pos) {
		j = (int)(toupper(locator[pos]) - 'A');
		mLtd = j*10.0;
	}
	pos++;
	if (strlen(locator) > pos) {
		k = (int)(locator[pos] - '0');
		mLng += k*2.0;
	}
	pos++;
	if (strlen(locator) > pos) {
		l = (int)(locator[pos] - '0');
		mLtd += l*1.0;
	}
	pos++;
	if (strlen(locator) > pos) {
		m = (int)(toupper(locator[pos]) - 'A');
		mLng += (m+0.5)/12.0;
	}
	pos++;
	if (strlen(locator) > pos) {
		n = (int)(toupper(locator[pos]) - 'A');
		mLtd += (n+0.5)/24.0;
	}
	*ret_longitude = mLng - 180.0;
	*ret_latitude = mLtd - 90.0;
}

