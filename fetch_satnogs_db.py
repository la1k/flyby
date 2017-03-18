#/bin/py :D
import urllib2
import json
from operator import itemgetter
from os.path import expanduser

#This is a script that fetches transponder data from db.satnogs.org and parses it to flyby transponder data.

#Step 1: Fetch transponder information from SatNOGS db.
tempfile="/tmp/flybydb"
dburl="https://db.satnogs.org/api/transmitters/"

request = urllib2.urlopen(dburl)
data = json.load(request)

#Step 2: Generate transponder database as expected by flyby.

#Flyby expects this format:

# Satellite name - can be anything
# NORAD number - must match
# Line containing string == "No alat, alon" could calculate squint angle with this parameter
# -- Transponder data, there can be multiple of these for a given norad entry
# String containing name of transponder - can be anything
# Uplink frequency in MHz [start,stop], fill with [0.0,0.0] if n/a
# Downling frequency in MHz [start,stop], fill with [0.0,0.0] if n/a
# String containing "No weekly schedule" - must be filler, has no purpose
# String containing "No orbital schedule" - must be filled, has no purpose
# -- Transponder data end
# end - End of transponder entry for given NORAD

sorteddata = sorted(data, key=itemgetter('norad_cat_id')) #Sort data based on norad entry number

with open(expanduser("~")+"/.local/share/flyby/flyby.db","w") as db: #Append to textfile
    previous_norad_id = False

    for i in sorteddata: # Iterate through all entries in SatNOGS database.
        for key in ['uplink_low','uplink_high','downlink_low','downlink_high', 'baud']: # Sanity checks
            if i[key] is None:
                i[key] = 0
        if previous_norad_id != False: # End previous NORAD entry, except for first iteration
            db.write("end\n")
        if i['norad_cat_id'] != previous_norad_id: # 
            # Start new norad transponder entry
            db.write("%s\n" %i['uuid']) # Name not used -> place UUID as dummy name
            db.write("%s\n" %i['norad_cat_id']) # Place norad_id
            db.write("No alat, alon\n") # Fill for compliance
            
        previous_norad_id = i['norad_cat_id']
        
        #Add transponder data
        db.write("%s - Alive = %r - Inverting = %r - Baud = %f\n" %(i['description'],i['alive'], i['invert'], i['baud']))
        db.write("%f, %f\n" %(i['uplink_low']/1e6,i['uplink_high']/1e6)) # Uplink frequency fill
        db.write("%f, %f\n" %((i['downlink_low']/1e6),(i['downlink_high']/1e6))) # Downlink frequency fill
        db.write("No weekly schedule\n") # Fill for compliance
        db.write("No orbital schedule\n") # Fill for compliance
    
    db.write("end\n")
