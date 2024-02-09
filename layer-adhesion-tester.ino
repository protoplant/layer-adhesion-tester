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


#include <EEPROM.h>

extern "C" {
  #include "src/hx711-pico-c/hx711.h"
  #include "src/hx711-pico-c/hx711_reader.pio.h"
  const hx711_config_t hxcfg = {
    .clock_pin = 17,
    .data_pin = 16,
    .pio = pio0,
    .pio_init = hx711_reader_pio_init,
    .reader_prog = &hx711_reader_program,
    .reader_prog_init = hx711_reader_program_init
  };
}

#define BREAK_SPEED 100
#define TEST_SAMPLES 150
#define HOME_SAMPLES 240
#define LC_MAX 100000
#define CALIBRATION_MASS 500.0f
#define MTR_OUT_PIN D14
#define MTR_IN_PIN D15


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


void promptMain() {
  printf("\n---=== Main Menu ===---\n");
  printf(" Global Commands:\n");
  printf("  v: view raw load cell data (toggle on/off)\n");
  printf("  z: zero load cell\n");
  printf(" Menu Commands:\n");
  printf("  t: test begin (during test <esc> or 'r' key will reset)\n");
  printf(" Sub Menus:\n");
  printf("  c: calibrate load cell\n");
  printf("  m: manual motor control\n");
  printf("All commands are lower case.  Upper case commands will be ignored.\n");
  printf("Type 'h' (help) to view these instructions again\n");
}

void promptManual() {
  printf("\n---=== Manual Motor Control ===---\n");
  printf(" Menu Commands:\n");
  printf("  0: stop");
  printf("  3-9: out");
  printf("  -: in");
  printf("  x: return to main menu\n");
}

void promptCalibrate() {
  printf("\n---=== Load Cell Calibration ===---\n");
  printf("Current Calibration:  Offset=%6i\tScale=%6i\n", offset, scale);
  printf(" Menu Commands:\n");
  printf("  z: zero load cell (make sure no weight is on the load cell)\n");
  printf("  c: calibrate load cell at %.0fg (make sure %.0fg calibration weight is on load cell)\n", CALIBRATION_MASS, CALIBRATION_MASS);
  printf("  s: save calibration to flash memory to be loaded on next reboot\n");
  printf("  x: return to main menu\n");
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

  Serial.begin();
  Serial.setDebugOutput(true);

  EEPROM.begin(256);
  EEPROM.get(0, offset);
  EEPROM.get(4, scale);
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

  lcRaw = hx711_get_value(&hx);
  gramsCur = (lcRaw+offset)/scale;
  if (gramsCur>gramsMax) gramsMax = gramsCur;

  spIn = 0;
  if (Serial.available()>0) spIn = Serial.read();
  if (state != 0) {
    if (spIn == 'v') viewData = !viewData;
    else if (spIn == 'z') offset=-lcRaw;
//    printf("%x\n", spIn);
  }
  if (viewData) printf("%i\n", gramsCur, gramsMax);

  switch (state) {
    case 0:  // init state: back up cylindar and wait for it to return home, eat up and ignore any stray chars in serial input buffer
      if (loopCount>HOME_SAMPLES) {
        mtrStop();
        printf("\n\n\n\n********   ProtoPlant Layer Adhesion Tester   ********\n");
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
      } else if (spIn=='9') {
        printf("Out 100%% Duty\n");
        mtrMoveOut(100);
      } else if (spIn=='0') {
        printf("Stop 0%% Duty\n");
        mtrStop();
      } else if (spIn=='-') {
        printf("In 100%% Duty\n");
        mtrMoveIn();
      } else if (spIn=='x') {
        mtrStop();
        promptMain();
        state = 1;
      } else if (spIn=='3') {
        printf("Out 30%% Duty\n");
        mtrMoveOut(30);
      } else if (spIn=='4') {
        printf("Out 40%% Duty\n");
        mtrMoveOut(40);
      } else if (spIn=='5') {
        printf("Out 50%% Duty\n");
        mtrMoveOut(50);
      } else if (spIn=='6') {
        printf("Out 60%% Duty\n");
        mtrMoveOut(60);
      } else if (spIn=='7') {
        printf("Out 70%% Duty\n");
        mtrMoveOut(70);
      } else if (spIn=='8') {
        printf("Out 80%% Duty\n");
        mtrMoveOut(80);
      }
    break;
    case 3:  // Load Cell Calibration
      if (spIn=='h') {
        promptCalibrate();
      } else if (spIn=='z') {
        printf("New Offset=%i\n", offset);
        EEPROM.put(0, offset);
      } else if (spIn=='c') {
        int32_t newScale=(lcRaw+offset)/CALIBRATION_MASS;
        printf("New Scale=%i\n", newScale);
        if (newScale>3||newScale<-3) {
          scale = newScale;
          EEPROM.put(4, scale);
        }
      } else if (spIn=='s') {
        EEPROM.commit();
        printf("Saved scale and offset to flash memory\n");
      } else if (spIn=='x') {
        promptMain();
        state = 1;
      }
    break;
    case 4:  // Test
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        if (spIn==0x1b || spIn=='r' || gramsCur>LC_MAX || loopCount>TEST_SAMPLES) testReset();
//      printf("%4i\t%8i\t%8i\t%8i\n", loopCount, gramsMax, gramsCur, gramsCur-gramsMax);
      printf("%i,%i\n", gramsCur, gramsMax);    // CSV format that "SerialPlot" can read
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



