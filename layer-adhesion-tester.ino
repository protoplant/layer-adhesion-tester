// MIT License
// 
// Copyright (c) 2024 Protoplant, makers of Protopasta
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#define BREAK_SPEED 100             //  % duty cycle of motor during a break test (100 = full speed/power)
#define TEST_SAMPLES 150            //  samples to count while extending cylinder for breaking test sample
#define HOME_SAMPLES 240            //  samples to count while retracting the cylinder to home position
#define LC_MAX 100000               //  test will abort if force on load cell exceeds this number (in grams, 100000 = 100kg)
#define CALIBRATION_MASS 500.0f     //  mass in grams of calibration weight
#define MTR_OUT_PIN D14             //  pin attached to motor driver pin that extends the cylinder
#define MTR_IN_PIN D15              //  pin attached to motor driver pin that retracts the cylinder
#define LC_CLOCK_PIN D17            //  pin attached to load cell amp clock pin
#define LC_DATA_PIN D16             //  pin attached to load cell amp data pin
#define LC_DEFAULT_OFFSET 11800     //  default offset to use before load cell is calibrated
#define LC_DEFAULT_SCALE 20         //  default scale to use before load cell is calibrated
#define EEPROM_KEY 0xCA1D           //  value to save to EEPROM (flash actually) that marks the Pi Pico as having calibration data in memory

#include <EEPROM.h>

extern "C" {
  #include "src/hx711-pico-c/hx711.h"
  #include "src/hx711-pico-c/hx711_reader.pio.h"
  const hx711_config_t hxcfg = {
    .clock_pin = LC_CLOCK_PIN,
    .data_pin = LC_DATA_PIN,
    .pio = pio0,
    .pio_init = hx711_reader_pio_init,
    .reader_prog = &hx711_reader_program,
    .reader_prog_init = hx711_reader_program_init
  };
}

SerialUSB cli;  // serial port for the Command Line Interface

hx711_t hx;
int32_t lcRaw;
int32_t offset;
int32_t scale;
int32_t gramsCur;
int32_t gramsMax = 0;

uint8_t state = 0;
uint32_t loopCount = 0;
int spIn = 0;
bool viewData = false;
bool lcCalDefault = true;


void promptMain() {
  cli.printf("\n---=== Main Menu ===---\n");
  cli.printf(" Global Commands:\n");
  cli.printf("  v: view raw load cell data (toggle on/off)\n");
  cli.printf("  z: zero load cell\n");
  cli.printf(" Menu Commands:\n");
  cli.printf("  t: test begin (during test <esc> or 'r' key will reset)\n");
  cli.printf(" Sub Menus:\n");
  cli.printf("  c: calibrate load cell\n");
  cli.printf("  m: manual motor control\n");
  cli.printf("All commands are lower case.  Upper case commands will be ignored.\n");
  cli.printf("Type 'h' (help) to view these instructions again\n");
}

void promptManual() {
  cli.printf("\n---=== Manual Motor Control ===---\n");
  cli.printf(" Menu Commands:\n");
  cli.printf("  <space>: stop");
  cli.printf("  3-0: out");
  cli.printf("  -: in");
  cli.printf("  x: return to main menu\n");
}

void promptCalibrate() {
  cli.printf("\n---=== Load Cell Calibration ===---\n");
  cli.printf("Current Calibration:  Offset=%6i\tScale=%6i", offset, scale);
  if (lcCalDefault) cli.printf("    (Default values)\n");
  else cli.printf("    (Values loaded from flash memory)\n");
  cli.printf(" Menu Commands:\n");
  cli.printf("  z: zero load cell (make sure no weight is on the load cell)\n");
  cli.printf("  c: calibrate load cell at %.0fg (make sure %.0fg calibration weight is on load cell)\n", CALIBRATION_MASS, CALIBRATION_MASS);
  cli.printf("  s: save calibration to flash memory to be loaded on next reboot\n");
  cli.printf("  r: reset flash memory and use default calibration\n");
  cli.printf("  x: return to main menu\n");
}

void mtrStop() {
  analogWrite(MTR_IN_PIN, 0);
  digitalWrite(MTR_OUT_PIN, LOW);
}

void mtrMoveOut(uint8_t speed) {
  if (speed>100) speed=100;
  analogWrite(MTR_IN_PIN, 100-speed);
  digitalWrite(MTR_OUT_PIN, HIGH);
}

void mtrMoveIn() {
  analogWrite(MTR_IN_PIN, 100);
  digitalWrite(MTR_OUT_PIN, LOW);
}

void testBegin() {
  viewData = false;
  offset=-lcRaw;
  gramsMax = 0;
  loopCount = 0;
  state = 4;
  mtrMoveOut(BREAK_SPEED);
}

void testReset() {
  digitalWrite(LED_BUILTIN, LOW);
  mtrMoveIn();
  loopCount = 0;
  state = 5;
}


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(MTR_OUT_PIN, OUTPUT);
  pinMode(MTR_IN_PIN, OUTPUT);

  analogWriteFreq(10000);
  analogWriteRange(100);

  cli.begin();

  EEPROM.begin(256);
  uint32_t key;
  EEPROM.get(0, key);
  if (key==EEPROM_KEY) {
    lcCalDefault = false;
    EEPROM.get(4, offset);
    EEPROM.get(8, scale);
  } else {
    lcCalDefault = true;
    offset = LC_DEFAULT_OFFSET;
    scale = LC_DEFAULT_SCALE;
  }
  if (scale==0) scale = 1; // avoid division by zero


  hx711_init(&hx, &hxcfg);

  // Power up the hx711 and set gain on chip
  hx711_power_up(&hx, hx711_gain_128);
  hx711_set_gain(&hx, hx711_gain_128);

  // Wait for readings to settle
  hx711_wait_settle(hx711_rate_10);

  mtrMoveIn();
}



void loop() {

  lcRaw = hx711_get_value(&hx);  // blocks until data is ready, approx. 12.5 miliseconds (data rate of 80 hz)
  gramsCur = (lcRaw+offset)/scale;
  if (gramsCur>gramsMax) gramsMax = gramsCur;

  spIn = 0;
  if (Serial.available()>0) spIn = Serial.read();
  if (state != 0) {
    if (spIn == 'v') viewData = !viewData;
    else if (spIn == 'z') offset=-lcRaw;
//    cli.printf("%x\n", spIn);
  }
  if (viewData) cli.printf("%i\n", gramsCur);

  switch (state) {
    case 0:  // init state: back up cylindar and wait for it to return home, eat up and ignore any stray chars in serial input buffer
      if (loopCount>HOME_SAMPLES) {
        mtrStop();
        cli.printf("\n\n\n\n********   ProtoPlant Layer Adhesion Tester   ********\n");
        promptMain();
        digitalWrite(LED_BUILTIN, HIGH);
        state = 1;
      }
    break;
    case 1:  // Main Menu
      if (spIn=='h' || spIn=='x') {
        promptMain();
      } else if (spIn=='m') {
        promptManual();
        state = 2;
      } else if (spIn=='c') {
        promptCalibrate();
        state = 3;
      } else if (spIn=='t') {
        testBegin();
      }
    break;
    case 2:  // Manual Motor Control
      if (spIn=='h') {
        promptManual();
      } else if (spIn=='x') {
        mtrStop();
        promptMain();
        state = 1;
      } else if (spIn=='-') {
        cli.printf("In 100%% Duty\n");
        mtrMoveIn();
      } else if (spIn==' ') {
        cli.printf("Stop 0%% Duty\n");
        mtrStop();
      } else if (spIn=='3') {
        cli.printf("Out 30%% Duty\n");
        mtrMoveOut(30);
      } else if (spIn=='4') {
        cli.printf("Out 40%% Duty\n");
        mtrMoveOut(40);
      } else if (spIn=='5') {
        cli.printf("Out 50%% Duty\n");
        mtrMoveOut(50);
      } else if (spIn=='6') {
        cli.printf("Out 60%% Duty\n");
        mtrMoveOut(60);
      } else if (spIn=='7') {
        cli.printf("Out 70%% Duty\n");
        mtrMoveOut(70);
      } else if (spIn=='8') {
        cli.printf("Out 80%% Duty\n");
        mtrMoveOut(80);
      } else if (spIn=='9') {
        cli.printf("Out 90%% Duty\n");
        mtrMoveOut(90);
      } else if (spIn=='0') {
        cli.printf("Out 100%% Duty\n");
        mtrMoveOut(100);
      }
    break;
    case 3:  // Load Cell Calibration
      if (spIn=='h') {
        promptCalibrate();
      } else if (spIn=='z') {
        cli.printf("New Offset=%i\n", offset);
        EEPROM.put(4, offset);
      } else if (spIn=='c') {
        int32_t newScale=(lcRaw+offset)/CALIBRATION_MASS;
        cli.printf("New Scale=%i\n", newScale);
        if (newScale>3||newScale<-3) {
          scale = newScale;
          EEPROM.put(8, scale);
        }
      } else if (spIn=='r') {
        EEPROM.put(0, 0);
        EEPROM.put(4, 0);
        EEPROM.put(8, 0);
        EEPROM.commit();
        lcCalDefault = true;
        offset = LC_DEFAULT_OFFSET;
        scale = LC_DEFAULT_SCALE;
        cli.printf("Flash memory reset, using default calibration\n");
      } else if (spIn=='s') {
        EEPROM.put(0, EEPROM_KEY);
        EEPROM.commit();
        lcCalDefault = false;
        cli.printf("Saved scale and offset to flash memory\n");
      } else if (spIn=='x') {
        promptMain();
        state = 1;
      }
    break;
    case 4:  // Test
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        if (spIn=='r' || gramsCur>LC_MAX || loopCount>TEST_SAMPLES) testReset();
//      cli.printf("%4i\t%8i\t%8i\t%8i\n", loopCount, gramsMax, gramsCur, gramsCur-gramsMax);
      cli.printf("%i,%i\n", gramsCur, gramsMax);    // CSV format that "SerialPlot" can read
    break;
    case 5:  // Home
      if (loopCount>HOME_SAMPLES) {
        mtrStop();
        digitalWrite(LED_BUILTIN, HIGH);
        promptMain();
        state = 1;
      }
    break;
  }

  ++loopCount;

}



