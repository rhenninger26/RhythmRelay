#include <Wire.h>
#include <TEA5767Radio.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <RFID.h>
#include <WiFiS3.h> // no library import, just included this line
#include "arduino_secrets.h"

// --------- RFID -------------
RFID rfid(10, 9);
unsigned char status;
unsigned char str[MAX_LEN]; //MAX_LEN is 16: size of the array

// --------- PLAY/PAUSE BUTTON -----------
int buttonPin = 2;
bool radioOn = true;
bool lastButtonState = LOW;
bool currentButtonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// -------- FREQUENCY POTENTIOMETER ---------
int frequencyPotPin = A0; // Potentiometer output connected to analog pin 0
int previousFrequency = 0;
unsigned long lastFrequencyCheck = 0;
const unsigned long frequencyCheckInterval = 200; // Check every 100ms instead 

// ------------- LCD ----------
// LCD initialization to the library with the number of the interface pins
LiquidCrystal_I2C lcd(0x27, 16, 2);

// -------- RADIO MODULE ----------
// Radio module initialization
TEA5767Radio radio = TEA5767Radio();

// ----------- WIFI ----------
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
int wifiStatus = WL_IDLE_STATUS;

void setup() {
  // Radio initialization
  Wire.begin();
  Serial.begin(9600);

  // LCD initalization
  lcd.init(); // initialize the lcd
  lcd.backlight(); // Turn on backlight
  lcd.print("  Rhythm Relay");// Print a message to the LCD

  // Button initalization
  pinMode(buttonPin, INPUT_PULLUP);  // Use internal pull-up resistor
  currentButtonState = digitalRead(buttonPin);
  lastButtonState = currentButtonState;

  // RFID initalization
  SPI.begin();
  rfid.init(); //initialization
  
  // WiFi initalization
  Serial.println("Connecting to wifi...");
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  while (wifiStatus != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);

    // Connect to WPA/WPA2 network:
    wifiStatus = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }

  // you're connected now, so print out the data:
  Serial.print("You're connected to the network");
}

void loop() {

    handleButtonPress();
    if (radioOn) {
        adjustRadioFrequency();
    }
}

void handleButtonPress() {
    // Read the current state
    bool reading = digitalRead(buttonPin);

    // If the reading has changed
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }

    // Wait for the debounce delay
    if ((millis() - lastDebounceTime) > debounceDelay) {
        // If the button state has changed
        if (reading != currentButtonState) {
            currentButtonState = reading;

            // Only toggle on button press (rising edge)
            if (currentButtonState == HIGH) {
                radioOn = !radioOn;

                // Update display and radio state
                lcd.clear();
                if (radioOn) {
                    radio.setFrequency(previousFrequency); // Restore last frequency
                    lcd.print(String(previousFrequency, 1));

                }
                else {
                    radio.setFrequency(0.0); // Set frequency to 0 MHz to effectively disable output
                    lcd.print("Radio OFF");
                }
            }
        }
    }

    // Save the reading for next time
    lastButtonState = reading;
}

void adjustRadioFrequency()
{
    // Only check frequency every 100ms
    if (millis() - lastFrequencyCheck >= frequencyCheckInterval) {
        lastFrequencyCheck = millis();

        int averageFrequencyVal = 0;
        // Reduce samples to decrease processing time
        for (int i = 0; i < 10; i++)  // Reduced from 50 to 10 samples
        {
            int frequencyPotVal = analogRead(frequencyPotPin);
            averageFrequencyVal = averageFrequencyVal + frequencyPotVal;
        }
        averageFrequencyVal = averageFrequencyVal / 10;

        // mapping the potentiometer value to a value between radio frequencies 87.0 - 107.00
        int frequencyInt = map(averageFrequencyVal, 0, 721, 8700, 10700);
        float frequency = frequencyInt / 100.0f;

        // Only update if there's a significant change
        if (abs(frequency - previousFrequency) >= 0.1f)
        {
            radio.setFrequency(frequency);
            delay(10);  // Give the radio module time to stabilize

            // Update LCD only when frequency actually changes
            lcd.clear();
            lcd.print(String(frequency, 1));
            previousFrequency = frequency;
        }
    }
}
