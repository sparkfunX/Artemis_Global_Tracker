# Artemis Global Tracker: Flight Simulator

# Written by: Paul Clark (PaulZC)
# 29th February 2020

# Licence: MIT

# This code allows you to test the stitcher, CSD DateTime converter, CSV to KML converter
# and the Mapper by creating periodic .bin messages as if they had come from virtual
# Trackers flying between St James' Park (Newcastle, UK) and the Stadium of Light
# (Sunderland, UK). The messages are generated in pseudo real time, but you can edit the
# code to speed up the file generation if you want to.

# Uses Hannah Fry's Lat/Lon to OSGB Conversion Code: http://www.hannahfry.co.uk/blog/

from scipy import *
from math import degrees, sqrt, pi, sin, cos, tan, asin, atan, atan2 as arctan2
import csv
import time

print('Artemis Global Tracker: Flight Simulator')

def WGS84toOSGB36(lat, lon):
    '''Hannah's code to convert WGS84 Lat and Lon to OS Eastings and Northings'''

    #First convert to radians
    #These are on the wrong ellipsoid currently: GRS80. (Denoted by _1)
    lat_1 = lat*pi/180
    lon_1 = lon*pi/180

    #Want to convert to the Airy 1830 ellipsoid, which has the following:
    a_1, b_1 =6378137.000, 6356752.3141 #The GSR80 semi-major and semi-minor axes used for WGS84(m)
    e2_1 = 1- (b_1*b_1)/(a_1*a_1) #The eccentricity of the GRS80 ellipsoid
    nu_1 = a_1/sqrt(1-e2_1*sin(lat_1)**2)

    #First convert to cartesian from spherical polar coordinates
    H = 0 #Third spherical coord.
    x_1 = (nu_1 + H)*cos(lat_1)*cos(lon_1)
    y_1 = (nu_1+ H)*cos(lat_1)*sin(lon_1)
    z_1 = ((1-e2_1)*nu_1 +H)*sin(lat_1)

    #Perform Helmut transform (to go between GRS80 (_1) and Airy 1830 (_2))
    s = 20.4894*10**-6 #The scale factor -1
    tx, ty, tz = -446.448, 125.157, -542.060 #The translations along x,y,z axes respectively
    rxs,rys,rzs = -0.1502, -0.2470, -0.8421#The rotations along x,y,z respectively, in seconds
    rx, ry, rz = rxs*pi/(180*3600.), rys*pi/(180*3600.), rzs*pi/(180*3600.) #In radians
    x_2 = tx + (1+s)*x_1 + (-rz)*y_1 + (ry)*z_1
    y_2 = ty + (rz)*x_1+ (1+s)*y_1 + (-rx)*z_1
    z_2 = tz + (-ry)*x_1 + (rx)*y_1 +(1+s)*z_1

    #Back to spherical polar coordinates from cartesian
    #Need some of the characteristics of the new ellipsoid
    a, b = 6377563.396, 6356256.909 #The GSR80 semi-major and semi-minor axes used for WGS84(m)
    e2 = 1- (b*b)/(a*a) #The eccentricity of the Airy 1830 ellipsoid
    p = sqrt(x_2**2 + y_2**2)

    #Lat is obtained by an iterative proceedure:
    lat = arctan2(z_2,(p*(1-e2))) #Initial value
    latold = 2*pi
    while abs(lat - latold)>10**-16:
        lat, latold = latold, lat
        nu = a/sqrt(1-e2*sin(latold)**2)
        lat = arctan2(z_2+e2*nu*sin(latold), p)

    #Lon and height are then pretty easy
    lon = arctan2(y_2,x_2)
    H = p/cos(lat) - nu

    #E, N are the British national grid coordinates - eastings and northings
    F0 = 0.9996012717 #scale factor on the central meridian
    lat0 = 49*pi/180#Latitude of true origin (radians)
    lon0 = -2*pi/180#Longtitude of true origin and central meridian (radians)
    N0, E0 = -100000, 400000#Northing & easting of true origin (m)
    n = (a-b)/(a+b)

    #meridional radius of curvature
    rho = a*F0*(1-e2)*(1-e2*sin(lat)**2)**(-1.5)
    eta2 = nu*F0/rho-1

    M1 = (1 + n + (5/4)*n**2 + (5/4)*n**3) * (lat-lat0)
    M2 = (3*n + 3*n**2 + (21/8)*n**3) * sin(lat-lat0) * cos(lat+lat0)
    M3 = ((15/8)*n**2 + (15/8)*n**3) * sin(2*(lat-lat0)) * cos(2*(lat+lat0))
    M4 = (35/24)*n**3 * sin(3*(lat-lat0)) * cos(3*(lat+lat0))

    #meridional arc
    M = b * F0 * (M1 - M2 + M3 - M4)

    I = M + N0
    II = nu*F0*sin(lat)*cos(lat)/2
    III = nu*F0*sin(lat)*cos(lat)**3*(5- tan(lat)**2 + 9*eta2)/24
    IIIA = nu*F0*sin(lat)*cos(lat)**5*(61- 58*tan(lat)**2 + tan(lat)**4)/720
    IV = nu*F0*cos(lat)
    V = nu*F0*cos(lat)**3*(nu/rho - tan(lat)**2)/6
    VI = nu*F0*cos(lat)**5*(5 - 18* tan(lat)**2 + tan(lat)**4 + 14*eta2 - 58*eta2*tan(lat)**2)/120

    N = I + II*(lon-lon0)**2 + III*(lon- lon0)**4 + IIIA*(lon-lon0)**6
    E = E0 + IV*(lon-lon0) + V*(lon- lon0)**3 + VI*(lon- lon0)**5 

    #Job's a good'n.
    return E,N

def OSGB36toWGS84(E,N):
    '''Hannah's code to convert OS Eastings and Northings to WGS84 Lat and Lon'''

    #E, N are the British national grid coordinates - eastings and northings
    a, b = 6377563.396, 6356256.909     #The Airy 180 semi-major and semi-minor axes used for OSGB36 (m)
    F0 = 0.9996012717                   #scale factor on the central meridian
    lat0 = 49*pi/180                    #Latitude of true origin (radians)
    lon0 = -2*pi/180                    #Longtitude of true origin and central meridian (radians)
    N0, E0 = -100000, 400000            #Northing & easting of true origin (m)
    e2 = 1 - (b*b)/(a*a)                #eccentricity squared
    n = (a-b)/(a+b)

    #Initialise the iterative variables
    lat,M = lat0, 0

    while N-N0-M >= 0.00001: #Accurate to 0.01mm
        lat = (N-N0-M)/(a*F0) + lat;
        M1 = (1 + n + (5./4)*n**2 + (5./4)*n**3) * (lat-lat0)
        M2 = (3*n + 3*n**2 + (21./8)*n**3) * sin(lat-lat0) * cos(lat+lat0)
        M3 = ((15./8)*n**2 + (15./8)*n**3) * sin(2*(lat-lat0)) * cos(2*(lat+lat0))
        M4 = (35./24)*n**3 * sin(3*(lat-lat0)) * cos(3*(lat+lat0))
        #meridional arc
        M = b * F0 * (M1 - M2 + M3 - M4)          

    #transverse radius of curvature
    nu = a*F0/sqrt(1-e2*sin(lat)**2)

    #meridional radius of curvature
    rho = a*F0*(1-e2)*(1-e2*sin(lat)**2)**(-1.5)
    eta2 = nu/rho-1

    secLat = 1./cos(lat)
    VII = tan(lat)/(2*rho*nu)
    VIII = tan(lat)/(24*rho*nu**3)*(5+3*tan(lat)**2+eta2-9*tan(lat)**2*eta2)
    IX = tan(lat)/(720*rho*nu**5)*(61+90*tan(lat)**2+45*tan(lat)**4)
    X = secLat/nu
    XI = secLat/(6*nu**3)*(nu/rho+2*tan(lat)**2)
    XII = secLat/(120*nu**5)*(5+28*tan(lat)**2+24*tan(lat)**4)
    XIIA = secLat/(5040*nu**7)*(61+662*tan(lat)**2+1320*tan(lat)**4+720*tan(lat)**6)
    dE = E-E0

    #These are on the wrong ellipsoid currently: Airy1830. (Denoted by _1)
    lat_1 = lat - VII*dE**2 + VIII*dE**4 - IX*dE**6
    lon_1 = lon0 + X*dE - XI*dE**3 + XII*dE**5 - XIIA*dE**7

    #Want to convert to the GRS80 ellipsoid. 
    #First convert to cartesian from spherical polar coordinates
    H = 0 #Third spherical coord. 
    x_1 = (nu/F0 + H)*cos(lat_1)*cos(lon_1)
    y_1 = (nu/F0+ H)*cos(lat_1)*sin(lon_1)
    z_1 = ((1-e2)*nu/F0 +H)*sin(lat_1)

    #Perform Helmut transform (to go between Airy 1830 (_1) and GRS80 (_2))
    s = -20.4894*10**-6 #The scale factor -1
    tx, ty, tz = 446.448, -125.157, + 542.060 #The translations along x,y,z axes respectively
    rxs,rys,rzs = 0.1502,  0.2470,  0.8421  #The rotations along x,y,z respectively, in seconds
    rx, ry, rz = rxs*pi/(180*3600.), rys*pi/(180*3600.), rzs*pi/(180*3600.) #In radians
    x_2 = tx + (1+s)*x_1 + (-rz)*y_1 + (ry)*z_1
    y_2 = ty + (rz)*x_1  + (1+s)*y_1 + (-rx)*z_1
    z_2 = tz + (-ry)*x_1 + (rx)*y_1 +  (1+s)*z_1

    #Back to spherical polar coordinates from cartesian
    #Need some of the characteristics of the new ellipsoid    
    a_2, b_2 =6378137.000, 6356752.3141 #The GSR80 semi-major and semi-minor axes used for WGS84(m)
    e2_2 = 1- (b_2*b_2)/(a_2*a_2)   #The eccentricity of the GRS80 ellipsoid
    p = sqrt(x_2**2 + y_2**2)

    #Lat is obtained by an iterative proceedure:   
    lat = arctan2(z_2,(p*(1-e2_2))) #Initial value
    latold = 2*pi
    while abs(lat - latold)>10**-16: 
        lat, latold = latold, lat
        nu_2 = a_2/sqrt(1-e2_2*sin(latold)**2)
        lat = arctan2(z_2+e2_2*nu_2*sin(latold), p)

    #Lon and height are then pretty easy
    lon = arctan2(y_2,x_2)
    H = p/cos(lat) - nu_2

#Uncomment this line if you want to print the results
    #print [(lat-lat_1)*180/pi, (lon - lon_1)*180/pi]

    #Convert to degrees
    lat = lat*180/pi
    lon = lon*180/pi

    #Job's a good'n. 
    return lat, lon

def calc_ground_speed(start_e, start_n, end_e, end_n, interval):
    '''Calculate the equivalent ground speed in m/s'''
    dist_e = end_e - start_e
    dist_n = end_n - start_n
    dist = sqrt(dist_e**2 + dist_n**2)
    return (dist / interval)

def calc_heading(start_e, start_n, end_e, end_n):
    '''Calculate the equivalent heading in degrees'''
    # https://stackoverflow.com/a/5060108
    dist_e = end_e - start_e
    dist_n = end_n - start_n
    angle = degrees(arctan2(dist_n, dist_e))
    return ((90. - angle) % 360.)

def write_files(imei, momsn, date, hour, lat, lon, alt, speed, heading, num_trackers, lon_offset):
    '''Write the time and position data to .bin file'''
    if (num_trackers < 1) or (num_trackers > 8): return
    for i in range(0, int(num_trackers)):
        filename = str(imei + i) + '-' + str(momsn) + '.bin'
        outputFile = open(filename, 'w', newline='') # Python3 csv needs w not wb
        output=csv.writer(outputFile,delimiter=',')
        datetime = date + time.strftime('%H%M%S', time.gmtime((hour * 3600.)))
        lat_str = '%0.7f' % lat
        lon_str = '%0.7f' % (lon + (lon_offset * i))
        alt_str = '%0.2f' % alt
        speed_str = '%0.1f' % speed
        head_str = '%0.1f' % heading
        line = [datetime, lat_str, lon_str, alt_str, speed_str, head_str]
        print('Writing ' + str(line) + ' to ' + filename)
        output.writerow(line)
        outputFile.close()

# St James Park
# 54°58'32.12"N  54.97558889
# 1°37'17.92"W   -1.621644444
# 68m

# Stadium Of Light
# 54°54'51.94"N  54.91442778
# 1°23'17.43"W   -1.388175
# 30m

# Choose the flight parameters so the flight takes place within one day
# (The code will not increment the date if the flight goes over midnight)
date = '20200229' # Date in YYYYMMDD format
hour = 12. # Start time (Hours)

# These values are all float. Don't forget to add the . at the end
start_e, start_n = WGS84toOSGB36(54.97558889, -1.621644444)
start_alt = 68. # m
end_e, end_n = WGS84toOSGB36(54.91442778, -1.388175)
end_alt = 30.
max_alt = 30000. # m
flight_time = 360. # sec
interval = 15. # sec
pre_dwell = 15. # sec
post_dwell = 15. # sec

num_trackers = 8 # the number of trackers you want to simulate (1-8) (int)
lon_offset = 0.0001 # the longitude offset between trackers (degrees) (float)

start_imei = 123456789012345 # IMEI: 15 digits
momsn = 0 # MOMSN starting value (increases by 1 for each message)

dist_e = end_e - start_e # The ground distance travelled east
dist_n = end_n - start_n # The ground distance travelled north
# The number of flight steps or increments
increments = float(int(flight_time / interval))
# The flight trajectory is pure sinusoidal, so we need to calculate the 'elevation'
# (angular offset) between start and finish
end_angle = pi - asin((end_alt - start_alt) / (max_alt - start_alt))

# The start time (so we can generate files in pseudo real time)
last_time = time.time()

#Pre dwell
if pre_dwell > 0:
    lat, lon = OSGB36toWGS84(float(start_e), float(start_n))
    for l in range(int(pre_dwell / interval)):
        write_files(start_imei, momsn, date, hour, lat, lon, start_alt, 0., 0., num_trackers, lon_offset)
        while (time.time() < (last_time + interval)):
            pass # Wait for interval to elapse
        last_time = last_time + interval # Keep track of time
        momsn = momsn + 1 # Increment the momsn
        hour = hour + (interval / 3600.) # Increment the time (in hours)

last_e = start_e
last_n = start_n

#Loop through the data
for l in range(int(increments) + 1):
    current_e = start_e + (dist_e * l / increments)
    current_n = start_n + (dist_n * l / increments)
    current_alt = start_alt + ((max_alt - start_alt) * sin(end_angle * l / increments))
    lat, lon = OSGB36toWGS84(float(current_e), float(current_n))
    speed = calc_ground_speed(last_e, last_n, current_e, current_n, interval)
    heading = calc_heading(last_e, last_n, current_e, current_n)
    write_files(start_imei, momsn, date, hour, lat, lon, current_alt, speed, heading, num_trackers, lon_offset)
    while (time.time() < (last_time + interval)):
        pass # Wait for interval to elapse
    last_time = last_time + interval # Keep track of time
    momsn = momsn + 1 # Increment the momsn
    hour = hour + (interval / 3600.) # Increment the time (in hours)
    last_e = current_e # Update last_e/n for calc_speed & calc_heading
    last_n = current_n

#Post dwell
if post_dwell > 0:
    for l in range(int(post_dwell / interval)):
        write_files(start_imei, momsn, date, hour, lat, lon, current_alt, 0., 0., num_trackers, lon_offset)
        while (time.time() < (last_time + interval)):
            pass # Wait for interval to elapse
        last_time = last_time + interval # Keep track of time
        momsn = momsn + 1 # Increment the momsn
        hour = hour + (interval / 3600.) # Increment the time (in hours)

print('Done!')
