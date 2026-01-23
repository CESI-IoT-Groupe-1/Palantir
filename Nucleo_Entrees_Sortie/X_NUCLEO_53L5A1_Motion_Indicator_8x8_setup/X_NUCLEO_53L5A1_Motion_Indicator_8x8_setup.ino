
/**
 ******************************************************************************
 * @file    X_NUCLEO_53L5A1_Motion_Indicator.ino
 * @author  STMicroelectronics
 * @version V1.0.0
 * @date    11 November 2021
 * @brief   Arduino test application for the X-NUCLEO-53L5A1 based on VL53L5CX
 *          proximity sensor.
 *          This application makes use of C++ classes obtained from the C
 *          components' drivers.
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; COPYRIGHT(c) 2021 STMicroelectronics</center></h2>
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of STMicroelectronics nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 */

#include <Wire.h>

#include <Arduino.h>
#include <vl53l5cx_class.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

#ifdef ARDUINO_SAM_DUE
  #define DEV_I2C Wire1
#else
  #define DEV_I2C Wire
#endif
#define SerialPort Serial

#ifndef LED_BUILTIN
  #define LED_BUILTIN 13
#endif
#define LedPin LED_BUILTIN

#define LPN_PIN 5
#define I2C_RST_PIN 3
#define PWREN_PIN A3
#define START_BYTE 0xAA
#define END_BYTE   0x55

// Components.
VL53L5CX sensor_vl53l5cx_top(&DEV_I2C, LPN_PIN, I2C_RST_PIN);

VL53L5CX_Motion_Configuration motion_config; /* Motion configuration*/

void blink_led_loop(void);

void blink_led_loop(void)
{
  do {
    // Blink the led forever
    digitalWrite(LedPin, HIGH);
    delay(500);
    digitalWrite(LedPin, LOW);
  } while (1);
}

void setup()
{
  uint8_t status;
  pinMode(LedPin, OUTPUT);
  
  // Power enable (X-NUCLEO)
  if (PWREN_PIN >= 0) {
    pinMode(PWREN_PIN, OUTPUT);
    digitalWrite(PWREN_PIN, HIGH);
    delay(10);
  }
  SerialPort.begin(115200);
  while (!SerialPort);
  SerialPort.println("VL53L5CX init...");
  
  // I2C
  DEV_I2C.begin();
  DEV_I2C.setClock(400000);
  
  // Sensor init
  sensor_vl53l5cx_top.begin();
  sensor_vl53l5cx_top.init_sensor();
  
  // --- RANGING RESOLUTION ---
  status = sensor_vl53l5cx_top.vl53l5cx_set_resolution(VL53L5CX_RESOLUTION_8X8);
  if (status) blink_led_loop();
  
  // --- MOTION INDICATOR ---
  status = sensor_vl53l5cx_top.vl53l5cx_motion_indicator_init(&motion_config, VL53L5CX_RESOLUTION_8X8);
  if (status) blink_led_loop();
  status = sensor_vl53l5cx_top.vl53l5cx_motion_indicator_set_resolution(&motion_config, VL53L5CX_RESOLUTION_8X8);
  if (status) blink_led_loop();
  status = sensor_vl53l5cx_top.vl53l5cx_motion_indicator_set_distance_motion(&motion_config, 400, 1500);
  if (status) blink_led_loop();
  
  // --- TIMING ---
  status = sensor_vl53l5cx_top.vl53l5cx_set_ranging_frequency_hz(10);
  if (status) blink_led_loop();

  // Start
  sensor_vl53l5cx_top.vl53l5cx_start_ranging();
  SerialPort.println("VL53L5CX ready (8X8)");
}

void loop()
{
  VL53L5CX_ResultsData Results;
  uint8_t NewDataReady;
  
  sensor_vl53l5cx_top.vl53l5cx_check_data_ready(&NewDataReady);
  
  if (NewDataReady)
  {
    sensor_vl53l5cx_top.vl53l5cx_get_ranging_data(&Results);
    
    int a_zone_count = 0;
    int b_zone_count = 0;

    for (int i = 0; i < 64; i++)
    {
      if (Results.distance_mm[i] > 200 && Results.distance_mm[i] < 1200)
      {
        if (i < 32) a_zone_count++;
        else b_zone_count++;
      }
    }

    uint8_t a_result = (a_zone_count > 5) ? 1 : 0;
    uint8_t b_result = (b_zone_count > 5) ? 1 : 0;

    // 📡 Envoi binaire compact
    Serial.write(START_BYTE);
    Serial.write(a_result);
    Serial.write(b_result);
    Serial.write(END_BYTE);

    // Debug USB lisible
    Serial.print("Sent -> A:");
    Serial.print(a_result);
    Serial.print(" B:");
    Serial.println(b_result);
  }

  delay(50);
}