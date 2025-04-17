#include <Wire.h>
#include <TEA5767Radio.h>
#include <LiquidCrystal_I2C.h>

int frequencyPotPin = A0; // Potentiometer output connected to analog pin 0
int previousFrequency = 0;
// LCD initialization to the library with the number of the interface pins
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Radio module initialization
TEA5767Radio radio = TEA5767Radio();

void setup() {
  // Radio initialization
  Wire.begin();
  radio.setFrequency(93.7); // pick your own frequency
  Serial.begin(9600);

  // LCD initalization
  lcd.init(); // initialize the lcd
  lcd.backlight(); // Turn on backlight
  lcd.print("  Rhythm Relay");// Print a message to the LCD
}

void loop() {
  adjustRadioFrequency();
}

void adjustRadioFrequency()
{
  int averageFrequencyVal = 0;
  // collecting the average value of the potentiometer
  for(int i; i < 50; i++)
  {
    int frequencyPotVal = analogRead(frequencyPotPin);
    averageFrequencyVal = averageFrequencyVal + frequencyPotVal; 
  }
  averageFrequencyVal = averageFrequencyVal/50;

  // mapping the potentiometer value to a value between radio frequencies 87.0 - 107.00
  int frequencyInt = map(averageFrequencyVal, 0, 721, 8700, 10700); //Analog value to frequency from 87.0 MHz to 107.00 MHz 
  float frequency = frequencyInt/100.0f;


  // Adjust radio frequency
  if(frequency - previousFrequency >= 0.1f || previousFrequency - frequency >= 0.1f)
  {
    radio.setFrequency(frequency);
    lcd.print(String(frequency, 1));
    previousFrequency = frequency;    
  }
}
