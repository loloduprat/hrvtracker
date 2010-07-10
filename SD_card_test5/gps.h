#ifndef   __GPS_H
  #define __GPS_H


#define GPS_CHECKSUM(mstr, mx)  {mx=0; for(int mi = 0; mi < strlen(mstr);mi++ ) mx ^= mstr[mi];}


//public functions
//char compareChecksum(const char *gps_string){
//void parseRMC(const char *gps_string){
int parseGPS(const char *gps_string);


//public data
extern char GPS_LatPos[15];
extern char GPS_LonPos[15];
extern char GPS_LatDir;
extern char GPS_LonDir;
extern char GPS_Time[10];
extern char GPS_Alt[10];
extern char GPS_Fix;

#endif //__GPS_H