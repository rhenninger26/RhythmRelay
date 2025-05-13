#include <Wire.h>
#include <TEA5767Radio.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <RFID.h>
#include <ArduinoBLE.h>

// --------- BLUETOOTH -----------
// BLE Service
BLEService messageService("19B10000-E8F2-537E-4F6C-D104768A1214"); // Custom BLE Service UUID

// BLE Characteristic for sending messages
BLEStringCharacteristic messageCharacteristic("19B10001-E8F2-537E-4F6C-D104768A1214", // Custom characteristic UUID
    BLERead | BLENotify, 50); // Allow remote device to read and get updates (notify), message up to 20 bytes

// --------- RFID -------------
RFID rfid(10, 9);
unsigned char status;
unsigned char str[MAX_LEN]; //MAX_LEN is 16: size of the array
unsigned char key[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; // Default key for authentication

// --------- PLAY/PAUSE BUTTON -----------
int buttonPin = 2;
bool radioOn = true;
bool lastButtonState = LOW;
bool currentButtonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// --------- RECORD MODE POTENTIOMETER -----------
int recordModePotPin = A2;
int recordModeChangeValue = 110; // the value in which the potentiometer will flip a bool to change into record mode

// -------- DC MOTOR ----------
int in1Pin = 5; // Define L293D channel 1 pin
int in2Pin = 3; // Define L293D channel 2 pin
int enable1Pin = 6; // Define L293D enable 1 pin
boolean rotationDir; // Define a variable to save the motor's rotation direction, true and false are represented by positive rotation and reverse rotation.
int rotationSpeed; // Define a variable to save the motor rotation speed

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

  // Motor initalization
  pinMode(in1Pin, OUTPUT);
  pinMode(in2Pin, OUTPUT);
  pinMode(enable1Pin, OUTPUT);

  // Bluetooth initalization
  if (!BLE.begin()) {
    Serial.println("Starting BLE failed!");
    while (1);
  }

  // Set advertised local name and service UUID
  BLE.setLocalName("ArduinoR4");
  BLE.setAdvertisedService(messageService);

  // Add the characteristic to the service
  messageService.addCharacteristic(messageCharacteristic);

  // Add service
  BLE.addService(messageService);

  // Set initial value for the characteristic
  messageCharacteristic.writeValue("Hello Phone!");

  // Start advertising
  BLE.advertise();

  Serial.println("Bluetooth device active, waiting for connections...");
}

void loop() {
    handleButtonPress();
    if (radioOn) {
        if(analogRead(recordModePotPin) >= recordModeChangeValue) {
            // start motor for record
            int potenVal = analogRead(A3); // Read potentiometer value
            // Determine rotation direction
            if (potenVal > 512) {
                rotationDir = true; // Clockwise
                rotationSpeed = potenVal - 512; // Speed based on deviation from center
            }
            else {
                rotationDir = false; // Counterclockwise
                rotationSpeed = 512 - potenVal; // Speed based on deviation from center
            }
            // Map the speed to a usable PWM range
            int pwmSpeed = map(rotationSpeed, 0, 512, 0, 255);

            // Drive the motor
            driveMotor(rotationDir, pwmSpeed);
            recordMode();
        }
        else {
            adjustRadioFrequency();
        }
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

// collect information from RFID tag to send to phone via bluetooth
void recordMode()
{
    // Adjust frequency to 87.5
    radio.setFrequency(87.5);

    // read RFID tag
    

    // ensure tag information is only sent once until a new record is used

    // Display track info if possible
}

void driveMotor(boolean dir, int spd) {
    // Control motor rotation direction
    if (dir) {
        digitalWrite(in1Pin, HIGH);
        digitalWrite(in2Pin, LOW);
    }
    else {
        digitalWrite(in1Pin, LOW);
        digitalWrite(in2Pin, HIGH);
    }

    // Control motor rotation speed
    analogWrite(enable1Pin, constrain(spd, 0, 255)); // Use the mapped speed
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
