#include "main.h"

#include "pi-topHELIOS.h"

///Global variables
packetStruct pack;
adcStruct adc[8]; //global ADC object
uint8_t datTX[RH_RF95_MAX_MESSAGE_LEN]="Empty Buffer";
uint8_t datRX[RH_RF95_MAX_MESSAGE_LEN]; //Don't put this on the stack:
uint8_t draw_state=0; //for graphics lib
int connectionState=DISCONNECTED; //global indicator of connection state
char text[100];
char text2[50];

Adafruit_GPS GPS(&GPSSerial); //Connect to the GPS on the hardware port

///Refactored globals
bool g_24_hour_mode=true;
bool g_alarm_active;

void setup(){
	setup_dummy();
}
void loop(){
	loop_dummy();
}

void update_date(){
	char date[11]; //yyyy-mm-dd
	sprintf(date, "20%02d-%02d-%02d", GPS.year, GPS.month, GPS.day);
	date[10]='\0';
	memcpy(pack.gpsDate, date, 11);
}
void update_time(bool mode_24_hours){
	if (mode_24_hours){
		char time[9]; //hh:mm:ss
		sprintf(time, "%02d:%02d:%02d", GPS.hour, GPS.minute, GPS.seconds);
		time[8]='\0';
		memcpy(pack.gpsTime, time, 9);
	}
	else{
		char time[11]; //hh:mm:ss12 12=AM/PM
		uint8_t hour=GPS.hour;
		char time_mode[3]="AM";
		if (GPS.hour>=12){
			if (GPS.hour!=12){
				hour=GPS.hour-12;
			}
			time_mode[0]='P'; //PM
		}
		sprintf(time, "%02d:%02d:%02d%s", hour, GPS.minute, GPS.seconds, time_mode);
		time[10]='\0';
		memcpy(pack.gpsTime, time, 11);
	}
}
void update_latitude(){
	char lat[9]; //?xx.xxxx
	char lat_sign= GPS.lat=='N' ? ' ' : '-';

	sprintf(lat, "%c%07.4f", lat_sign, GPS_degMin_to_decDeg(GPS.latitude));
	lat[8]='\0';
	memcpy(pack.gpsLattitude, lat, 9);
}
void update_longitude(){
	char lon[9]; //?xx.xxxx
	char lon_sign= GPS.lon=='E' ? ' ' : '-';
	sprintf(lon, "%c%07.4f", lon_sign, GPS_degMin_to_decDeg(GPS.longitude));
	lon[8]='\0';
	memcpy(pack.gpsLongitude, lon, 9);
}

void blare_alarm(uint8_t amount, uint8_t time_buffer, uint8_t duration){ //time is in ms
	for (uint8_t i=0; i<amount; ++i){
		tone(BUZZER_PIN, 900, duration);
		delay(time_buffer);
	}
}
