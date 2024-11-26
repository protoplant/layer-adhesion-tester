# layer-adhesion-tester
A device to measure layer adhesion in FDM-3D-printed parts. The results from testing printed samples with this device can be use to analyze slicer settings and material properties. 

![Protoplant Layer Adhesion Tester Illustration](img/Layer_Adhesion_Tester_drawing.jpg)

The device consists of a linear actuator and a load cell.  The load cell data is amplified and digitized by an HX711 breakout board, and the motor in the linear actuator is driven by a MOSFET H-bridge.  A Raspberry Pi Pico micro controller ties it all together and sends data thru a USB serial port for analysis and charting on a computer.

The code is formatted as an Arduino Sketch, and is ready for loading into the Arduino IDE for compiling.  The driver for the HX711 load cell amplifier was written by Daniel Robertson, and a subset of this library (minus the code that supports daisy-chaining multiple load cell amps together on the same bus) is included in the repository for easy compiling.  The full library and documentation can be found here: [hx711-pico-c](https://github.com/endail/hx711-pico-c)
 
# Building the Electronics

## Sourcing the Parts

Raspberry Pi Picos can be bought online from several different places.  However, the protoboard used for the project is unique and a bit harder to find.  It's nice because it clearly labels the Pi Pico pins, it has the same layout as a small breadboard, and it allows one to solder the Pi Pico directly to the board without needing to add headers.  (It also has holes if you want to use a Pi Pico H with pre-soldered headers.)

The "Pico Proto PCB" is made by MonkMakes, and at the time of this writing is available in the US from [PiShop.us](https://www.pishop.us/product/pico-proto-pcb/)

The load cell amp is a [breakout board based on the HX711 made by SparkFun](https://www.sparkfun.com/products/13879)

There are many options out there for load cell amps, and many others could be substituted.  It is important, however, that the sampling rate is high enough to get accurate results.  The layer adhesion tester can put a lot of force on the sample before it manages to break it, and the force on the load cell ramps up very quickly just before breakage.  So if the sample rate is too slow, you will get inconsistent results since the sample should ideally be taken just before breakage.  If the sample is taken a bit before breakage, and then after breakage, the time where "peak force" is on the load cell will be missed, and the device will show a lower breaking force than what actually happened.  The SparkFun load cell amp linked above has the option to increase the sampling rate to 80 hz by cutting a jumper on the back of the board labeled "RATE" - it is highly recommended to cut this jumper.  (It's easier to do this before it is soldered down.  Ask us how we know...)

One "gotcha" with this particular breakout board is that you can power the Wheatstone Bridge in your load cell with a different power supply than the on-board interface logic.  You can (arguably) gain an SNR edge by using a higher voltage for the AD conversion and bridge, so we have the bridge hooked up to VSYS on the Pi Pico (about 5 volts) and the logic hooked to 3.3 volts.  (See the photo below to see how this works.) Be careful to not mix this up, since the HX711 will happily run at 5V but the Pi Pico is not 5 volt tolerant and will likely not be as happy...  To be safe you could get away with hooking both VCC and VDD on the load cell amp to 3.3v.  (Will likely work fine, but we have not tested it that way.)

If you look carefully at the picture below of the soldered protoboard, you'll notice that one of the jumpers that connect the power rails to 3.3v is NOT soldered.  It's the one on the top right labeled "3V."  Just below that there is an orange jumper going to the Pi Pico.  You can't see the silk screen label in the photo, but it's labeled "Vs" for VSYS. In other words, the left positive power rail is set to 3.3V and the right positive power rail is set to 5V.  Both negative power rails go to ground.  These rails get power from the USB port.  The 12 volts for the motor comes from a separate power supply.

An interesting experiment would be to build this device with [this load cell amp](https://www.sparkfun.com/products/15242), also from SparkFun.  According to the data sheet, this chip can sample at 320 samples per second!  Of course, it can be assumed that a faster sample rate reduces or eliminates some internal oversampling and filtering done on the chip, and therefore will probably yield a noisier signal.  The added noise may negate any advantage that a fast sample rate would give you, so this would take some experimentation to prove out.

The H-bridge can be almost anything, even cooked up from scratch with parts you may already have lying around.  However, we found that the [DRV8871-based breakout from AdaFruit](https://www.adafruit.com/product/3190) fits this project perfectly.  By default, this board is current limited to 2 amps, which is just about perfect for this application.  That's plenty of power to break very strong test samples, yet not so much that you risk burning out the motor or melting wires.  

## Routing and Soldering the Wires onto a Protoboard

Below is an image showing one way to hook up the Pi Pico to the headers for the load cell amp and the motor driver:

![Photo of ProtoBoard Wire Routing](img/protoboard_layout.jpg)

## Placement of Load Cell Amp and H-Bridge

Here is what the finished circuit looks like all soldered together and ready to go:

![Photo of Finished Circuit](img/components_soldered.jpg)


# Compiling the Code

If you don't have it already, the Arduino IDE can be [downloaded here](https://www.arduino.cc/en/software)

The code in this repo uses an "external" Arduino core for the Raspberry Pi Pico called arduino-pico.  The Arduino IDE comes with a core for the Pi Pico called MBED-OS, but the arduino-pico core has many advantages and is well worth the trouble of installing it. Just follow the [instructions here](https://arduino-pico.readthedocs.io/en/stable/install.html) to set up the Arduino IDE with arduino-pico.

The advantages of arduino-pico core include the addition of printf() functions for debugging and serial output, an EEPROM emulator (for persistent storage to flash) and lower memory usage.  [Here](https://github.com/earlephilhower/arduino-pico/discussions/246) is a more in-depth discussion of the differences in the two cores.


# Operation and Setup

## USB Serial Command Line Interface

The device is controlled thru a "serial port emulator" that uses the USB port of the Pi Pico.  The Arduino IDE has a built in "serial monitor" that can be activated by clicking a button in the upper-right corner.  This works OK for testing things out, but it has the annoying property of sending a line of text at a time.  In other words, to send a command in Serial Monitor you need to select the "message box" at the top of the monitor, type in the single-letter command, and then hit the enter key.  If you are doing a lot of work with the menus and commands (especially testing the motor with "Manual Motor Control") you may find an alternate serial terminal program that just sends keystrokes without hitting the enter key every time (like PuTTY for Windows) much easier to work with.

When the device is started up, plugged in, and the serial monitor is active, you should see a greeting and some menu options for interacting with the device.  When everything is set up and ready to go, you just need the "t" command to run the test.  Everything is automatic from there.  The test is normally done using an application called "Serial Plot" which will create a nice pretty chart of the data as it comes in.  (See the [Testing Printed Samples](https://github.com/protoplant/layer-adhesion-tester?tab=readme-ov-file#testing-printed-samples) section below for details.)

The other menu options are for setup and testing the device.  Use "v" to view the load cell data.  Send another "v" to turn it off again.  Also, the zero point of the load cell varies quite a bit with temperature, so you can use "z" to set the zero point.  (Note that every time you use "t" to test a sample, the load cell is automatically zeroed.)

The "calibrate" function is optional, but recommended.  There are default values for the load cell in the code where it should be fairly accurate if you are using the same type of load cell we used.  If you just want to compare different samples to each other, you can get away with using these defaults.  If you would like to calibrate your load cell, there is a [section below](https://github.com/protoplant/layer-adhesion-tester?tab=readme-ov-file#calibrating-the-load-cell) describing the process.

The "Manual Motor Control" option is for testing the linear actuator (making sure it works and goes the right direction.)  There are options for using PWM at different duty cycles to make the cylinder of the linear actuator move out at a slower rate.  Breaking the sample more slowly may yield more accurate results, but if the layer adhesion is very strong, it might not break at all at lower duty cycles.  In our testing, the full speed break was able to break every sample we tried, so that is how the code is configured by default.  Feel free to experiment with this, however.

It should be noted that when the linear actuator is installed in the 3D printed frame, it can "crash" into the load cell, possibly damaging it.  So please be aware of this!  The code is set up by default to end the test before crashing, and it should work just fine if you are using the same linear actuator and same setup as designed.  But it would still be a good idea to test it with your setup before installing the linear actuator.


## Calibrating the Load Cell

If you are going to calibrate the load cell, it will be easier to do so before installing it into the frame.  Send the 'c' character to the device thru the USB serial port to enter the calibration sub-menu.  The current calibration values (Offset and Scale) are displayed above the "help menu."  If you have not calibrated the load cell before, these will be the default values of 11800 for offset and 20 for scale.  Make sure the load cell is sitting on a level surface, and that there is nothing on the load cell, and use the 'z' command to set the offset value (the zero point.)  Then, place your calibration weight onto the load cell and use the 'c' command to set the scale value.  At this point you need to use the 's' command to save the values to flash memory so that they will be used every time the device is powered up.  Note that the 'v' command (view data) can be use in the calibration sub menu to check the calibration before saving the values.  (Put a couple of objects of known weight on the load cell and make sure the number is close - the units are in grams.)

At ProtoPlant we have several 500g calibration weights sitting around in various places to calibrate our equipment, so the calibration weight is set to 500g.  If you have, for example, a 1kg weight instead, simply change this number in the code.  The line of code is near the top of the sketch, and looks like this:
```c
#define CALIBRATION_MASS 500.0f     //  mass in grams of calibration weight
```
If you don't have any calibration weights handy, but you have a fairly accurate scale, one way get the job done is to find some dense object, put it on your scale to get the number of grams it weighs, put this number into the code for the CALIBRATION_MASS, compile and upload, and then calibrate the load cell with this same object.  Note that the heavier the object is, the more accurate the calibration will be.  Of course, we need to be practical - calibrating the load cell at full scale would require a 200kg calibration weight!  Anything over 200g should suffice.


## Testing Printed Samples

It's possible to just run tests with the Arduino IDE Serial Monitor.  The force numbers are output in a CSV-like format which can be copied-and-pasted into a .CSV file and then opened in a spreadsheet application for further plotting and analysis.  There are two numbers output for each sample.  The first number is the force in grams, and the second number is the maximum amount of force recorded during the test.  The "max value" is just a convenience so you don't have to go searching thru a long list of samples to find the largest one.

Wait a minute... The "force in grams?"  What does THAT mean?  Grams is a unit of mass, not a unit of force!  OK, well, yes, but it seems intuitive to use a unit that most people are familiar with.  If you hold an object with X grams of mass on Planet Earth, you feel a force pulling down exactly proportional to that mass.  So it just kinda makes sense to us to use this pseudo-unit we call "grams of force."  Newtons are fig bars that you eat for a snack.  It would be rather easy to change the code and calibration to use any unit you want, however...

Serial Monitor will get the job done, but it's much nicer to work with an application that records the data to a file without the select-copy-paste hassle.  (It seems difficult to select multiple pages of numbers in Serial Monitor.  It only wants to select the samples that fit on the screen.)  And wouldn't a chart graphed in real time as you are running a test be great?  Well, there is a free and open source program that does all of this and more called "SerialPlot."  It is available for Linux and Windows, and someone somewhere probably compiled it for Mac if you dig a little.

Here is the [SerialPlot GitHub Page](https://github.com/hyOzd/serialplot)

Here is where [SerialPlot binaries](https://hackaday.io/project/5334-serialplot-realtime-plotting-software) can be found

Once you have SerialPlot installed and running, you'll have to do some setup tasks to get it ready to go.  To make this easier, there is a [SerialPlot "Settings" file](https://github.com/protoplant/layer-adhesion-tester/blob/main/serialplot.ini) in this repo that you can load into SerialPlot with the "Load Settings" menu option in the "File" menu.

In the "Port" tab of SerialPlot, Select the port for the device and click the "Open" button.  Then select the "Commands" tab and click the top "Send" button on the right.  This will send the "test" command to the device and you should see it cycle thru the test sequence.  You should also be seeing the chart above updated in real time.

There is another command (the bottom "Send" button) that you can use to abort a test sequence if necessary.  (The test sequence only takes a couple seconds, so you probably won't need to use this.)

After a test, you can "Export CSV" to get the data into a handy format for further analysis.  If you want to know the max force for a trial, just mouse-over the chart (on the right side after the break) and note the topmost number.  Also, there is a "record" feature in SerialPlot where you could record several tests into one file.  Finally, there is a "Snapshot" feature in SerialPlot that is handy for comparing trials to each other.



