/*
 * Web enabled FFT VU meter for a matrix, ESP32 and INMP441 digital mic.
 * The matrix width MUST be either 8 or a multiple of 16 but the height can
 * be any value. E.g. 8x8, 16x16, 8x10, 32x9 etc.
 * 
 * We are using the LEDMatrx library for easy setup of a matrix with various
 * wiring options. Options are:
 *  HORIZONTAL_ZIGZAG_MATRIX
 *  HORIZONTAL_MATRIX
 *  VERTICAL_ZIGZAG_MATRIX
 *  VERTICAL_MATRIX
 * If your matrix has the first pixel somewhere other than the bottom left
 * (default) then you can reverse the X or Y axis by writing -M_WIDTH and /
 * or -M_HEIGHT in the cLEDMatrix initialisation.
 * 
 * REQUIRED LIBRARIES
 * FastLED            Arduino libraries manager
 * ArduinoFFT         Arduino libraries manager
 * EEPROM             Built in
 * WiFi               Built in
 * AsyncTCP           https://github.com/me-no-dev/ESPAsyncWebServer
 * ESPAsyncWebServer  https://github.com/me-no-dev/AsyncTCP
 * LEDMatrix          https://github.com/AaronLiddiment/LEDMatrix
 * LEDText            https://github.com/AaronLiddiment/LEDText
 * 
 * WIRING
 * LED data     D2 via 470R resistor
 * GND          GND
 * Vin          5V
 * 
 * INMP441
 * VDD          3V3
 * GND          GND
 * L/R          GND
 * WS           D15
 * SCK          D14     
 * SD           D32
 * 
 * REFERENCES
 * Main code      Scott Marley            https://www.youtube.com/c/ScottMarley
 * Web server     Random Nerd Tutorials   https://randomnerdtutorials.com/esp32-web-server-slider-pwm/
 *                                  and   https://randomnerdtutorials.com/esp32-websocket-server-arduino/
 * Audio and mic  Andrew Tuline et al     https://github.com/atuline/WLED
 */

#include "audio_reactive.h"
#include <FastLED.h>
#include <LEDMatrix.h>
#include <LEDText.h>
#include <FontMatrise.h>
#include <EEPROM.h>

#define EEPROM_SIZE 5
#define LED_PIN     2
#define M_WIDTH     16
#define M_HEIGHT    16
#define NUM_LEDS    (M_WIDTH * M_HEIGHT)

#define EEPROM_BRIGHTNESS   0
#define EEPROM_GAIN         1
#define EEPROM_SQUELCH      2
#define EEPROM_PATTERN      3
#define EEPROM_DISPLAY_TIME 4

uint8_t numBands;
uint8_t barWidth;
uint8_t pattern;
uint8_t brightness;
uint16_t displayTime;
bool autoChangePatterns = false;

#include "web_server.h"

cLEDMatrix<M_WIDTH, M_HEIGHT, HORIZONTAL_ZIGZAG_MATRIX> leds;
cLEDText ScrollingMsg;

uint8_t peak[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t prevFFTValue[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t barHeights[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// Colors and palettes
DEFINE_GRADIENT_PALETTE( purple_gp ) {
  0,   0, 212, 255,   //blue
255, 179,   0, 255 }; //purple
DEFINE_GRADIENT_PALETTE( outrun_gp ) {
  0, 141,   0, 100,   //purple
127, 255, 192,   0,   //yellow
255,   0,   5, 255 };  //blue
DEFINE_GRADIENT_PALETTE( greenblue_gp ) {
  0,   0, 255,  60,   //green
 64,   0, 236, 255,   //cyan
128,   0,   5, 255,   //blue
192,   0, 236, 255,   //cyan
255,   0, 255,  60 }; //green
DEFINE_GRADIENT_PALETTE( redyellow_gp ) {
  0,   200, 200,  200,   //white
 64,   255, 218,    0,   //yellow
128,   231,   0,    0,   //red
192,   255, 218,    0,   //yellow
255,   200, 200,  200 }; //white
CRGBPalette16 purplePal = purple_gp;
CRGBPalette16 outrunPal = outrun_gp;
CRGBPalette16 greenbluePal = greenblue_gp;
CRGBPalette16 heatPal = redyellow_gp;
uint8_t colorTimer = 0;

void setup() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds[0], NUM_LEDS);
  Serial.begin(57600);

  setupWebServer();
  setupAudio();

  if (M_WIDTH == 8) numBands = 8;
  else numBands = 16;
  barWidth = M_WIDTH / numBands;
  
  EEPROM.begin(EEPROM_SIZE);
  
  // It should not normally be possible to set the gain to 255
  // If this has happened, the EEPROM has probably never been written to
  // (new board?) so reset the values to something sane.
  if (EEPROM.read(EEPROM_GAIN) == 255) {
    EEPROM.write(EEPROM_BRIGHTNESS, 50);
    EEPROM.write(EEPROM_GAIN, 0);
    EEPROM.write(EEPROM_SQUELCH, 0);
    EEPROM.write(EEPROM_PATTERN, 0);
    EEPROM.write(EEPROM_DISPLAY_TIME, 10);
    EEPROM.commit();
  }

  // Read saved values from EEPROM
  FastLED.setBrightness( EEPROM.read(EEPROM_BRIGHTNESS));
  brightness = FastLED.getBrightness();
  gain = EEPROM.read(EEPROM_GAIN);
  squelch = EEPROM.read(EEPROM_SQUELCH);
  pattern = EEPROM.read(EEPROM_PATTERN);
  displayTime = EEPROM.read(EEPROM_DISPLAY_TIME);

  if (WiFi.status() == WL_CONNECTED) showIP();
}  

void loop() {
  if (pattern != 5) FastLED.clear();
  
  uint8_t divisor = 1;                                                    // If 8 bands, we need to divide things by 2
  if (numBands == 8) divisor = 2;                                         // and average each pair of bands together
  
  for (int i = 0; i < 16; i += divisor) {
    uint8_t fftValue;
    
    if (numBands == 8) fftValue = (fftResult[i] + fftResult[i+1]) / 2;    // Average every two bands if numBands = 8
    else fftValue = fftResult[i];

    fftValue = ((prevFFTValue[i/divisor] * 3) + fftValue) / 4;            // Dirty rolling average between frames to reduce flicker
    barHeights[i/divisor] = fftValue / (255 / M_HEIGHT);                  // Scale bar height
    
    if (barHeights[i/divisor] > peak[i/divisor])                          // Move peak up
      peak[i/divisor] = min(M_HEIGHT, (int)barHeights[i/divisor]);
      
    prevFFTValue[i/divisor] = fftValue;                                   // Save prevFFTValue for averaging later
    
  }

  // Draw the patterns
  for (int band = 0; band < numBands; band++) {
    drawPatterns(band);
  }

  // Decay peak
  EVERY_N_MILLISECONDS(60) {
    for (uint8_t band = 0; band < numBands; band++)
      if (peak[band] > 0) peak[band] -= 1;
  }

  EVERY_N_SECONDS(30) {
    // Save values in EEPROM. Will only be commited if values have changed.
    EEPROM.write(EEPROM_BRIGHTNESS, brightness);
    EEPROM.write(EEPROM_GAIN, gain);
    EEPROM.write(EEPROM_SQUELCH, squelch);
    EEPROM.write(EEPROM_PATTERN, pattern);
    EEPROM.write(EEPROM_DISPLAY_TIME, displayTime);
    EEPROM.commit();
  }
  
  EVERY_N_SECONDS_I(timingObj, displayTime) {
    timingObj.setPeriod(displayTime);
    if (autoChangePatterns) pattern = (pattern + 1) % 6;
  }
  
  FastLED.setBrightness(brightness);
  FastLED.show();
  delay(1);

  ws.cleanupClients();
}

void drawPatterns(uint8_t band) {
  
  uint8_t barHeight = barHeights[band];
  
  // Draw bars
  switch (pattern) {
    case 0:
      rainbowBars(band, barHeight);
      break;
    case 1:
      // No bars on this one
      break;
    case 2:
      purpleBars(band, barHeight);
      break;
    case 3:
      centerBars(band, barHeight);
      break;
    case 4:
      changingBars(band, barHeight);
      EVERY_N_MILLISECONDS(10) { colorTimer++; }
      break;
    case 5:
      createWaterfall(band);
      EVERY_N_MILLISECONDS(30) { moveWaterfall(); }
      break;
  }

  // Draw peaks
  switch (pattern) {
    case 0:
      whitePeak(band);
      break;
    case 1:
      outrunPeak(band);
      break;
    case 2:
      whitePeak(band);
      break;
    case 3:
      // No peaks
      break;
    case 4:
      // No peaks
      break;
    case 5:
      // No peaks
      break;
  }
}

void showIP(){
  char strIP[16] = "               ";
  IPAddress ip = WiFi.localIP();
  ip.toString().toCharArray(strIP, 16);
  Serial.println(strIP);
  ScrollingMsg.SetFont(MatriseFontData);
  ScrollingMsg.Init(&leds, leds.Width(), ScrollingMsg.FontHeight() + 1, 0, 0);
  ScrollingMsg.SetText((unsigned char *)strIP, sizeof(strIP) - 1);
  ScrollingMsg.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 0xff, 0xff, 0xff);
  ScrollingMsg.SetScrollDirection(SCROLL_LEFT);
  ScrollingMsg.SetFrameRate(160 / M_WIDTH);       // Faster for larger matrices

  while(ScrollingMsg.UpdateText() == 0) {
    FastLED.show();  
    delay(1);
  }
}

//////////// Patterns ////////////

void rainbowBars(uint8_t band, uint8_t barHeight) {
  int xStart = barWidth * band;
  for (int x = xStart; x < xStart + barWidth; x++) {
    for (int y = 0; y <= barHeight; y++) {
      leds(x,y) = CHSV((x / barWidth) * (255 / numBands), 255, 255);
    }
  }
}

void purpleBars(int band, int barHeight) {
  int xStart = barWidth * band;
  for (int x = xStart; x < xStart + barWidth; x++) {
    for (int y = 0; y < barHeight; y++) {
      leds(x,y) = ColorFromPalette(purplePal, y * (255 / barHeight));
    }
  }
}

void changingBars(int band, int barHeight) {
  int xStart = barWidth * band;
  for (int x = xStart; x < xStart + barWidth; x++) {
    for (int y = 0; y < barHeight; y++) {
      leds(x,y) = CHSV(y * (255 / M_HEIGHT) + colorTimer, 255, 255); 
    }
  }
}

void centerBars(int band, int barHeight) {
  int xStart = barWidth * band;
  for (int x = xStart; x < xStart + barWidth; x++) {
    if (barHeight % 2 == 0) barHeight--;
    int yStart = ((M_HEIGHT - barHeight) / 2 );
    for (int y = yStart; y <= (yStart + barHeight); y++) {
      int colorIndex = constrain((y - yStart) * (255 / barHeight), 0, 255);
      leds(x,y) = ColorFromPalette(heatPal, colorIndex);
    }
  }
}

void whitePeak(int band) {
  int xStart = barWidth * band;
  int peakHeight = peak[band];
  for (int x = xStart; x < xStart + barWidth; x++) {
    leds(x,peakHeight) = CRGB::White;
  }
}

void outrunPeak(int band) {
  int xStart = barWidth * band;
  int peakHeight = peak[band];
  for (int x = xStart; x < xStart + barWidth; x++) {
    leds(x,peakHeight) = ColorFromPalette(outrunPal, peakHeight * (255 / M_HEIGHT));
  }
}

void createWaterfall(int band) {
  int xStart = barWidth * band;
  // Draw bottom line
  for (int x = xStart; x < xStart + barWidth; x++) {
    leds(x,0) = CHSV(constrain(map(fftResult[band],0,254,160,0),0,160), 255, 255);
  }
}

void moveWaterfall() {
  // Move screen up starting at 2nd row from top
  for (int y = M_HEIGHT - 2; y >= 0; y--) {
    for (int x = 0; x < M_WIDTH; x++) {
      leds(x,y+1) = leds(x,y);
    }
  }
}
