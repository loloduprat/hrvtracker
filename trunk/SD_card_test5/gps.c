//Functions to parse messages from the GPS receiver
//
// Aaron Le Compte
// June 2010
#include <string.h>
#include "gps.h"
#include "main.h"


//Current GPS data
char GPS_LatPos[15];
char GPS_LonPos[15];
char GPS_LatDir;
char GPS_LonDir;
char GPS_Time[10];
char GPS_Alt[10];
char GPS_Fix;
char GPS_Date[10];



char compareChecksum(const char *gps_string){
	int cur_index=0, i=0;
	char string[GPS_BUFFER_SIZE];
	char checksum_string[2];
	char checksum=0, checksum_compare_value=0;
		
	//Get the message from the GPS string
	for(i=0; gps_string[i]!='*'; i++){
		//If we reach the end of the string without seeing a checksum indicator, return an error
		if(gps_string[i]=='\n' || gps_string[i]=='\r')return 0;
		if(gps_string[i]!='$'){
			string[cur_index++]=gps_string[i];
		}
	}
	string[cur_index++]='\0';
	
	//Get the checksum value from the gps string
	for(int j=0; j<2; j++){
		//If the value of the checksum character isn't alphanumeric, return an error.
		checksum_string[j]=gps_string[++i]; //Skip the '*'
	}
	
	//Combine the two checksum characters
	checksum=((checksum_string[0]<<4)&0xF0)|(checksum_string[1]&0x0F);
	//Find the checksum of the received string
	GPS_CHECKSUM(string,checksum_compare_value)
		
	//rprintf("\n\rReceived: %x\n\rCalculated: %x\n\n\r", checksum, checksum_compare_value);
	//Compare the calculated checksum to the received checksum; if they don't match we're out!
	if(checksum != checksum_compare_value)return 0;
	return 1;
}



//Usage: parseRMC(final_message);
//Inputs: const char *gps_string - RMC NMEA string
//This functions splits a GGA message into the
//portions and assigns them to components of
//a GPS structure
//This functions splits a RMC message into the
//portions and assigns them to components of
//a GPS structure
void parseRMC(const char *gps_string){
	int i=0;
	
	//Parse the GGA Message.  1st portion dismissed
	while(gps_string[i] != ',')i++;
	i++;
	
	//Second portion is UTC timestamp
	for(int j=0;gps_string[i] != ','; j++){
		GPS_Time[j]=gps_string[i];
		i++;
	}
	i++;
	
	//Third portion is fix
	while(gps_string[i] != ','){
		GPS_Fix=gps_string[i];
		i++;
	}	
	i++;
	
	//Fourth portion is Latitude
	for(int j=0;gps_string[i] != ',';j++){
		GPS_LatPos[j]=gps_string[i];
		i++;
	}
	i++;	
	
	//Fifth portion is Latitude direction
	for(int j=0;gps_string[i] != ','; j++){
		GPS_LatDir=gps_string[i];
		i++;
	}
	i++;
	
	//Sixth portion is Long.
	for(int j=0;gps_string[i] != ','; j++){
		GPS_LonPos[j]=gps_string[i];
		i++;
	}
	i++;			
	
	//Seventh portion is Long direction
	while(gps_string[i] != ','){
		GPS_LonDir=gps_string[i];
		i++;
	}
	i++;
	
	//8th portion dismissed
	while(gps_string[i] != ',')i++;
	i++;				
	//9th portion dismissed
	while(gps_string[i] != ',')i++;
	i++;				
	//10th portion is Date
	for(int j=0;gps_string[i] != ','; j++){
		GPS_Date[j]=gps_string[i];
		i++;
	}
}


//RETURN: 0-Error in message. Either bad checksum or insuffecient data
	   // 1-Valid GPS message
int parseGPS(const char *gps_string){
	//Make sure we have a valid string!
	if(!compareChecksum(gps_string))return 0;
	//If we didn't receive the correct SiRF header, the return an error
	if(gps_string[3] == 'R' && gps_string[4] == 'M' && gps_string[5] == 'C'){
		parseRMC(gps_string);
		return 1;
	}
	return 0;
	
}