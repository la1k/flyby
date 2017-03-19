#This is a script that fetches transponder data from db.satnogs.org and parses it to flyby transponder data.
import urllib2
import json
from operator import itemgetter
from os.path import expanduser



#Step 1: Fetch JSON transponder information from SatNOGS db.
request = urllib2.urlopen("https://db.satnogs.org/api/transmitters")
data = json.load(request)

#Step 2: Generate transponder database as expected by flyby.
sorteddata = sorted(data, key=itemgetter('norad_cat_id')) #Sort data based on norad entry number

with open(expanduser("~")+"/.local/share/flyby/flyby.db","w") as db: #Append to textfile
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

        if previous_norad_id != i['norad_cat_id'] and previous_norad_id != False: 
            # End previous NORAD entry, except for first iteration
            db.write("end\n")
        if i['norad_cat_id'] != previous_norad_id: # 
            # Start new norad transponder entry
            db.write("%s\n" %i['uuid']) # Name not used -> place UUID as dummy name
            db.write("%s\n" %i['norad_cat_id']) # Place norad_id
            db.write("No alat, alon\n") # Fill for compliance
            
        previous_norad_id = i['norad_cat_id']
        
        #Add transponder data
        db.write("%s - Inverting = %r " %(i['description'], i['invert']))
        if not i['baud']:
            db.write("- Baud = %s " %i['baud'])
        if not i['alive']:
            db.write("Not currently alive\n")
        else:
            db.write("\n")
        db.write("%f, %f\n" %(i['uplink_low']/1e6,i['uplink_high']/1e6))
        db.write("%f, %f\n" %((i['downlink_low']/1e6),(i['downlink_high']/1e6)))
        db.write("No weekly schedule\n")
        db.write("No orbital schedule\n")
    
    db.write("end\n")
