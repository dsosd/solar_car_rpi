#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

#include <Adafruit_GPS.h>
#include <RH_RF95.h>

#include "globals.h"

#define GPSSerial Serial //Hardware serial port

///Struct declarations
typedef struct{
	char gpsTime[11]; //8-10 chars
	char gpsDate[11]; //10 chars
	char gpsLongitude[9]; //8 chars
	char gpsLattitude[9]; //8 chars
	float gpsSpeed;
	int gpsSatellites;
	float adc0;
	float adc1;
	float current;
} packetStruct;

typedef struct{
	uint32_t rawReading; //raw numerical reading from the ADC
	uint32_t rawVoltage; //actual voltage reading after potential divider
	float voltage; //voltage reading at screw terminal

} adcStruct;

///Global variables
extern packetStruct pack;
extern adcStruct adc[8]; //global ADC object
extern uint8_t datTX[RH_RF95_MAX_MESSAGE_LEN];
extern uint8_t datRX[RH_RF95_MAX_MESSAGE_LEN]; //Don't put this on the stack:
extern uint8_t draw_state; //for graphics lib
extern int connectionState; //global indicator of connection state
extern char text[100];
extern char text2[50];

extern Adafruit_GPS GPS; //Connect to the GPS on the hardware port

///Refactored
void update_date();
void update_time(bool mode_24_hours=true);
void update_latitude();
void update_longitude();

void blare_alarm(uint8_t amount=4, uint8_t time_buffer=80, uint8_t duration=50); //time is in ms

///Refactored globals
extern bool g_24_hour_mode;
extern bool g_alarm_active;
constexpr float g_low_voltage_bound=10.0f;

#endif // MAIN_H
