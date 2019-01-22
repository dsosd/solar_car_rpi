#ifndef GGGG
#define GGGG

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GPS.h>
#include <MCP3208.h>
#include <U8g2lib.h>
#include <RHReliableDatagram.h>
#include <RH_RF95.h>
#include "bitmap_logos.h"
#include "globals.h"
#include <math.h>

void chase_car_loop();
void solar_car_loop();
void OLED_fw_update_prompt();
void OLED_wrong_fw_warning();
void OLED_draw_full_data_page(int connectionStatus, int gpsFix);
double GPS_degMin_to_decDeg (float degMin);
void GPS_print_status_to_serial();
void GPS_print_status_to_logscreen();
void OLED_update();
void set_main_led(uint8_t r, uint8_t g, uint8_t b);
void indicate_critical_error();
void error_buzzer(int repeat);
void success_buzzer();
void fail_buzzer();
void update_lora_packet();
int lora_send_data_to_chase_car(uint8_t* data,uint8_t len);
void update_ADC_readings();
void OLED_draw_ADC_page();
void relay(int command);
void GPS_pps();

#endif // GGGG
