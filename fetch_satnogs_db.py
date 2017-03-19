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

#Step 2: Generate transponder database as expected by flyby.

# Order data by norad_cat_id entry
sorteddata = sorted(data, key=itemgetter('norad_cat_id'))

# Write to $HOME/.local/share/flyby/flyby.db
with open(expanduser("~")+"/.local/share/flyby/flyby.db","w") as db:
    previous_norad_id = False

    for i in sorteddata: # Iterate through all entries in SatNOGS database.
        
        # Fill null fields with 0
        for key in ['uplink_low','uplink_high','downlink_low','downlink_high', 'baud']:
            if i[key] is None:
                i[key] = 0

        # Single frequency transponders should have same high and low frequency
        if i['uplink_high'] == 0:
            i['uplink_high'] = i['uplink_low']
        if i['downlink_high'] == 0:
            i['downlink_high'] = i['downlink_low']

        # End previous NORAD entry, except for first iteration
        if previous_norad_id != i['norad_cat_id'] and previous_norad_id != False: 
            db.write("end\n")

        # Start new transponder entry
        if i['norad_cat_id'] != previous_norad_id: 
            db.write("%s\n" %i['uuid']) 
            db.write("%s\n" %i['norad_cat_id'])
            db.write("No alat, alon\n")
            
        previous_norad_id = i['norad_cat_id']
        
        # Add transponder data
        db.write("%s - Inverting = %r " %(i['description'], i['invert']))
        if not i['baud']:
            db.write("- Baud = %s " %i['baud'])
        if not i['alive']:
            db.write("Not currently alive\n")
        else:
            db.write("\n")
        # Convert frequency input to MHz
        db.write("%f, %f\n" %(i['uplink_low']/1e6,i['uplink_high']/1e6))
        db.write("%f, %f\n" %((i['downlink_low']/1e6),(i['downlink_high']/1e6)))
        db.write("No weekly schedule\n")
        db.write("No orbital schedule\n")
    
    db.write("end\n")
