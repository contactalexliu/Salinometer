/**
 * 
 * Arcadia High School Science Olympiad 2019-2020
 * Digital Salinometer Firmware for Water Quality
 * Rev. 01 by Alex Liu (2020/01/09), Originally by Michael Ratcliffe (2015/08/28)
 * (https://hackaday.io/project/7008-fly-wars-a-hackers-solution-to-world-hunger/log/24646-three-dollar-ec-ppm-meter-arduino)
 * 
 */

// ************************************************** BEGIN Function Switches ******************************************************************* //

// Enable/Disable automatic temperature compensation
#define AUTOTEMPCOMP_ENABLED true

// Enable/Disable printing extra information to serial monitor
#define DEBUG_ENABLED false

// ************************************************** END Function Switches ********************************************************************* //

#include <Arduino.h>
#include <Wire.h>
#include <U8x8lib.h>

#if AUTOTEMPCOMP_ENABLED
  #include <OneWire.h>
  #include <DallasTemperature.h>
#endif

// ************************************************** BEGIN User Defined Variables ************************************************************** //

// ################################################################################
// -----------  Do not replace R1 with a resistor lower than 300 ohms   -----------
// ################################################################################

// *************** Select Resistance of Voltage Divider *************** //
int R1 = 2000;

// *************** Add Resistance of Powering Pins *************** //
// (http://ww1.microchip.com/downloads/en/DeviceDoc/ATmega48A-PA-88A-PA-168A-PA-328-P-DS-DS40002061A.pdf, P.489, Figure 31-307)
// Ra = (3 - 2.2)/(0.020)
int Ra = 40;

int ECRead = A0;
int ECGround = A1;
int ECPower = A3;

// *************** PPM Conversion *************** //
// (https://www.naturalresources.sa.gov.au/samurraydarlingbasin/publications/measuring-salinity)
float PPMconversion = 0.55;

// *************** Cell Constant For EC Measurements *************** //
float K = 2.88;

#if AUTOTEMPCOMP_ENABLED
  // *************** Temperature Compensation *************** //
  // (https://www.onsetcomp.com/files/manual_pdfs/15260-B%20Conductivity%20Assistant%20Calculations%20Sea%20Water.pdf, P.4, Figure 1B)
  // TempCoeff = Average Calculated Temperature Coefficient in %/ºC
  float TempCoeff = 1.9387;

  // *************** Temperature Probe Setup *************** //
  #define ONE_WIRE_BUS 10          // Data wire for temp probe plugged into pin 10
  const int TempProbePositive = 8; // Temp Probe power connected to pin 9
  const int TempProbeNegative = 9; // Temp Probe ground connected to pin 8
#endif

// ************************************************** END User Defined Variables **************************************************************** //

U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE);

#if AUTOTEMPCOMP_ENABLED
  OneWire oneWire(ONE_WIRE_BUS);       // Setup a oneWire instance to communicate with OneWire devices
  DallasTemperature sensors(&oneWire); // Pass our oneWire reference to Dallas Temperature

  float Temperature = 10;
#endif

float EC = 0;
float EC25 = 0;
int ppm = 0;
float percent = 0;

float raw = 0;
float Vin = 5;
float Vdrop = 0;
float Rc = 0;
char charBuff[10];

// ************************************************** BEGIN Setup - Runs Once (Set Pins, etc.) ************************************************** //
void setup(void)
{
  u8x8.begin(); // Send setup sequence to display

  #if AUTOTEMPCOMP_ENABLED
    // Set temperature probe ground pin to sink current
    pinMode(TempProbeNegative, OUTPUT);
    digitalWrite(TempProbeNegative, LOW);

    // Set temperature probe power pin to source current +3V
    pinMode(TempProbePositive, OUTPUT);
    digitalWrite(TempProbePositive, HIGH);
  #endif

  pinMode(ECRead, INPUT);      // Use pin for ADC reading
  pinMode(ECPower, OUTPUT);    // Set conductivity probe power pin for sourcing current
  pinMode(ECGround, OUTPUT);   // Set conductivity probe ground pin for sinking current
  digitalWrite(ECGround, LOW); // We can leave ground connected permanently

  #if AUTOTEMPCOMP_ENABLED
    delay(100); // Give temperature sensor time to settle
    sensors.begin();
    delay(100);
  #endif

  // *************** Add Digital Pin Resistance to Voltage Divider Resistor *************** //
  // (http://www.element14.com/community/servlet/JiveServlet/showImage/38-21209-229476/Voltage+Divider.png)
  R1 = (R1 + Ra);

  u8x8.setFont(u8x8_font_chroma48medium8_u);
  u8x8.drawString(2, 0, "ARCADIA HIGH");
  u8x8.drawString(1, 1, "SCHOOL SCIENCE");
  u8x8.drawString(0, 2, "OLYMPIAD 2019-20");
  u8x8.setFont(u8x8_font_saikyosansbold8_u);
  u8x8.drawString(1, 4, "DIGITAL EC AND");
  u8x8.drawString(2, 5, "TEMP. SENSOR");
  delay(5000);
  u8x8.clear();

  for (int i = 0; i < 5; i++)
  {
    u8x8.setFont(u8x8_font_saikyosansbold8_u);
    u8x8.drawString(0, 0, "! !   NOTE   ! !");
    u8x8.drawString(2, 2, "MAKE SURE THE");
    u8x8.drawString(0, 4, "SOLUTION IS WELL");
    u8x8.drawString(0, 6, "MIXED BEFORE USE");
    delay(1000);
    u8x8.clear();
    delay(200);
  }
}
// ************************************************** END Setup ********************************************************************************* //

// ************************************************** BEGIN EC Function ************************************************************************* //
void GetEC()
{
  #if AUTOTEMPCOMP_ENABLED
    // *************** Read Temperature of Solution *************** //
    sensors.requestTemperatures();            // Get temperature
    Temperature = sensors.getTempCByIndex(0); // Store value
  #endif

  // *************** Estimate Resistance of Liquid *************** //
  digitalWrite(ECPower, HIGH);
  raw = analogRead(ECRead);
  raw = analogRead(ECRead); // This is not a mistake! First reading will be low beause it charges a capacitor.
  digitalWrite(ECPower, LOW);

  // *************** Convert to EC *************** //
  Vdrop = (Vin * raw) / 1024.0;
  Rc = (Vdrop * R1) / (Vin - Vdrop);
  Rc = Rc - Ra; // Account for power pin resistance
  EC = (K / Rc);

  #if AUTOTEMPCOMP_ENABLED
    // *************** Automatic Temperaure Compensation *************** //
    // (https://www.onsetcomp.com/files/manual_pdfs/15260-B%20Conductivity%20Assistant%20Calculations%20Sea%20Water.pdf, P.1, 1.)
    EC25 = EC / (1 - ((25.0 - Temperature) * TempCoeff / 100));

    // *************** Convert to PPM *************** //
    ppm = (EC25 * 1000) * (PPMconversion);  // Multiply by 1000 to convert mS/cm to µS/cm
    percent = (ppm / 10000);                // Convert ppm to g/mL * 100%
  #else
    // *************** Convert to PPM *************** //
    ppm = (EC * 1000) * (PPMconversion);  // Multiply by 1000 to convert mS/cm to µS/cm
    percent = (ppm / 10000);              // Convert ppm to g/mL * 100%
  #endif
}
// ************************************************** END EC Function *************************************************************************** //

// ************************************************** BEGIN Print Function ********************************************************************** //
void PrintReadings()
{
  u8x8.setFont(u8x8_font_8x13B_1x2_r);
  u8x8.drawString(0, 0, "Rc:");
  dtostrf(Rc, 6, 0, charBuff);
  u8x8.drawString(4, 0, charBuff);
  u8x8.drawString(12, 0, "ohms");

  #if AUTOTEMPCOMP_ENABLED
    u8x8.drawString(0, 2, "C25:");
    dtostrf(EC25, 3, 1, charBuff);
    u8x8.drawString(5, 2, charBuff);
    u8x8.drawString(11, 2, "mS/cm");
    u8x8.drawString(0, 4, "TDS:");
    dtostrf(percent, 2, 6, charBuff);
    u8x8.drawString(5, 4, charBuff);
    u8x8.drawUTF8(15, 4, "%");
    u8x8.drawString(0, 6, "Temp:");
    dtostrf(Temperature, 2, 1, charBuff);
    u8x8.drawString(6, 6, charBuff);
    u8x8.drawUTF8(10, 6, "°C");
  #else
    u8x8.drawString(0, 2, "EC:");
    dtostrf(EC, 2, 2, charBuff);
    u8x8.drawString(4, 2, charBuff);
    u8x8.drawString(10, 2, "mS/cm");
    u8x8.drawString(0, 4, "TDS:");
    dtostrf(percent, 2, 6, charBuff);
    u8x8.drawString(5, 4, charBuff);
    u8x8.drawUTF8(15, 4, "%");
  #endif

  #if DEBUG_ENABLED
    // *************** Debug Output *************** //
    delay(5000);
    u8x8.clear();
    u8x8.drawString(0, 0, "Drop:");
    dtostrf(Vdrop, 1, 2, charBuff);
    u8x8.drawString(6, 0, charBuff);
    u8x8.drawString(11, 0, "V");
    #if AUTOTEMPCOMP_ENABLED
      u8x8.drawString(0, 2, "EC:");
      dtostrf(EC, 2, 2, charBuff);
      u8x8.drawString(4, 2, charBuff);
      u8x8.drawString(10, 2, "mS/cm");
    #endif
  #endif
}
// ************************************************** END Print Function ************************************************************************ //

// ************************************************** BEGIN Main Loop - Runs Forever ************************************************************ //
// Heavy work in subroutines above to prevent cluttering the main loop
void loop(void)
{
  GetEC();         // Calls GetEC() loop. Don't call this more than (1/5) Hz or you will polarise the water!
  PrintReadings(); // Calls Serial Print routine.

  delay(5000); // 5 second delay for (1/5) Hz.
}
// ************************************************** END Main Loop ***************************************************************************** //