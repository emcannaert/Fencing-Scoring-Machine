#include <Adafruit_NeoPixel.h>

#define PIN 6        // data pin connected to LED matrix
#define NUMPIXELS 64 // total number of LEDs in the matrix

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup() 
{
  pixels.begin();
  pixels.show(); // Initialize all pixels to 'off'
  Serial.begin(9600);  // match the controller baud rate
}

void loop() 
{
  if (Serial.available() > 0) 
  {
    char cmd = Serial.read();

    uint32_t color;

    switch (cmd) 
    {
      case 'R': 
        color = pixels.Color(255, 0, 0); // Red
        break;
      case 'G':
        color = pixels.Color(0, 255, 0); // Green
        break;
      case 'W':
        color = pixels.Color(255, 255, 255); // White
        break;
      case 'O': 
        color = pixels.Color(0, 0, 0); // Off
        break;
      default:
        return; // ignore unknown commands
    }

    // set all pixels to the selected color
    for (int i = 0; i < NUMPIXELS; i++) 
    {
      pixels.setPixelColor(i, color);
    }
    pixels.show();
  }
