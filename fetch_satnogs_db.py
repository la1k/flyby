#This is a script that fetches transponder data from db.satnogs.org and parses it to flyby transponder data.
import urllib2
import json
import sys
from operator import itemgetter
from os.path import expanduser

# The script will overwrite the old database, user must agree to this.
while True:
    prompt = raw_input("This will overwrite the existing database at $HOME/.local/share/flyby/flyby.db, do you want to proceed? (yes/no): ")
    if prompt == "yes" or prompt == "y" or prompt == "Yes": 
        break
    elif prompt == "no" or prompt == "n" or prompt == "No":
        sys.exit("Script terminated")

#Step 1: Fetch JSON transponder information from SatNOGS db.
request = urllib2.urlopen("https://db.satnogs.org/api/transmitters")
data = json.load(request)

# Order data by norad_cat_id entry
sorteddata = sorted(data, key=itemgetter('norad_cat_id'))

#Step 2: Generate transponder database as expected by flyby.

# Write to $HOME/.local/share/flyby/flyby.db
with open(expanduser("~")+"/.local/share/flyby/flyby.db","w") as db:
    previous_norad_id = False

    # Iterate through SatNOGS database
    for transponder in sorteddata:
        
        # Fill null fields with 0
        for key in ['uplink_low','uplink_high','downlink_low','downlink_high', 'baud']:
            if transponder[key] is None:
                transponder[key] = 0

        # Single frequency transponders should have same high and low frequency
        if transponder['uplink_high'] == 0:
            transponder['uplink_high'] = transponder['uplink_low']
        if transponder['downlink_high'] == 0:
            transponder['downlink_high'] = transponder['downlink_low']

        # End previous NORAD entry, except for first iteration
        if previous_norad_id != transponder['norad_cat_id'] and previous_norad_id != False: 
            db.write("end\n")

        # Start new transponder entry
        if transponder['norad_cat_id'] != previous_norad_id: 
            db.write("%s\n" %transponder['uuid']) 
            db.write("%s\n" %transponder['norad_cat_id'])
            db.write("No alat, alon\n")
            
        previous_norad_id = transponder['norad_cat_id']
        
        # Add transponder data
        db.write("%s" %(transponder['description']))
        if transponder['uplink_low']>0 and transponder['downlink_low']>0:
            if transponder['invert']:
                db.write(" - Inverting")
            else:
                db.write(" - Non-inverting")
        if transponder['baud']>0:
            db.write(" - Baud = %s " %transponder['baud'])
        if not transponder['alive']:
            db.write(" - (dead)\n")
        else:
            db.write("\n")

        # Convert frequency input to MHz
        db.write("%f, %f\n" %(transponder['uplink_low']/1e6,transponder['uplink_high']/1e6))
        db.write("%f, %f\n" %((transponder['downlink_low']/1e6),(transponder['downlink_high']/1e6)))
        db.write("No weekly schedule\n")
        db.write("No orbital schedule\n")
    
    db.write("end\n")
