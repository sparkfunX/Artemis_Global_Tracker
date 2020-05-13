# Artemis Global Tracker: Stitcher

# Written by: Paul Clark (PaulZC)
# 29th February 2020

# Licence: MIT

# Stitches .bin SBD files from the Global Tracker into .csv files

# This code searches through the current directory and/or sub-directories,
# finds all .bin SBD files (downloaded by Artemis_Global_Tracker_GMail_Downloader.py)
# and converts them into .csv files.

# Files from different trackers (with different IMEIs) are processed separately.
# The MOMSN is added to the end of each line.

# Rock7 RockBLOCK SBD filenames have the format IMEI-MOMSN.bin where:
# IMEI is the International Mobile Equipment Identity number (15 digits)
# MOMSN is the Mobile Originated Message Sequence Number (1+ digits)
# Other Iridium service providers use different filename conventions.

# All files get processed. You will need to 'hide' files you don't
# want to process by moving them to (e.g.) a different directory.

import numpy as np
import matplotlib.dates as mdates
import os
import re

# https://stackoverflow.com/a/2669120
def sorted_nicely(l): 
   """ Sort the given iterable in the way that humans expect.""" 
   convert = lambda text: int(text) if text.isdigit() else text 
   alphanum_key = lambda key: [ convert(c) for c in re.split('([0-9]+)', key) ] 
   return sorted(l, key = alphanum_key)

# list of imeis
imeis = []

# csv filenames
csv_filenames = []

print('Artemis Global Tracker: Stitcher')
print

# Ask the user if they want to Overwrite or Append existing sbd files
try:
   overwrite_files = input('Do you want to Overwrite or Append_To existing csv files? (O/A) (Default: O) : ')
except:
   overwrite_files = 'O'
if (overwrite_files != 'O') and (overwrite_files != 'o') and (overwrite_files != 'A') and (overwrite_files != 'a'):
   overwrite_files = 'O'
if (overwrite_files == 'o'): overwrite_files = 'O'

print

# Identify all .bin SBD files
for root, dirs, files in os.walk(".", followlinks=False):
    
    # Comment out the next two lines to process all files in this directory and its subdirectories
    # Uncomment one or the other to search only this directory or only subdirectories
    #if root != ".": # Ignore files in this directory - only process subdirectories
    if root == ".": # Ignore subdirectories - only process this directory

    # Uncomment and modify the next two lines to only process a single subdirectory
    #search_me = "Test_Messages" # Search this subdirectory
    #if root == os.path.join(".",search_me): # Only process files in this subdirectory
    
        if len(files) > 0:
            # Find filenames with the correct format (imei-momsn.bin)
            valid_files = [afile for afile in files if ((afile[-4:] == '.bin') and (afile[15:16] == '-'))]
        else:
            valid_files = []
            
        if len(valid_files) > 0:
            for filename in sorted_nicely(valid_files):
                longfilename = os.path.join(root, filename)
                momsn = filename[16:-4] # Get the momsn
                imei = filename[0:15] # Get the imei

                print('Found SBD file from beacon IMEI',imei,'with MOMSN',momsn)
               
                # Check if this new file is from a beacon imei we haven't seen before
                if imei in imeis:
                    pass # We have seen this one before
                else:
                    imeis.append(imei) # New imei so add it to the list
                    csv_filenames.append('%s.csv'%imei) # Create the csv filename
                    if (overwrite_files == 'O'):
                        fp = open(csv_filenames[-1],'w') # Create the csv file (clear it if it already exists)
                        fp.close()

                index = imeis.index(imei) # Get the imei index

                fp = open(csv_filenames[index],'a') # Open the csv file for append
                fr = open(longfilename,'r') # Open the SBD file for read
                the_sbd = fr.read() # Read the SBD data
                if (ord(the_sbd[-2]) == 13) and (ord(the_sbd[-1]) == 10):
                   the_sbd = the_sbd[:-2] # Strip CRLF is present
                if (ord(the_sbd[-1]) == 13):
                   the_sbd = the_sbd[:-1] # Strip CR is present
                if (ord(the_sbd[-1]) == 10):
                   the_sbd = the_sbd[:-1] # Strip LF is present
                fp.write(the_sbd) # Copy the SBD data into the csv file
                fp.write(',') # Add a comma
                fp.write(momsn) # Add the MOMSN
                fp.write('\n') # Add LF
                fr.close() # Close the SBD file
                fp.close() # Close the csv file




           
    
