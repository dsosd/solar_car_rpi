#ifndef PI_TOP_HELIOS_H
#define PI_TOP_HELIOS_H

#include <math.h>

#include <SPI.h>
#include <Wire.h>

#include <MCP3208.h>
#include <RHReliableDatagram.h>
#include <U8g2lib.h>

#include "bitmap_logos.h"
#include "globals.h"
#include "main.h"

void setup_dummy();
void loop_dummy();

void chase_car_loop();
void solar_car_loop();

void OLED_fw_update_prompt();
void OLED_wrong_fw_warning();
void OLED_draw_full_data_page(int connectionStatus, int gpsFix);
void OLED_draw_ADC_page();
void OLED_update();

double GPS_degMin_to_decDeg(float degMin);
void GPS_print_status_to_serial();
void GPS_print_status_to_logscreen();
void GPS_pps();

void relay(int command);
void set_main_led(uint8_t r, uint8_t g, uint8_t b);
void indicate_critical_error();

void error_buzzer(int repeat);
void success_buzzer();
void fail_buzzer();

int lora_send_data_to_chase_car(uint8_t* data, uint8_t len);
void update_lora_packet();

void update_ADC_readings();

#endif // PI_TOP_HELIOS_H
