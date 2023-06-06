#include <M5StickC.h>
#include "secrets.h"
#include "WiFi.h"
#include "AsyncUDP.h"

// GPIOs
#define ADC_PIN 36
#define STARLINK_POWER_PIN 26

#define ADC_10V 957
#define ADC_15V 1519
#define ADC_15_10V (ADC_15V - ADC_10V)
#define DELTA_V_REF 5.0

// battery voltage variables
const int avgArraySize = 10;
const double lowVLimit = 12.5;
const double shutdownVLimit = 5.0;
double batteryVolts;
int avgArrayIndex = 0;
double batteryVoltsArray[avgArraySize];  // array of last n voltage measurements used for trailing average

// power control variables
bool powerEnableStatus;  // currrent power-enable state

// button variables
bool buttonA = false;
bool buttonB = false;

// Display variables
int displayMode = 0;  // 0: volts, 1: time
int timeMode = 0; // 0: display, 1: set year, 2 set month...
char statusMsg[50];

// Time-related variables
int64_t secondsSinceStart = 0;
int64_t nextSecondTime;
int lastHour = 0;
// RTC time variables
RTC_TimeTypeDef RTC_TimeStruct;
RTC_DateTypeDef RTC_DateStruct;

// Network variables
bool wifiConnecting = false;
bool wifiConnected = false;
bool udpListening = false;
bool wifiSetupComplete = false;
const int udpPort = 6970;

IPAddress local_IP(192, 168, 8, 10);  // Static IP address
IPAddress gateway(192, 168, 8, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);   // optional
IPAddress secondaryDNS(8, 8, 4, 4); // optional

AsyncUDP udp;


bool connectWifi()
{
  if (!wifiConnecting)
  {
    Serial.println("working on setting up networking");
    // runs once
    WiFi.mode(WIFI_STA);
    // Configure static IP address
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      Serial.println("STA Failed to configure");
      delay(1000);
      return(false);
    }
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    wifiConnecting = true;
    return false;
  }

  if (!wifiConnected)
  {
    // polled until wifi connects
    if (WiFi.status() != WL_CONNECTED)
    {
      return false;
    }
    wifiConnected = true;
    M5.Lcd.setTextColor(TFT_YELLOW,TFT_BLACK);
    Serial.print("connected to wifi with IP: ");
    Serial.println(WiFi.localIP());
    return false;
  }

  if (!udpListening)
  {
    // set up lambda to call on receipt of a udp packet
    Serial.println("Setting up UDP server");
    if (udp.listen(udpPort))
    {
      Serial.print("UDP listening on port ");
      Serial.println(udpPort);
      udp.onPacket([](AsyncUDPPacket packet)
      {
        // this is a bloody lambda!
        Serial.print("Received packet from ");
        Serial.print(packet.remoteIP());
        Serial.print(", Length: ");
        Serial.print(packet.length());
        Serial.print(", Data: ");
        Serial.write(packet.data(), packet.length());
        Serial.println();
        if (!strncmp("status", (char*)packet.data(), 6))
        {
          Serial.println("Got status request");
          udp.writeTo((uint8_t*)statusMsg, strnlen(statusMsg, sizeof(statusMsg)), packet.remoteIP(), udpPort);
        }
        else if (!strncmp("toggle", (char*)packet.data(), 6))
        {
          Serial.println("Got toggle request");
          setPowerEnable(!powerEnableStatus);
        }
      });
      udpListening = true;
      M5.Lcd.setTextColor(TFT_WHITE,TFT_BLACK);
    }
  }
  return true;
}

double adc2Volts(int adcReading)
{
  double volts = 10.0 + (DELTA_V_REF / ADC_15_10V) * (adcReading - ADC_10V);
  // Serial.println(volts);
  return volts;
}

double readVolts()
{
  double v = 0.0;
  // read voltage & compute display value
  int adcValue = analogRead(ADC_PIN);
  double currentBatteryVolts = adc2Volts(adcValue);
  batteryVoltsArray[avgArrayIndex++] = currentBatteryVolts;
  avgArrayIndex %= avgArraySize;
  // compute trailing average of battery voltage measurements
  for (int i=0; i<avgArraySize; i++)
    v += batteryVoltsArray[i];
  v /= avgArraySize;
  // Serial.printf("current voltage: %0.2f averge: %0.2f\n", currentBatteryVolts, v);
  return v;
}

void displayVolts(double v, bool enable)
{
  String soc;

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0, 2);

  if (v >= 13.5) soc = "100%";
  else if (v >=13.4) soc = "99%";
  else if (v >=13.3) soc = "90-99%";
  else if (v >=13.2) soc = "70-90%";
  else if (v >=13.1) soc = "40-70%";
  else if (v >=13.0) soc = "30-40%";
  else if (v >=12.9) soc = "20-30%";
  else if (v >=12.8) soc = "10-20%";
  else if (v >=10.8) soc = "1-10%";
  else soc = "Unknown";

  sprintf(statusMsg, "%s, %0.2fV\nSoC %s", enable?"ON":"OFF", v, soc.c_str());
  M5.Lcd.print(statusMsg);
}

void resetDisplayMode()
{
  timeMode = 0;
  displayMode = 0;
}

void displayDateTime()
{
  // Display prompt for date/time field to set
  M5.Lcd.setCursor(0, 40);
  switch (timeMode)
  {
    case 0:
      M5.Lcd.println("M5: Set\ntime/date");
      if (buttonA)
        timeMode++;
      break;
    case 1:
      M5.Lcd.println("M5: set year");
      if (buttonA)
      {
        RTC_DateStruct.Year++;
        if (RTC_DateStruct.Year > 2028)
        {
          RTC_DateStruct.Year = 2023;
        }
        M5.Rtc.SetData(&RTC_DateStruct);
      }
      break;
    case 2:
      M5.Lcd.println("M5: set month");
      if (buttonA)
      {
        RTC_DateStruct.Month++;
        if (RTC_DateStruct.Month > 12)
        {
          RTC_DateStruct.Month = 1;
        }
        M5.Rtc.SetData(&RTC_DateStruct);
      }
      break;
    case 3:
      M5.Lcd.println("M5: set day-of-month");
      if (buttonA)
      {
        RTC_DateStruct.Date++;
        if (RTC_DateStruct.Date > 31)
        {
          RTC_DateStruct.Date = 1;
        }
        M5.Rtc.SetData(&RTC_DateStruct);
      }
      break;
    case 4:
      M5.Lcd.println("M5: set hour");
      if (buttonA)
      {
        RTC_TimeStruct.Hours++;
        if (RTC_TimeStruct.Hours > 23)
        {
          RTC_TimeStruct.Hours = 0;
        }
        M5.Rtc.SetTime(&RTC_TimeStruct);
      }
      break;
    case 5:
      M5.Lcd.println("M5: set minute");
      if (buttonA)
      {
        RTC_TimeStruct.Minutes++;
        if (RTC_TimeStruct.Minutes > 59)
        {
          RTC_TimeStruct.Minutes = 0;
        }
        M5.Rtc.SetTime(&RTC_TimeStruct);
      }
      break;
    case 6:
      M5.Lcd.println("M5: clear seconds");
      if (buttonA)
      {
        RTC_TimeStruct.Seconds = 0;
        M5.Rtc.SetTime(&RTC_TimeStruct);
      }
      break;
    default:
      resetDisplayMode();
  }
  M5.Lcd.setCursor(0, 1, 1);
  M5.Lcd.printf("%04d-%02d-%02d\n", RTC_DateStruct.Year,
  RTC_DateStruct.Month, RTC_DateStruct.Date);
  M5.Lcd.printf("%02d:%02d:%02d\n", RTC_TimeStruct.Hours,
    RTC_TimeStruct.Minutes, RTC_TimeStruct.Seconds);
}

void updateDisplay()
{
  M5.Lcd.fillScreen(BLACK);

  if (displayMode == 0)
  {
    displayVolts(batteryVolts, powerEnableStatus);
    // Serial.printf("Volts: %0.2f, powerEnable: %d\n", batteryVolts, powerEnableStatus);
  }
  else if (displayMode == 1)
    displayDateTime();
  else
    displayMode = 0;  // should never get here
  // clear buttonB after display functions have used it
}

void setPowerEnable(bool enable)
{
  if (enable != powerEnableStatus)
  {
    if (true == enable)
    {
      digitalWrite(STARLINK_POWER_PIN, HIGH);
      powerEnableStatus = true;
    }
    else
    {
      digitalWrite(STARLINK_POWER_PIN, LOW);
      powerEnableStatus = false;

    }
    Serial.printf("setPowerEnable(): enable: %d, powerEnableStatus %d\n", enable, powerEnableStatus);
  }
}

void midnight()
{
  Serial.println("Midnight");
  setPowerEnable(false);
}

void hoursUpdate()
{
  int currentHour = RTC_TimeStruct.Hours;
  if (currentHour != lastHour)
  {
    Serial.printf("New hour: %d\n", currentHour);

    if (0 == currentHour)
      midnight();
  }
}
    
void secondsUpdate()
{
  bool rv;
  // Check if we're connected to Wifi yet
  if (!wifiSetupComplete)
  {
    wifiSetupComplete = connectWifi();
  }

  // Get the time-of-day from the real-time clock for use by various called functions.
  M5.Rtc.GetTime(&RTC_TimeStruct);
  M5.Rtc.GetData(&RTC_DateStruct);

  if (buttonA)
  {
    setPowerEnable(!powerEnableStatus);
    buttonA = false;
    Serial.printf("ButtonA setting powerEnable to %d\n", powerEnableStatus);
  }

  batteryVolts = readVolts();
  if (batteryVolts < shutdownVLimit)
  {
    // power off when master switch turned off
    const int cancelDelaySec = 5;
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0, 2);
    M5.Lcd.println("Shutting\nDown");
    delay(2000);

    M5.Axp.PowerOff();
    // never reached
  }
  else if (batteryVolts < lowVLimit)
  {
    setPowerEnable(false);
  }

  // update the LCD display once/sec, blank it & hand off to display functions
  updateDisplay();

  // check if we are in a new hour and do hour-aligned work
  hoursUpdate();
}

void setup() {
  // initialize M5StickC
  M5.begin();
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0, 2);
  M5.Lcd.setTextColor(TFT_RED,TFT_BLACK);
  M5.Lcd.setTextSize(2);
  M5.update();
  Serial.begin(115200);

  // for Starlink telemetry, G36 is battery voltage input, G26 is Starlink power enable output
  gpio_pulldown_dis(GPIO_NUM_25);
  gpio_pullup_dis(GPIO_NUM_25);
  gpio_pulldown_dis(GPIO_NUM_36);
  gpio_pullup_dis(GPIO_NUM_36);

  pinMode(STARLINK_POWER_PIN, OUTPUT);
  digitalWrite(STARLINK_POWER_PIN, HIGH);
  powerEnableStatus = true;

  // initialize array for battery voltage trailing average
  for (int i=0; i<avgArraySize; i++)
    batteryVoltsArray[i] = 13.0;  // initialize with good value that doesn't trigger shutdown

  // initialize time
  int64_t now = esp_timer_get_time();
  nextSecondTime = now + 1000000;  // time to increment the seconds counter
  // Get the time-of-day from the real-time clock.
  M5.Rtc.GetTime(&RTC_TimeStruct);
  M5.Rtc.GetData(&RTC_DateStruct);
  lastHour = RTC_TimeStruct.Hours;
}

void loop() {
  // Read buttons
  M5.update();
  if (M5.BtnA.wasReleased())
  {
    buttonA = true;
  }
  if (M5.BtnB.wasReleased())
  {
    buttonB = true;
  }

  // Small button cycles through display mode & time set fields
  if (buttonB)
  {
    if (displayMode == 0)
      displayMode = 1;
    else if (displayMode == 1 && timeMode == 0)
      displayMode = 0;
    else if (displayMode == 1 && timeMode != 0)
      timeMode++;
    else
      displayMode = 0;
    
    buttonB = false;
  }

  // Check if it's time to increment the seconds-counter
  // and process everything that should be done each second
  int64_t now_us = esp_timer_get_time();
  if (now_us > nextSecondTime)
  {
    secondsSinceStart++;
    nextSecondTime += 1000000;
    // Serial.printf("%lld toggleTime: %d state %d\n", secondsSinceStart, nextFlowToggleTime, generateFlowPulses);
    secondsUpdate();
  }
}
