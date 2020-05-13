# Artemis Global Tracker: Convert CSV DateTime

# Written by: Paul Clark (PaulZC)
# 29th February 2020

# Licence: MIT

# This code converts the first column of a Global Tracker csv file
# from YYYYMMDDHHMMSS format into a more friendly DD/MM/YY HH:MM:SS format

import csv
from datetime import datetime
import os
import codecs

print('Artemis Global Tracker: CSV DateTime Converter')
print

resp = 'N'

# Find the csv file
for root, dirs, files in os.walk("."):
    if len(files) > 0:
        # Comment out the next two lines to process all files in this directory and its subdirectories
        # Uncomment one or the other to search only this directory or only subdirectories
        #if root != ".": # Only check sub directories
        if root == ".": # Only check this directory
            for filename in files:
                if filename[-4:] == '.csv':
                    longfilename = os.path.join(root, filename)
                    question =  'Open ' + filename + '? (Y/n) : '
                    resp = input(question)
                    if resp == '' or resp == 'Y' or resp == 'y': break
                    longfilename = ''
                if resp == '' or resp == 'Y' or resp == 'y': break
            if resp == '' or resp == 'Y' or resp == 'y': break
    if resp == '' or resp == 'Y' or resp == 'y': break
                    
if longfilename == '': raise Exception('No file to open!')

outfile = longfilename[:-4] + '_DateTime' + longfilename[-4:]

with open(outfile,"w", newline='') as dest:
    with open(longfilename, "r") as source:
        reader = csv.reader(source)
        writer = csv.writer(dest)
        for line in reader:
            try:
                if len(line) > 6: # Check it has sufficient fields
                    if (line[0][:2] == 'RB') : # Does the message payload have an RB prefix?
                        dt = datetime.strptime(line[1],'%Y%m%d%H%M%S')
                        line.append(line[-1])
                        for l in range((len(line)-2), 1, -1):
                            line[l+1] = line[l]
                        line[1] = dt.strftime('%d/%m/%Y')
                        line[2] = dt.strftime('%H:%M:%S')
                    else:
                        dt = datetime.strptime(line[0],'%Y%m%d%H%M%S')
                        line.append(line[-1])
                        for l in range((len(line)-2), 0, -1):
                            line[l+1] = line[l]
                        line[0] = dt.strftime('%d/%m/%Y')
                        line[1] = dt.strftime('%H:%M:%S')
                writer.writerow(line)
            except:
                pass
        source.close()
    dest.close()
