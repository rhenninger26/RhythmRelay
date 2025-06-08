#include <Wire.h>
#include <TEA5767Radio.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <RFID.h>
#include <WiFiS3.h>
#include "arduino_secrets.h"

// --------- WIFI -----------
WiFiSSLClient client;
int wifiStatus = WL_IDLE_STATUS;

// --------- RFID -------------
RFID rfid(10, 9);
#ifndef MAX_LEN
#define MAX_LEN 16
#endif
unsigned char status;
unsigned char str[MAX_LEN]; //MAX_LEN is 16: size of the array
unsigned char key[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; // Default key for authentication

char lastReadUrl[300] = "";
bool urlPrinted = false;

// --------- PLAY/PAUSE BUTTON -----------
int buttonPin = 2;
bool radioOn = true;
bool lastButtonState = LOW;
bool currentButtonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// --------- RECORD MODE POTENTIOMETER -----------
int recordModePotPin = A2;
int recordModeChangeValue = 50; // the value in which the potentiometer will flip a bool to change into record mode

// -------- DC MOTOR ----------
// int in1Pin = 5; // Define L293D channel 1 pin
// int in2Pin = 3; // Define L293D channel 2 pin
// int enable1Pin = 6; // Define L293D enable 1 pin
// boolean rotationDir; // Define a variable to save the motor's rotation direction, true and false are represented by positive rotation and reverse rotation.
// int rotationSpeed; // Define a variable to save the motor rotation speed

// -------- FREQUENCY POTENTIOMETER ---------
int frequencyPotPin = A0; // Potentiometer output connected to analog pin 0
int previousFrequency = 87.5;
unsigned long lastFrequencyCheck = 0;
const unsigned long frequencyCheckInterval = 200; // Check every 200ms instead 

// ------------- LCD ----------
// LCD initialization to the library with the number of the interface pins
LiquidCrystal_I2C lcd(0x27, 16, 2);
bool isLcdOn = true;

// -------- RADIO MODULE ----------
TEA5767Radio radio = TEA5767Radio();

// ------- PUSHOVER API -----------
const char* pushoverHost = "api.pushover.net";
const int   pushoverPort = 443;
const char* pushoverToken = PUSHOVER_TOKEN;
const char* pushoverUser = PUSHOVER_ID;

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
	rfid.init();

	// Motor initalization
	// pinMode(in1Pin, OUTPUT);
	// pinMode(in2Pin, OUTPUT);
	// pinMode(enable1Pin, OUTPUT);

	// WiFi initalization
	Serial.println("Connecting to wifi...");

	if (WiFi.status() == WL_NO_MODULE)
	{
		Serial.println("Communication with WiFi module failed!");
		while (true);
	}

	String fv = WiFi.firmwareVersion();
	if (fv < WIFI_FIRMWARE_LATEST_VERSION)
	{
		Serial.println("Please upgrade the firmware");
	}

	while (wifiStatus != WL_CONNECTED)
	{
		Serial.print("Attempting to connect to WPA SSID: ");
		Serial.println(SECRET_SSID);
		wifiStatus = WiFi.begin(SECRET_SSID, SECRET_PASS);
		delay(10000);
	}
	Serial.println("You're connected to the network");
}

void loop() {
	handleButtonPress();
	if (radioOn) 
    {
		if (analogRead(recordModePotPin) >= recordModeChangeValue) 
        {
			//start motor for record
			// int potenVal = analogRead(A3); // Read potentiometer value
			// // Determine rotation direction
			// if (potenVal > 512) 
      //       {
			// 	rotationDir = true; // Clockwise
			// 	rotationSpeed = potenVal - 512; // Speed based on deviation from center
			// }
			// else {
			// 	rotationDir = false; // Counterclockwise
			// 	rotationSpeed = 512 - potenVal; // Speed based on deviation from center
			// }
			// // Map the speed to a usable PWM range
			// int pwmSpeed = map(rotationSpeed, 0, 512, 0, 255);

			// // Drive the motor
			// driveMotor(rotationDir, pwmSpeed);
			recordMode();
		}
		else 
        {
			// Make sure display is on when in radio mode
			if (!isLcdOn) 
			{
				lcd.display();
				lcd.backlight();
				lcd.clear();
				lcd.print(String(previousFrequency, 1));
				isLcdOn = true;
			}
			adjustRadioFrequency();
		}
	}
}

void handleButtonPress() 
{
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

// collect information from NFC tag to send to phone
void recordMode() 
{
	// Turn off LCD display for record mode
	lcd.clear();
	lcd.print("Record Mode");
	delay(1000);  // Show "Record Mode" briefly before turning off
	lcd.noDisplay();
	lcd.noBacklight();
	isLcdOn = false;  // Track LCD state

	// Adjust frequency to 87.5
	radio.setFrequency(87.5);

	// Process NFC scanning
	collectAndSendURLFromTag();
}

void collectAndSendURLFromTag()
{
	String extractedUrl = ""; // String to store and return the URL

	if (rfid.findCard(PICC_REQIDL, str) == MI_OK)
	{
		if (!urlPrinted) {
			Serial.println("Find the card!");
			ShowCardType(str);

			if (rfid.anticoll(str) == MI_OK) {
				Serial.print("The card's number is : ");
				for (int i = 0; i < 4; i++)
				{
					Serial.print(0x0F & (str[i] >> 4), HEX);
					Serial.print(0x0F & str[i], HEX);
				}
				Serial.println("");

				// For NTAG215, each page is 4 bytes, and there are 135 pages (0-134)
				unsigned char readPage[4];
				unsigned char allData[256];
				int totalBytesRead = 0;
				bool readFailed = false;

				// Read pages 4 to 30 (adjust as needed)
				for (int page = 4; page <= 30; page++) {
					if (rfid.read(page, readPage) == MI_OK) {
						for (int i = 0; i < 4; i++) {
							if (totalBytesRead < 256) {
								allData[totalBytesRead++] = readPage[i];
							}
						}
					}
					else {
						readFailed = true;
						break;
					}
				}

				// Extract and print only the URL from the NFC data
				char url[200];
				int urlIndex = 0;
				bool urlFound = false;

				// Find the start of the URL ("http")
				for (int i = 0; i < totalBytesRead - 4; i++) {
					if (allData[i] == 'h' && allData[i + 1] == 't' && allData[i + 2] == 't' && allData[i + 3] == 'p') {
						// Copy from here until a non-printable or NDEF terminator (0xFE)
						for (int j = i; j < totalBytesRead && urlIndex < sizeof(url) - 1; j++) {
							if (allData[j] < 32 || allData[j] > 126 || allData[j] == 0xFE) {
								break;
							}
							url[urlIndex++] = (char)allData[j];
						}
						urlFound = true;
						break;
					}
				}
				url[urlIndex] = '\0'; // Null-terminate the string

				if (urlFound) {
					Serial.print("Extracted URL: ");
					Serial.println(url);

					// Store the URL for future connections
					strncpy(lastReadUrl, url, sizeof(lastReadUrl) - 1);
					lastReadUrl[sizeof(lastReadUrl) - 1] = '\0'; // Ensure null-termination
					//hasStoredUrl = true;

					// // Send the URL
					sendPushoverNotification(url);
					Serial.println("Sent URL via Pushover");
				}
				else {
					Serial.println("No URL found in NFC data.");
				}
				urlPrinted = true;
			}
		}
	}
	else {
		urlPrinted = false; // Reset flag when no card is present
	}
	rfid.halt(); // command the card to enter sleeping state
}

void sendPushoverNotification(const String& targetURL) {
	String postData = "token=" + String(pushoverToken) +
		"&user=" + String(pushoverUser) +
		"&message=" + targetURL +
		"&url=" + targetURL +
		"&url_title=" + "Launch+Website";

	Serial.print("Connecting to Pushover... ");
	if (!client.connect(pushoverHost, pushoverPort)) {
		Serial.println("Connection failed.");
		return;
	}
	Serial.println("Connected!");

	String request = String("POST ") + "/1/messages.json" + " HTTP/1.1\r\n" +
		"Host: " + pushoverHost + "\r\n" +
		"User-Agent: Arduino\r\n" +
		"Content-Type: application/x-www-form-urlencoded\r\n" +
		"Content-Length: " + postData.length() + "\r\n" +
		"Connection: close\r\n\r\n" +
		postData;

	client.print(request);
	Serial.println("Request sent:");
	Serial.println(request);

	Serial.println("Server response:");
	while (client.connected() || client.available()) {
		if (client.available()) {
			String line = client.readStringUntil('\n');
			Serial.println(line);
		}
	}
	client.stop();
	Serial.println("Connection closed.");
}

void ShowCardType(unsigned char* type)
{
	Serial.print("Card type: ");
	Serial.print("Raw type bytes: ");
	Serial.print(type[0], HEX);
	Serial.print(" ");
	Serial.println(type[1], HEX);

	if (type[0] == 0x04 && type[1] == 0x00)
		Serial.println("MFOne-S50");
	else if (type[0] == 0x02 && type[1] == 0x00)
		Serial.println("MFOne-S70");
	else if (type[0] == 0x44 && type[1] == 0x00)
		Serial.println("MF-UltraLight");
	else if (type[0] == 0x08 && type[1] == 0x00)
		Serial.println("MF-Pro");
	else if (type[0] == 0x44 && type[1] == 0x03)
		Serial.println("MF Desire");
	else if (type[0] == 0x04 && type[1] == 0x03)
		Serial.println("NTAG215");
	else
		Serial.println("Unknown");
}

// void driveMotor(boolean dir, int spd)
// {
// 	// Control motor rotation direction
// 	if (dir) {
// 		digitalWrite(in1Pin, HIGH);
// 		digitalWrite(in2Pin, LOW);
// 	}
// 	else {
// 		digitalWrite(in1Pin, LOW);
// 		digitalWrite(in2Pin, HIGH);
// 	}

// 	// Control motor rotation speed
// 	analogWrite(enable1Pin, constrain(spd, 0, 255)); // Use the mapped speed
// }

void adjustRadioFrequency()
{
	// Only check frequency every 200ms
	if (millis() - lastFrequencyCheck >= frequencyCheckInterval)
	{
		lastFrequencyCheck = millis();

		int averageFrequencyVal = 0;
		// Reduce samples to decrease processing time
		for (int i = 0; i < 30; i++)
		{
			int frequencyPotVal = analogRead(frequencyPotPin);
			averageFrequencyVal = averageFrequencyVal + frequencyPotVal;
		}
		averageFrequencyVal = averageFrequencyVal / 30;

		// mapping the potentiometer value to a value between radio frequencies 87.0 - 107.00
		int frequencyInt = map(averageFrequencyVal, 0, 729, 8700, 10700);
		float frequencyRaw = frequencyInt / 100.0f;

		// Stabilize to odd decimal places (x.1, x.3, x.5, x.7, x.9)
		float frequency = stabilizeToOddDecimals(frequencyRaw);

		// Only update if the stabilized frequency has changed
		if (frequency != previousFrequency)
		{
			Wire.endTransmission(); // End any pending transmissions
			delay(5);
			radio.setFrequency(frequency);
			delay(20);  // Increase stabilization time

			// Update LCD with more delay
			Wire.endTransmission();
			delay(5);
			lcd.clear();
			delay(5);
			lcd.print(String(frequency, 1));
			previousFrequency = frequency;
		}
	}
}

// Helper function to stabilize frequency to odd decimal places
float stabilizeToOddDecimals(float rawFrequency)
{
	// Extract the integer and fractional parts
	int intPart = (int)rawFrequency;
	float fracPart = rawFrequency - intPart;

	// Round to the nearest odd decimal (0.1, 0.3, 0.5, 0.7, 0.9)
	int roundedFrac = round(fracPart * 10);

	// Handle rollover
	if (roundedFrac >= 10) 
	{
		roundedFrac = 1;
		intPart += 1;
	}

	// Ensure it's odd (1, 3, 5, 7, 9)
	if (roundedFrac % 2 == 0) 
	{
		if (roundedFrac == 0) 
		{
			// If the rounded fraction is exactly 0 (e.g., 100.0), snap to 0.1 (the lowest odd decimal)
			roundedFrac = 1;
		}
		else if (roundedFrac == 10) 
		{
			// If the rounded fraction is 10 (e.g., 100.95 rounds to 10), roll over to the next integer and set to 0.1
			roundedFrac = 1;
			intPart += 1;
		}
		else 
		{
			// For any other even value, adjust to the nearest odd value:
			// If the original fraction was just above the even value, round up to the next odd;
			// otherwise, round down to the previous odd.
			roundedFrac = (fracPart * 10 > roundedFrac) ? roundedFrac + 1 : roundedFrac - 1;
		}
	}

	// Combine parts back to final frequency
	return intPart + (roundedFrac / 10.0);
}
