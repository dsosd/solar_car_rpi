#include "pi-topHELIOS.h"

#include <Arduino.h>

#include "car_define.h"

//******* DEVICE ADDRESS & BOARD TYPE *******
//It's essential that the device's unique ID is assigned here
//This number is used to define a unique 100ms window in which to broadcast
//So, it is limited to a maximum value of 30
#define PAIRING_ID 9
//TEST USE ONLY - Uncomment if programming old boards (pre V2) //todo: erase this line
//#define OLD_HARDWARE_PINOUT

//******* Miscellaneous Defines *******
#define FIRMWARE_VERSION_MAJOR	3
#define FIRMWARE_VERSION_MINOR	0
#define FIRMWARE_UPDATE_PROMPT //ask user to update to latest firmware
#define SPLASH_SCREEN_DURATION 1000 //length of time, in ms, to display the splash screen logos on boot

//******* LoRa Radio Object *******
#define LORA_TX_INTERVAL 5000//1500 //important variable - how long to wait between sending data packets back to chase car
#define LORA_CONNECTION_TX_FAILCOUNT	2// the number of failed transmits required on the TX side to register a 'broken connection'
#define LORA_CONNECTION_RX_TIMEOUT (LORA_TX_INTERVAL*2.1/*3.5*/) //high-layer timeout before we say connection is lost on the RX end
#define TRANSMIT_WINDOW (100*PAIRING_ID)
#define RF95_CAD_TIMEOUT 1000 //channel activity detection window - wait this long for a blank window in which to transmit
#define RF95_ACK_TIMEOUT 1000 //how long to wait for an ACK reply after sending a message
RH_RF95 LoRa(LORA_CS_PIN, LORA_IRQ_PIN); //Create the Radio object
#define RF95_FREQ 915.0 //Define Frequency for radio
//The ID of the chase car is automatically the ID of the solar car + 100, to maintain unique IDs
//DO NOT EDIT THESE:
#define CLIENT_PAIRING_ID PAIRING_ID
#define SERVER_PAIRING_ID (PAIRING_ID+100)
// Class to manage reliable message delivery and receipt, using the LoRa driver declared above
#ifdef SOLAR_CAR
	RHReliableDatagram LoRaManager(LoRa, CLIENT_PAIRING_ID);
#else
	RHReliableDatagram LoRaManager(LoRa, SERVER_PAIRING_ID);
#endif


//******* OLED Display Object *******
#define BROADCAST_PAGE_UPDATE_INTERVAL 250 //how frequently page is update - may affect GPS parsing
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_USE_I2C //uncomment to use I2C, otherwise SPI is used
U8G2LOG u8g2log;
#define U8LOG_WIDTH 32
#define U8LOG_HEIGHT 10
uint8_t u8log_buffer[U8LOG_WIDTH*U8LOG_HEIGHT];// assume 4x6 font
// U8g2 Constructor List (Frame Buffer)
// The complete list is available here: https://github.com/olikraus/u8g2/wiki/u8g2setupcpp
#ifdef OLED_USE_I2C
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, /* reset=*/ OLED_RST_PIN);
#else
U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R2, /* cs=*/ OLED_CS_PIN, /* dc=*/ OLED_DC_PIN, /* reset=*/ OLED_RST_PIN);
#endif
//U8G2_SH1106_128X64_VCOMH0_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);    // same as the NONAME variant, but maximizes setContrast() range
//U8G2_SH1106_128X64_WINSTAR_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);   // same as the NONAME variant, but uses updated SH1106 init sequence

//******* Analog-to-Digital Converter Object *******
MCP3208 ExtADC(ADC_CS_PIN);//create the ADC device
#define PrimaryCorrect 0.0653 //Define Scaling factors for sensors
#define Shunt .0005 //Define Scaling factors for sensors

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//In the Arduino environment, the setup function is called once,
//at the start of the program, then loop() is called and runs forever
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void setup_dummy(){
	//******* Basic Pin Initialisation *******
	//Do this first so these pins aren't floating for any time,
	//since floating pins can cause all sorts of weirdness
	//Pins are input by default, but are declared explicitly here for clarity
	//RGB Led
	pinMode(RGB_LED_RED_PIN,OUTPUT);  digitalWrite(RGB_LED_RED_PIN,LOW);
	pinMode(RGB_LED_GRN_PIN,OUTPUT);  digitalWrite(RGB_LED_GRN_PIN,LOW);
	pinMode(RGB_LED_BLU_PIN,OUTPUT);  digitalWrite(RGB_LED_BLU_PIN,LOW);
	//User Interaction
	pinMode(BUZZER_PIN,OUTPUT);
	digitalWrite(RELAY_PIN, LOW);
	relay(HIGH); ///enable low voltage alarm on boot
	pinMode(BUTT0_PIN,INPUT);
	pinMode(BUTT1_PIN,INPUT);
	//Spare GPIO
	pinMode(GPIO0_PIN,INPUT);
	pinMode(GPIO1_PIN,INPUT);
	pinMode(GPIO2_PIN,INPUT);
	pinMode(GPIO3_PIN,INPUT);
	//Misc
	pinMode(RELAY_PIN,OUTPUT);        digitalWrite(RELAY_PIN,LOW);
	pinMode(UNUSED_1_PIN,INPUT_PULLUP); //don't leave MCU pins floating
	//GPS Pin Definitions
	#ifndef OLD_HARDWARE_PINOUT
		pinMode(GPS_1PPS_PIN,INPUT_PULLUP);
	#endif
	//OLED Display Pin Definitions
	pinMode(OLED_DC_PIN,OUTPUT);      digitalWrite(OLED_DC_PIN,LOW);
	pinMode(OLED_RST_PIN,OUTPUT);     digitalWrite(OLED_RST_PIN,LOW); //Pull low to reset
	pinMode(OLED_CS_PIN,OUTPUT);      digitalWrite(OLED_CS_PIN,HIGH); //Pull low to select
	//ADC Pin Definitions
	pinMode(ADC_CS_PIN,OUTPUT);       digitalWrite(ADC_CS_PIN,HIGH); //Pull low to select
	//LORA Radio Pin Definitions
	pinMode(LORA_CS_PIN,OUTPUT);      digitalWrite(LORA_CS_PIN,HIGH); //Pull low to select
	pinMode(LORA_RST_PIN,OUTPUT);     digitalWrite(LORA_RST_PIN,LOW); //Pull low to reset
	pinMode(LORA_IRQ_PIN,INPUT);

	//******* Start Virtual USB Serial Port *******
	SerialUSB.begin(115200);
	//while(!SerialUSB); //Wait for the serial to begin

	//******* Start HW Serial Port to RasPi *******
	#ifdef CHASE_CAR
		Serial.begin(57600);
	#endif

	//******* Begin Analog-to-Digital Converter *******
	ExtADC.begin(); //attempt to talk to ADC on all boards

	//******* Boot OLED display *******
	//u8g2.begin();
	u8g2.begin(/*Select=*/ BUTT0_PIN, /*Right/Next=*/ U8X8_PIN_NONE, /*Left/Prev=*/ U8X8_PIN_NONE, /*Up=*/ U8X8_PIN_NONE, /*Down=*/ BUTT1_PIN, /*Home/Cancel=*/ U8X8_PIN_NONE);
	u8g2.setFont(u8g2_font_6x12_tr);
	u8g2.clearBuffer();

	//******* Check what type of board this is *******
	//If we read blanks from ADC, there is no ADC and this must be a chase car board
	if((ExtADC.analogRead(0)==0x0FFF)&&(ExtADC.analogRead(3)==0x0FFF)&&(ExtADC.analogRead(7)==0x0FFF)){
		//No GPS module fitted, must be chase car board
		#ifdef SOLAR_CAR
			//Warn user that firmware is incorrect for board type
			OLED_wrong_fw_warning();
			while(1);
		#endif
	} else {
		//GPS module fitted, must be solar car board
		#ifdef CHASE_CAR
			//Warn user that firmware is incorrect for board type
			OLED_wrong_fw_warning();
			while(1);
		#endif
	}

	//******* User Update Prompt *******
	#ifdef FIRMWARE_UPDATE_PROMPT
		OLED_fw_update_prompt();
	#endif

	//******* Display splash screens *******
	u8g2.setDrawColor(1); // White
	u8g2.drawBitmap(0,0,16,64,pitop_Logo_Horiz);
	OLED_update();
	delay(SPLASH_SCREEN_DURATION);
	u8g2.setDrawColor(0);
	u8g2.drawBitmap(0,0,16,64,SCC_Logo_Horiz);
	OLED_update();
	delay(SPLASH_SCREEN_DURATION);

	//******* Begin Analog-to-Digital Converter *******
	#ifdef SOLAR_CAR
		ExtADC.begin();
	#endif

	//******* Boot GPS Module *******
	#ifdef SOLAR_CAR
		SerialUSB.println("Starting GPS Module...");
		// connect at 115200 so we can read the GPS fast enough and echo without dropping chars
		// also spit it out
		// 9600 NMEA is the default baud rate for MTK GPS's- some use 4800
		GPS.begin(9600);
		GPSSerial.begin(9600);
		GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
		// uncomment this line to turn on only the "minimum recommended" data
		//GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
		// For parsing data, we don't suggest using anything but either RMC only or RMC+GGA since
		// the parser doesn't care about other sentences at this time
		// Set the update rate
		GPS.sendCommand(PMTK_SET_NMEA_UPDATE_200_MILLIHERTZ); //update rate
		//GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);   // 1 Hz update rate
		// For the parsing code to work nicely and have time to sort thru the data, and
		// print it out we don't suggest using anything higher than 1 Hz
		// Request updates on antenna status, comment out to keep quiet
		//GPS.sendCommand(PGCMD_ANTENNA);
		//delay(1000);
		// Ask for firmware version
		//GPSSerial.println(PMTK_Q_RELEASE);
		//Attach interrupt to the GPS pulse-per-second line
		attachInterrupt(digitalPinToInterrupt(GPS_1PPS_PIN), GPS_pps, CHANGE);
	#endif

	//******* Boot Radio module *******
	SerialUSB.println("Starting LoRa Radio Module...");
	//Reset radio module
	digitalWrite(LORA_RST_PIN, LOW);  delay(10);
	digitalWrite(LORA_RST_PIN, HIGH); delay(10);
	if (!LoRaManager.init()) {
		SerialUSB.print("ERROR: ");
		SerialUSB.println("LoRa Radio Failed to Initialize");
		indicate_critical_error();
		u8g2.userInterfaceMessage("Failed to start","LoRa radio",""," ok ");
		//while(1); //hang forever
	} else {
		SerialUSB.println("LoRa Radio Initialized OKAY!");
		// Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
		// Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on
		if (!LoRa.setFrequency(RF95_FREQ)) {
			SerialUSB.println("setFrequency failed");
			indicate_critical_error();
			u8g2.userInterfaceMessage("Failed to set","LoRa frequency",""," ok ");
			//while (1); //hang forever
		} else {
			SerialUSB.print("Freq set to: "); SerialUSB.print(RF95_FREQ); SerialUSB.println(" MHz");
			// The default transmitter power is 13dBm, using PA_BOOST.
			// If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
			// you can set transmitter powers from 5 to 23 dBm:
			LoRa.setTxPower(23, false);
			// You can optionally require this module to wait until Channel Activity
			// Detection shows no activity on the channel before transmitting by setting
			// the CAD timeout to non-zero:
			LoRa.setCADTimeout(RF95_CAD_TIMEOUT); //For the crowded RF environment of the SOLAR CAR CHALLENGE, this is essential
		}
	}

}//--------------------------------------------------------------------



const char *string_list =
	"Broadcasting Mode\n"//1
	"ADC Readings\n"//2
	"LoRa Test\n"//3
	"GPS Status\n"//4
	"Toggle Relay\n"//5
	"View SCC Logo\n"//6
	"View pi-top Logo\n"
	"Toggle 12/24 hour";
//"---------------------\n"
//ESSENTIAL THAT THESE TWO LISTS ARE IN THE SAME ORDER
//SOLAR CAR MENU
enum{
	HOME_PAGE,
	BROADCASTING_PAGE,
	ADC_PAGE,
	LORA_PAGE,
	GPS_PAGE,
	RELAY_PAGE,
	SCC_LOGO_PAGE,
	PT_LOGO_PAGE,
	MODE_24_HOURS
};

void loop_dummy(){
	#ifdef SOLAR_CAR
		solar_car_loop();
	#endif // SOLAR_CAR
	#ifdef CHASE_CAR
		chase_car_loop();
	#endif // CHASE_CAR
}

//Chase car states
enum{
	STARTUP,
	NO_CONNECTION_PAGE,
	CONNECTION_LOST_PAGE,
	WAITING_FOR_CONNECTION,
	HAVE_CONNECTION,
	RX_DATA_PAGE
};

volatile unsigned long ppsTimestamp=0;

			//u8g2_font_9x15_tf <-save this font because I like it

uint8_t data[] = "ACK";

//Runs on repeat in chase car
void chase_car_loop(void) {
	static int state=0;
	static unsigned long prevMillis = 0;
	static unsigned long prevMillisBlinky = 0;
	static unsigned long interval = 0;
	static unsigned long lastValidPacket = 0;
	static int newPacketWaiting = 0;
	switch(state){
		case STARTUP:
			connectionState = DISCONNECTED;
			state = NO_CONNECTION_PAGE;
			break;
		case CONNECTION_LOST_PAGE:
			u8g2.clearBuffer();
			u8g2.setDrawColor(1); // White
			u8g2.drawBox(0,21,OLED_WIDTH,14);
			u8g2.setFont(u8g2_font_7x14B_tf);
			u8g2.setDrawColor(0); // Black
			u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth("CONNECTION LOST")/2),33);
			u8g2.print("CONNECTION LOST");
			OLED_update();
			set_main_led(255, 0, 0);
			fail_buzzer();
			delay(2000);
			set_main_led(0, 0, 0);
			state = NO_CONNECTION_PAGE;
			break;
		case NO_CONNECTION_PAGE:
			u8g2.clearBuffer();
			u8g2.setDrawColor(1); // White
			u8g2.drawBox(0,0,OLED_WIDTH,12);
			u8g2.setFont(u8g2_font_crox2hb_tf);
			u8g2.setDrawColor(0); // Black
			u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth("DISCONNECTED")/2),11);
			u8g2.print("DISCONNECTED");
			u8g2.setDrawColor(1); // White
			u8g2.setFont(u8g2_font_crox2h_tf);
			u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth("Searching for")/2),25);
			u8g2.print("Searching for");
			sprintf((char*)text,"Solar Car %d",PAIRING_ID);
			u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2),38);
			u8g2.print(text);
			OLED_update();
			state = WAITING_FOR_CONNECTION;
			break;
		case WAITING_FOR_CONNECTION:
			//state = RX_DATA_PAGE;
			if(newPacketWaiting){
				//Connection established!
				success_buzzer();
				connectionState = CONNECTED;
				state=HAVE_CONNECTION;
			}
			break;
		case HAVE_CONNECTION:
			if(millis()-lastValidPacket>LORA_CONNECTION_RX_TIMEOUT){
				state=CONNECTION_LOST_PAGE;
				connectionState = DISCONNECTED;
				break;
			}
			if(newPacketWaiting){
				//If there's new data waiting:
				//First, forward the packet to the pi-top
				Serial.println((char*)datRX);
				//Then, redraw the RX data page with it
				state = RX_DATA_PAGE;
			}
			break;
		case RX_DATA_PAGE:
			u8g2.clearBuffer();
			u8g2.setDrawColor(1); // White
			u8g2.drawBox(0,0,OLED_WIDTH,12);
			u8g2.setFont(u8g2_font_crox2hb_tf);
			u8g2.setDrawColor(0); // Black
			sprintf((char*)text,"Connected to %d",PAIRING_ID);
			u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2),11);
			u8g2.print(text);
			OLED_update();
			newPacketWaiting=0;//data processed, clear the new packet flag
			//todo - forward data to pi-top
			state = HAVE_CONNECTION;
			break;
	}
	//LoRa Radio Receive routine, always runs
	if (LoRaManager.available()){
		//Clear buffer
		memset(datRX,0,RH_RF95_MAX_MESSAGE_LEN);//clear the buffer
		// Wait for a message addressed to us from the client
		uint8_t len = sizeof(datRX); //set to available space in buffer
		uint8_t from;
		if (LoRaManager.recvfromAck(datRX, &len, &from)){ //here len is set to the number of octets copied
			//Flash LED on RX
			//set_main_led(0, 255, 0);
			interval=ONTIME;
			prevMillisBlinky=millis();
			//print info
			SerialUSB.print("ID ");
			SerialUSB.print(from, DEC);
			SerialUSB.print(" len ");
			SerialUSB.print(len, DEC);
			SerialUSB.print(": ");
			SerialUSB.println((char*)datRX);
			//Mark the connection as live
			lastValidPacket=millis();
			//Mark new data waiting
			newPacketWaiting=1;
			// Send a reply back to the originator client
			/*
			if (!LoRaManager.sendtoWait(data, sizeof(data), from)){
				SerialUSB.println("ACK Failed");
			}
			*/
		}
	}
	//Status LED routine, always runs
	if(millis()-prevMillisBlinky>interval){
		prevMillisBlinky=millis();
		if(interval==ONTIME){
			interval=OFFTIME;
			set_main_led(0, 0, 0);
		}else{
			interval=ONTIME;
			switch(state){
				case WAITING_FOR_CONNECTION:
					//set_main_led(255, 0, 0);
					break;
				//case HAVE_CONNECTION:
				//	set_main_led(LED_GRN);
				//	break;
			}
		}
	}
}

volatile int sentInThisWindow=0;

//Runs on repeat in solar car
void solar_car_loop(){
	static uint8_t current_selection = 0;
	static int state=BROADCASTING_PAGE; //DEFAULT PAGE
	static unsigned long prevMillis = 0;
	static unsigned long prevBroadcastTimestamp=0;
	static unsigned long prevDisplayUpdate = 0;
	static unsigned long gpsMsgStartTime = 0;
	static int enteringLoraPage=1;
	static int enteringHomePage=1;
	static int enteringGpsPage=1;
	static int transmitFailStrikes=DISCONNECTED;
	int broadcastFlag=0;
	char gpsChar=0;

	auto trigger_alarm=[&](){
		if (g_alarm_active && adc[0].voltage<g_low_voltage_bound){ //required low voltage alarm. can be disable by toggling relay
			blare_alarm();
		}
	};

	switch (state){
	case MODE_24_HOURS:
		g_24_hour_mode^=1ul;
		state=HOME_PAGE;
		break;
	case HOME_PAGE:
		trigger_alarm();
		if(enteringHomePage){
			//u8g2.begin(/*Select=*/ BUTT0_PIN, /*Right/Next=*/ U8X8_PIN_NONE, /*Left/Prev=*/ U8X8_PIN_NONE, /*Up=*/ U8X8_PIN_NONE, /*Down=*/ BUTT1_PIN, /*Home/Cancel=*/ U8X8_PIN_NONE);
			u8g2.setFont(u8g2_font_6x12_tr);
			u8g2.clearBuffer();
			u8g2.setDrawColor(1); // White
			enteringHomePage=0;
		}
		//A blocking menu routine to select mode
		current_selection = 0;
		sprintf((char*)text,"Solar Car ID: %02d",PAIRING_ID);
		current_selection = u8g2.userInterfaceSelectionList(text,current_selection,string_list);
		if(current_selection!=0){
			state = current_selection;
			SerialUSB.print(current_selection);
			SerialUSB.println(" selected");
			enteringHomePage=1;//reset 'first time' flag
			GPS.fix=0;//mark GPS as dead to force immediate screen update
		}
		//if(current_selection==0){
		//	u8g2.userInterfaceMessage("Nothing selected.","",""," ok ");
		break;
	case BROADCASTING_PAGE:
		//GPS parsing
		gpsChar = GPS.read();
		//if (gpsChar){SerialUSB.print(gpsChar);}
		if (GPS.newNMEAreceived()){
			if (!GPS.parse(GPS.lastNMEA())){  // this also sets the newNMEAreceived() flag to false
				break;// we can fail to parse a sentence in which case we should just wait for another
			}
		}
		//If we have a GPS lock, broadcasting windows are determined by the PPS timestamp
		//If there is no GPS lock, broadcasting windows are determined by LORA_TX_INTERVAL
		if (GPS.fix==1){
			//Update at a given time after PPS, based on device number
			if (((millis()-ppsTimestamp)>TRANSMIT_WINDOW)&&(sentInThisWindow==0)){
				broadcastFlag=1;
				sentInThisWindow=1;
			}
		}
		else{
			//No GPS fix - broadcast at set intervals
			if (millis()-prevBroadcastTimestamp>LORA_TX_INTERVAL){
				prevBroadcastTimestamp=millis();
				broadcastFlag=1;
			}
		}
		if (broadcastFlag){
			trigger_alarm();

			//Send, update
			update_ADC_readings();
			update_lora_packet();
			SerialUSB.println((char*)datTX);//print data being sent
			if (GPS.fix==0){
				//set_main_led(LED_ORA_DIM); //TODO refactor, but not with magic numbers
			}
			else if (connectionState==CONNECTED){
				//set_main_led(0, 255, 0);
			}
			else{
				//set_main_led(255, 0, 0);
			}
			//set_main_led((connectionState==CONNECTED)?LED_GRN:LED_RED);
			if (lora_send_data_to_chase_car(datTX,strlen((const char*)datTX))==SUCCESS){
				//Transmitted successfully
				connectionState = CONNECTED; //set to 0
			}
			else{
				//Failed to transmit
				connectionState++; //failed transmits, non-zero is failed
			}
			set_main_led(0, 0, 0);
			GPS_print_status_to_serial();//todo - comment out
			OLED_draw_full_data_page((connectionState>=LORA_CONNECTION_TX_FAILCOUNT)?DISCONNECTED:CONNECTED,GPS.fix); //draws all data to screen
			broadcastFlag=0;
		}
		//Wait for button press to exit
		if ((digitalRead(BUTT0_PIN)==LOW)||(digitalRead(BUTT1_PIN)==LOW)){
			while((digitalRead(BUTT0_PIN)!=HIGH)||(digitalRead(BUTT1_PIN)!=HIGH)){}
			state=HOME_PAGE;
		}
		break;
	case ADC_PAGE:
		trigger_alarm();
		//ADC
		update_ADC_readings();
		OLED_draw_ADC_page();
		//Wait for button press to exit
		if ((digitalRead(BUTT0_PIN)==LOW)||(digitalRead(BUTT1_PIN)==LOW)){
			while ((digitalRead(BUTT0_PIN)!=HIGH)||(digitalRead(BUTT1_PIN)!=HIGH)){
				trigger_alarm();
			}
			state=HOME_PAGE;
		}
		break;
	case LORA_PAGE:
		trigger_alarm();
		if (enteringLoraPage){
			//Run when entering the LoRa page
			u8g2.setFont(u8g2_font_tom_thumb_4x6_mf);  // set the font for the terminal window
			u8g2log.begin(u8g2, U8LOG_WIDTH, U8LOG_HEIGHT, u8log_buffer);
			u8g2log.setLineHeightOffset(0); // set extra space between lines in pixel, this can be negative
			u8g2log.setRedrawMode(0);   // 0: Update screen with newline, 1: Update screen for every char
			u8g2log.println("LORA DEBUGGING LOG");
			u8g2log.println("To exit page, hold any button");
			u8g2log.println("then release");
			delay(1000);
			//u8g2log.println("ATTEMPTING TO CONNECT TO");
			//u8g2log.println("PAIRED PARTNER OVER LORA");
			enteringLoraPage=0;
		}
		//Transmit data package to paired unit, at specified intervals
		if (millis()-prevMillis>LORA_TX_INTERVAL){
			prevMillis=millis();
			//Populate data buffer with test data
			sprintf((char*)datTX,"23:59:59,%d.00,%d.00,%d,33.0382N, 97.2816W,%d",random(7,17),random(38,58),random(-50,50),random(0,60));
			//update_ADC_readings();
			//update_lora_packet();
			SerialUSB.println((char*)datTX);
			switch (lora_send_data_to_chase_car(datTX,strlen((const char*)datTX))){
			case SUCCESS:
				//u8g2log.print(millis());
				u8g2log.print("SUCCESS: Connection OKAY!\n");
				connectionState=CONNECTED;
				break;
			case LORA_ACK_FAILURE:
				u8g2log.print("FAIL: Connected but no reply\n");
				connectionState++;//failed - increment
				break;
			case LORA_CAD_FAILURE:
				u8g2log.print("FAIL: Couldn't connect to target\n");
				connectionState++;//failed - increment
				break;
			}
		}
		//Wait for button press to exit
		if ((digitalRead(BUTT0_PIN)==LOW)||(digitalRead(BUTT1_PIN)==LOW)){
			while ((digitalRead(BUTT0_PIN)!=HIGH)||(digitalRead(BUTT1_PIN)!=HIGH)){
				trigger_alarm();
			}
			state=0;
			//Reset entering flag
			enteringLoraPage=1;
		}
		break;
	case GPS_PAGE:
		trigger_alarm();
		if (enteringGpsPage){
			u8g2.setFont(u8g2_font_tom_thumb_4x6_mf);  // set the font for the terminal window
			u8g2log.begin(u8g2, U8LOG_WIDTH, U8LOG_HEIGHT, u8log_buffer);
			u8g2log.setLineHeightOffset(0); // set extra space between lines in pixel, this can be negative
			u8g2log.setRedrawMode(0);   // 0: Update screen with newline, 1: Update screen for every char
			u8g2log.println("GPS DEBUGGING LOG");
			u8g2log.println("To exit page, hold any button");
			u8g2log.println("then release");
			delay(1000);
			enteringGpsPage=0;
		}
		gpsChar = GPS.read();
		if (gpsChar){
			SerialUSB.print(gpsChar);
		}
		if (GPS.newNMEAreceived()){
			if (!GPS.parse(GPS.lastNMEA())){  // this also sets the newNMEAreceived() flag to false
				break;
			}  // we can fail to parse a sentence in which case we should just wait for another
		}
		// if millis() or timer wraps around, we'll just reset it
		if (prevMillis > millis()){
			prevMillis = millis();
		}
		// approximately every N seconds or so, print out the current stats
		if (millis() - prevMillis > 5000){
			prevMillis = millis(); // reset the timer
			GPS_print_status_to_serial();
			GPS_print_status_to_logscreen();
		}
		//Wait for button press to exit
		if ((digitalRead(BUTT0_PIN)==LOW)||(digitalRead(BUTT1_PIN)==LOW)){
			while ((digitalRead(BUTT0_PIN)!=HIGH)||(digitalRead(BUTT1_PIN)!=HIGH)){
				trigger_alarm();
			}
			state=0;
			enteringGpsPage=1;
		}
		break;
	case RELAY_PAGE:
		/*current_selection = u8g2.userInterfaceMessage("Selection:","Toggle Relay",""," ok \n cancel ");
		if(current_selection==1){
			SerialUSB.println("Toggling relay");
			relay(TOGGLE);
		}*/
		relay(TOGGLE);
		state = HOME_PAGE;
		break;
	case SCC_LOGO_PAGE:
		//SCC Logo
		u8g2.clearBuffer();
		u8g2.setDrawColor(0);
		u8g2.drawBitmap(0,0,16,64,SCC_Logo_Horiz);
		OLED_update();
		state=HOME_PAGE;
		while ((digitalRead(BUTT0_PIN)!=LOW)&&(digitalRead(BUTT1_PIN)!=LOW)){
			trigger_alarm();
		}
		while ((digitalRead(BUTT0_PIN)!=HIGH)||(digitalRead(BUTT1_PIN)!=HIGH)){
			trigger_alarm();
		}
		break;
	case PT_LOGO_PAGE:
		//pi-top Logo
		u8g2.clearBuffer();
		u8g2.setDrawColor(1);
		u8g2.drawBitmap(0,0,16,64,pitop_Logo_Horiz);
		OLED_update();
		state=HOME_PAGE;
		while ((digitalRead(BUTT0_PIN)!=LOW)&&(digitalRead(BUTT1_PIN)!=LOW)){
			trigger_alarm();
		}
		while ((digitalRead(BUTT0_PIN)!=HIGH)||(digitalRead(BUTT1_PIN)!=HIGH)){
			trigger_alarm();
		}
		break;
	}

	// if millis() or prevMillis wraps around, we'll just reset it
	if (prevMillis>millis()){
		prevMillis=millis();
	}
}



void OLED_fw_update_prompt(){//TODO MARK: CLEAN BUT NOT REFACTORED
	unsigned long millisMarker=millis();
	//Wipe the page clean
	u8g2.clearBuffer();
	//Draw the title bar
	u8g2.setDrawColor(1); //White
	u8g2.drawBox(0, 0, OLED_WIDTH,15);
	u8g2.setFont(u8g2_font_crox2hb_tf);
	//u8g2.setFont(u8g2_font_profont10_tf); //5 pixel height
	u8g2.setDrawColor(0); //Black
	sprintf((char*)text, "Update Firmware", FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR);
	u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2),11);
	u8g2.print(text);
	//Body text
	u8g2.setFont(u8g2_font_tom_thumb_4x6_mf);
	u8g2.setDrawColor(1); //White
	//Print current version
	sprintf((char*)text, "Current Firmware: v%d.%d", FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR);
	u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2), 23);
	u8g2.print(text);
	//Encourage user to update to latest
	sprintf((char*)text, "Please flash the");
	u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2), 35);
	u8g2.print(text);
	sprintf((char*)text, "latest firmware from");
	u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2), 35+u8g2.getMaxCharHeight());
	u8g2.print(text);
	sprintf((char*)text, "your local computer.");
	u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2), 35+2*u8g2.getMaxCharHeight());
	u8g2.print(text);
	sprintf((char*)text, "-Andy Huang");
	u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2), 35+3*u8g2.getMaxCharHeight());
	u8g2.print(text);
	//Update screen
	OLED_update();
	//Wait for button press to exit
	while (digitalRead(BUTT0_PIN)!=LOW && digitalRead(BUTT1_PIN)!=LOW){
		if (millis()-millisMarker>10*1000){
			return;
		} //exit after timeout
	}
	while (digitalRead(BUTT0_PIN)!=HIGH || digitalRead(BUTT1_PIN)!=HIGH){
		//busy wait
	}
}

void OLED_wrong_fw_warning(void){
	//Wipe the page clean
	u8g2.clearBuffer();
	//Draw the title bar
	u8g2.setDrawColor(1); // White
	u8g2.drawBox(0,0,OLED_WIDTH,15);
	u8g2.setFont(u8g2_font_crox2hb_tf);
	//u8g2.setFont(u8g2_font_profont10_tf); //5 pixel height
	u8g2.setDrawColor(0); // Black
	sprintf((char*)text,"Wrong Firmware!");
	u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2),11);
	u8g2.print(text);
	//Encourage user to update to latest
	//u8g2.setFont(u8g2_font_tom_thumb_4x6_mf);
	u8g2.setFont(u8g2_font_profont10_tf);
	u8g2.setDrawColor(1); // White
	#ifdef SOLAR_CAR
		sprintf((char*)text,"This is a chase car board");
		u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2),30);
		u8g2.print(text);
		sprintf((char*)text,"");
		u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2),30+u8g2.getMaxCharHeight());
		u8g2.print(text);
		sprintf((char*)text,"Please flash the solar");
		u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2),45);
		u8g2.print(text);
		sprintf((char*)text,"car board firmware");
		u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2),45+u8g2.getMaxCharHeight());
		u8g2.print(text);
	#endif // SOLAR_CAR
	#ifdef CHASE_CAR
		u8g2.setFont(u8g2_font_tom_thumb_4x6_mf);
		u8g2.setDrawColor(1); // White
		sprintf((char*)text,"This is a solar car board");
		u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2),30);
		u8g2.print(text);
		sprintf((char*)text,"");
		u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2),30+u8g2.getMaxCharHeight());
		u8g2.print(text);
		sprintf((char*)text,"Please flash the chase");
		u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2),45);
		u8g2.print(text);
		sprintf((char*)text,"car board firmware");
		u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2),45+u8g2.getMaxCharHeight());
		u8g2.print(text);
	#endif // CHASE_CAR
	//Update screen
	OLED_update();
}

void OLED_draw_full_data_page(int connectionStatus, int gpsFix){
	int cursorXpos=0;
	int cursorYpos=0;

	//Wipe the page clean
	u8g2.clearBuffer();

	//Draw the title bar
	u8g2.setDrawColor(1); // White
	u8g2.drawBox(0,0,OLED_WIDTH,12);
	u8g2.setFont(u8g2_font_crox2hb_tf);
	//u8g2.setFont(u8g2_font_profont10_tf); //5 pixel height
	u8g2.setDrawColor(0); // Black
	if(connectionStatus==CONNECTED){sprintf((char*)text,"Connected to %d ",PAIRING_ID);}
	else{sprintf((char*)text,"DISCONNECTED");}
	u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2),11);
	u8g2.print(text);

	//GPS fix
	if(gpsFix==0){
		//No GPS fix
		sprintf((char*)text,"Waiting for GPS fix");
	} else {
		sprintf((char*)text,"GPS fix acquired");
	}
	u8g2.setDrawColor(1); // White
	u8g2.setFont(u8g2_font_6x12_tr);
	u8g2.setCursor((OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2),23);
	u8g2.print(text);

	//GPS Info
	cursorXpos=BP_GPS_DATA_XPOS;
	cursorYpos=BP_GPS_DATA_YPOS;
	u8g2.setFont(u8g2_font_u8glib_4_tf); //4 pixel height
	//u8g2.setFont(u8g2_font_profont10_tf); //5 pixel height
	//Time & date
	update_date();
	update_time(g_24_hour_mode);
	if (g_24_hour_mode){
		sprintf((char*)text, "TIMESTAMP: %sT%sZ", pack.gpsDate, pack.gpsTime);
	}
	else{ //remove UTC specifier to fit entire string on page for 12 hour mode
		sprintf((char*)text, "TIMESTAMP: %sT%s", pack.gpsDate, pack.gpsTime);
	}
	cursorXpos = (OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2); //Justify to center
	u8g2.setCursor(cursorXpos,cursorYpos);
	u8g2.print(text);
	//GPS Coordinates
	update_latitude();
	update_longitude();
	sprintf((char*)text, "LOCATION: %s, %s", pack.gpsLattitude, pack.gpsLongitude);
	cursorXpos = (OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2); //Justify to center
	cursorYpos+=u8g2.getMaxCharHeight();
	u8g2.setCursor(cursorXpos,cursorYpos);
	u8g2.print(text);
	//Speed & Signal Strength
	sprintf((char*)text,"SATELLITES: %2d SPEED: %0.1f mph", pack.gpsSatellites, pack.gpsSpeed);
	cursorXpos = (OLED_WIDTH/2)-(u8g2.getStrWidth((const char*)text)/2); //Justify to center
	cursorYpos+=u8g2.getMaxCharHeight();
	u8g2.setCursor(cursorXpos,cursorYpos);
	u8g2.print(text);

	//Draw ADC voltages
	u8g2.setDrawColor(1); // White
	u8g2.setFont(u8g2_font_u8glib_4_tf); //4 pixel height
	u8g2.setCursor(BP_ADC_XPOS,BP_ADC_YPOS-7);
	sprintf((char*)text," ADC0    ADC1   ADC2   ADC3 ");
	u8g2.print(text);
	u8g2.setFont(u8g2_font_profont10_tf); //5 pixel height
	u8g2.setCursor(BP_ADC_XPOS,BP_ADC_YPOS);
	sprintf((char*)text,"%04.1f|%04.1f|%04.1f|%04.1f",adc[0].voltage,adc[1].voltage,adc[2].voltage,adc[3].voltage);
	u8g2.print(text);

	//Draw to screen
	OLED_update();
}



// converts lat/long from Adafruit
// degree-minute format to decimal-degrees
double GPS_degMin_to_decDeg (float degMin) {
	double min = 0.0;
	double decDeg = 0.0;
	//get the minutes, fmod() requires double
	min = fmod((double)degMin, 100.0);
	//rebuild coordinates in decimal degrees
	degMin = (int) ( degMin / 100 );
	decDeg = degMin + ( min / 60 );
	return decDeg;
}


void GPS_print_status_to_serial(void){
	update_date();
	update_time();
	SerialUSB.print("\nDate: ");
	SerialUSB.println(pack.gpsDate);
	SerialUSB.print("Time: ");
	SerialUSB.println(pack.gpsTime);

	SerialUSB.print("Fix: "); SerialUSB.print((int)GPS.fix);
	SerialUSB.print(" quality: "); SerialUSB.println((int)GPS.fixquality);
	if (GPS.fix){
		update_latitude();
		update_longitude();
		SerialUSB.print("Location: ");
		SerialUSB.print(pack.gpsLattitude);
		SerialUSB.print(", ");
		SerialUSB.print(pack.gpsLongitude);
		SerialUSB.print("\n");
		SerialUSB.print("Speed (mph): "); SerialUSB.println(convertKnotsToMph(GPS.speed),1);
		SerialUSB.print("Angle: "); SerialUSB.println(GPS.angle);
		SerialUSB.print("Altitude: "); SerialUSB.println(GPS.altitude);
		SerialUSB.print("Satellites: "); SerialUSB.println((int)GPS.satellites);
	}
}

void GPS_print_status_to_logscreen(){
	update_date();
	update_time(g_24_hour_mode);
	u8g2log.print("\nDate: ");
	u8g2log.println(pack.gpsDate);
	u8g2log.print("Time: ");
	u8g2log.println(pack.gpsTime);

	u8g2log.print("Fix: "); u8g2log.print((int)GPS.fix);
	u8g2log.print(" Quality: "); u8g2log.println((int)GPS.fixquality);
	if (GPS.fix){
		update_latitude();
		update_longitude();
		u8g2log.print("Location: ");
		u8g2log.print(pack.gpsLattitude);
		u8g2log.print(", ");
		u8g2log.print(pack.gpsLongitude);
		u8g2log.print("\n");
		u8g2log.print("Speed: "); u8g2log.println(convertKnotsToMph(GPS.speed),1);
		u8g2log.print("Angle: "); u8g2log.println(GPS.angle);
		u8g2log.print("Altitude: "); u8g2log.println(GPS.altitude);
		u8g2log.print("Satellites: "); u8g2log.println((int)GPS.satellites);
	} else {
		u8g2log.println("Waiting for GPS fix...");
	}
}

//Function used to update the OLED screen
void OLED_update(void){
	#ifndef OLED_USE_I2C
	//disable all interrupts if using SPI, to prevent
	// interference with LORA radio interrupts
	noInterrupts();
	#endif
	u8g2.sendBuffer();
	interrupts(); //(re)enable all interrupts
}

//LED writing function
void set_main_led(uint8_t r, uint8_t g, uint8_t b){
	analogWrite(RGB_LED_RED_PIN,r);
	analogWrite(RGB_LED_GRN_PIN,g);
	analogWrite(RGB_LED_BLU_PIN,b);
}

//Indicate error on LED and buzzer
void indicate_critical_error(void){
	set_main_led(255, 0, 0);
	error_buzzer(3);
}

void error_buzzer(int repeat){
	while(repeat){
		tone(BUZZER_PIN,1000,100);delay(150);
		repeat--;
	}
}

void success_buzzer(void){
	tone(BUZZER_PIN,1100,50);delay(75);
	tone(BUZZER_PIN,1100,50);delay(75);
	//tone(BUZZER_PIN,2000,50);delay(100);
// 	int intervals=50;
// 	int duration=100;
// 	int start=950;//lowest 800
// 	int finish=1100;//highest 1100
// 	int level = start;
// 	while(level!=finish){
// 		tone(BUZZER_PIN,level,duration);delay(duration);
// 		level+=intervals;
// 	}
}

void fail_buzzer(void){
	tone(BUZZER_PIN,800,50);delay(75);
	tone(BUZZER_PIN,800,50);delay(75);
	tone(BUZZER_PIN,800,50);delay(75);
	tone(BUZZER_PIN,800,50);delay(75);
// 	int intervals=50;
// 	int duration=100;
// 	int start=1100;//highest 1100
// 	int finish=950;//lowest 800
// 	int level = start;
// 	while(level!=finish){
// 		tone(BUZZER_PIN,level,duration);delay(duration);
// 		level-=intervals;
// 	}
}

//Fills outgoing LoRa with the latest known data
void update_lora_packet(void){
	//Time & Date
	update_date();
	update_time();
	//Location
	update_latitude();
	update_longitude();
	//Speed
	pack.gpsSpeed=convertKnotsToMph(GPS.speed);
	//Satellites
	pack.gpsSatellites=GPS.satellites;
	//ADC readings
	pack.adc0=adc[0].voltage;
	pack.adc1=adc[1].voltage;
	//Jarret's line for calculating current:
	pack.current=((adc[2].voltage/**PrimaryCorrect*/)-(adc[3].voltage/**PrimaryCorrect*/)) * (1/Shunt);
	//Assemble it all together
	memset(datTX, 0, RH_RF95_MAX_MESSAGE_LEN);//clear the buffer
	sprintf((char*)datTX, "%s,%05.2f,%05.2f,%0.4f,%s,%s,%0.1f", pack.gpsTime, pack.adc0, pack.adc1, pack.current, pack.gpsLattitude, pack.gpsLongitude, pack.gpsSpeed);
	/*
	//Create string to send
	String FinalString =	\
		String(GPS.hour)	+	\
		":"	+	\
		String(GPS.minute)	+	\
		":"	+	\
		String(GPS.seconds)	+	\
		", "	+	\
		String((adc[0].voltage),4)	+	\
		", "	+	\
		String((adc[1].voltage),4)	+	\
		", "	+	\
		String((current),4)	+	\
		", "	+	\
		String(GPS_degMin_to_decDeg(GPS.latitude),4)	+	\
		String(GPS.lat)	+	\
		", "	+	\
		String(GPS_degMin_to_decDeg(GPS.longitude),4)	+	\
		String(GPS.lon)	+	\
		", "	+	\
		String(convertKnotsToMph(GPS.speed),1);
	//Convert to character array to be sent, load into the output buffer
	FinalString.toCharArray((char*)datTX, FinalString.length());
	*/
}


//Send LoRa data. Blocking function. Returns SUCCESS if received ACK, FAILED if failed.
int lora_send_data_to_chase_car(uint8_t* data,uint8_t len){
	// Send a message to manager_server
	if (LoRaManager.sendtoWait(data, len, SERVER_PAIRING_ID)){
		SerialUSB.println("SUCCESS: Message sent okay\n");
		return SUCCESS;
		/*
		// Now wait for a reply from the server
		len = sizeof(datRX);
		uint8_t from;
		if (LoRaManager.recvfromAckTimeout(datRX, &len, RF95_ACK_TIMEOUT, &from)){
			SerialUSB.print("got reply from ID:");
			SerialUSB.print(from, DEC);
			SerialUSB.print(" : ");
			SerialUSB.println((char*)datRX);
			return SUCCESS;
		}
		else{
			SerialUSB.println("FAIL: Connected but no reply\n");
			return LORA_ACK_FAILURE;
		}
		*/
	}
	else {
		SerialUSB.println("FAIL: Couldn't connect to target\n");
		return LORA_CAD_FAILURE;
	}
}



void update_ADC_readings(void){
	//Compensate for layout pin mapping here
	noInterrupts(); //disable interrupts to prevent interference from LoRa interrupt routine
	adc[0].rawReading = ExtADC.analogRead(5);
	adc[1].rawReading = ExtADC.analogRead(4);
	adc[2].rawReading = ExtADC.analogRead(3);
	adc[3].rawReading = ExtADC.analogRead(2);
	adc[4].rawReading = ExtADC.analogRead(1);
	adc[5].rawReading = ExtADC.analogRead(0);
	adc[6].rawReading = ExtADC.analogRead(6);
	adc[7].rawReading = ExtADC.analogRead(7);
	interrupts();
	//Calculate voltages
	for(int x=0;x<8;x++){
		adc[x].voltage = float(adc[x].rawReading*16); //actual value after mapping and PD
		adc[x].voltage /= 1000; //Convert from millivolts to volts
	}
	//If debugging
	#ifdef SERIAL_DEBUG_ADC
	for(int x=0;x<8;x++){
		SerialUSB.print(x,DEC);
		SerialUSB.print(": ");
		SerialUSB.print(adc.voltage[x],DEC);
		SerialUSB.print("mV (");
		SerialUSB.print(adc.rawReading[x],DEC);
		SerialUSB.println(")");
		delay(1);
	}
	SerialUSB.println("\r\n\n\n");
	#endif
}




void OLED_draw_ADC_page(void){
	int baseX=0;
	int baseY=0;
	//Set up for writing
	u8g2.clearBuffer();
	u8g2.setDrawColor(1); // White
	u8g2.setFontMode(1);
	u8g2.setFontPosCenter();
	u8g2.setFont(u8g2_font_6x12_tr);
	//Loop through all ADC frames
	for(int f=0;f<8;f++){
		baseX = ADC_FRAME_WIDTH*f;
		if(f>3){
			baseX-=(ADC_FRAME_WIDTH*4);
			baseY=ADC_FRAME_HEIGHT;
		}
		//Draw full frame
		u8g2.setDrawColor(1);// White
		u8g2.drawFrame(baseX,baseY,ADC_FRAME_WIDTH,ADC_FRAME_HEIGHT);
		//Draw title frame
		u8g2.setDrawColor(1);// White
		u8g2.drawBox(baseX,baseY,ADC_FRAME_WIDTH,ADC_TITLE_HEIGHT);
		//Draw title text
		sprintf((char*)text,"ADC%d",f);
		u8g2.setDrawColor(2);// XOR
		u8g2.setCursor(baseX+ADC_TITLE_XPOS, baseY+ADC_TITLE_YPOS);
		u8g2.print(text);
		//Draw ADC reading
		//adc[f].voltage=0;
		sprintf((char*)text,"%4.1f",adc[f].voltage);
		u8g2.setDrawColor(2);// XOR
		u8g2.setCursor(baseX+ADC_READING_XPOS, baseY+ADC_READING_YPOS);
		u8g2.print(text);
	}
	OLED_update();
	u8g2.setFontPosBaseline();
}



void relay(int command){
	static uint8_t state=0x00;
	switch (command){
	case HIGH:
		state=0xFF;
		digitalWrite(RELAY_PIN, HIGH);
		//SerialUSB.println("Relay ON");
		break;
	case LOW:
		state=0x00;
		digitalWrite(RELAY_PIN, LOW);
		//SerialUSB.println("Relay OFF");
		break;
	case TOGGLE:
		state=~state;
		digitalWrite(RELAY_PIN, state==0xFF);
		//SerialUSB.print("Relay TOGGLED (");
		//SerialUSB.print(state?"ON":"OFF");
		//SerialUSB.println(")");
		break;
	}
	g_alarm_active= state==0xFF;
}

void GPS_pps(void){
	ppsTimestamp=millis();
	sentInThisWindow=0;//new window
	#ifndef OLD_HARDWARE_PINOUT
		//analogWrite(RGB_LED_BLU_PIN,digitalRead(GPS_1PPS_PIN)?255:0); //why would you not refactor this pi-top team???
	#endif
	//digitalWrite(BUZZER_PIN,digitalRead(GPIO3_PIN));
}



  /*
  int16_t adc0, adc1, adc2, adc3, current;
  // read data from the GPS in the 'main loop'
   char c = GPS.read();
  // if you want to debug, this is a good time to do it!
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences!
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    //Read the analog to digital converter voltages
    adc0 = ExtADC.readADC_SingleEnded(0);
    adc1 = ExtADC.readADC_SingleEnded(1);
    adc2 = ExtADC.readADC_SingleEnded(2);
    adc3 = ExtADC.readADC_SingleEnded(3);
    current = ((adc3*PrimaryCorrect)-(adc3*PrimaryCorrect))*(1/Shunt);
      if (GPS.fix) {
        display.clearDisplay();
        //display.setCursor(0, 0);
        display.println("Aux  Volt: " + String(adc0*PrimaryCorrect));
        //display.setCursor(0, 1);
        display.println("Main Volt: " + String(adc1*PrimaryCorrect));
        //display.setCursor(0, 2);
        display.println("Current: " + String(round((current)*10)/10));
        //display.setCursor(0, 3);
        display.println("Speed: " + String(round((GPS.speed)*10)/10));
        uint8_t data[120];
        //Create Output sting
        String FinalString= String(GPS.hour)+":"+String(GPS.minute)+":"+String(GPS.seconds)+ ", " + String((adc0*PrimaryCorrect),4)+ ", " + String((adc1*PrimaryCorrect),4) + ", " + String((current),4) + ", "+ String(GPS.latitude,4) + String(GPS.lat) + ", " + String(GPS.longitude,4) + String(GPS.lon) + ", " + String(GPS.speed);
        //convert output sting to bytes (uint8_t) that can be transmitted
        FinalString.toCharArray((char*)data, FinalString.length());
        //transmit the data string
        LoRa.send((uint8_t *)data, sizeof(data));
        GPS.fix =false ;
      }
   if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
      return; // we can fail to parse a sentence in which case we should just wait for another
  }
 */


/*
	//Create Output sting
	String FinalString = String(GPS.hour)+":"+String(GPS.minute)+":"+String(GPS.seconds)+ ", " + String((adc0*PrimaryCorrect),4)+ ", " + String((adc1*PrimaryCorrect),4) + ", " + String((current),4) + ", "+ String(GPS.latitude,4) + String(GPS.lat) + ", " + String(GPS.longitude,4) + String(GPS.lon) + ", " + String(GPS.speed);
	//convert output sting to bytes (uint8_t) that can be transmitted
	FinalString.toCharArray((char*)data, FinalString.length());
	//transmit the data string
	LoRa.send((uint8_t *)data, sizeof(data));
	GPS.fix =false ;
*/
