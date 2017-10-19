import sys
from math import sin,cos,asin,atan2,sqrt,pow,pi

def rad(deg):
    return (deg * pi)/180.0

def distance(lat1,lon1,lat2,lon2):
    # radius of earch in km
    R = 6371.0
    lat = rad(lat2 - lat1)
    lon = rad(lon2 - lon1)
    a = pow(sin(lat/2),2) + cos(rad(lat1)) * cos(rad(lat2)) * pow(sin(lon/2),2)
    return 2 * R * atan2(sqrt(a),sqrt(1-a))

if __name__ == '__main__':
    lat1 = float(sys.argv[1]) 
    lon1 = float(sys.argv[2])
    lat2 = float(sys.argv[3])
    lon2 = float(sys.argv[4])
    print distance(lat1,lon1,lat2,lon2)
