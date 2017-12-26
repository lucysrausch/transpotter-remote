#include <Wire.h>
#include <RFM69.h>
#include <SPI.h>
#include "ESP8266TrueRandom.h"
#include "SSD1306.h"
// Include the UI lib
#include "OLEDDisplayUi.h"

// Include custom images
#include "images.h"

#define FREQUENCY      RF69_868MHZ
#define KEY            "SuperSecretKey"     //super secret key
#define TIMEOUT        1000                //timeout we transmit, after that we expect to be known dead
#define TRANSMITPERIOD 300                //enough time to be heard two times within timeout
#define LED            2

SSD1306  display(0x3c, 4, 1);


typedef struct {
  uint16_t      nodeId;  //store this nodeId
  uint16_t      timeout; //timeout in ms when to stop
  float         battery; //battery voltage in volts
  uint8_t       command;
} Payload;
Payload theData;

typedef struct {
  uint16_t      nodeId; //store this nodeId
  uint8_t       networkId = 123; //store this networkId
} Config;
Config netConf;

typedef struct {
  uint16_t      nodeId;   //store this nodeId
  unsigned long lastseen; //time in ms since last ping
  uint16_t      timeout; //timeout in ms when to stop
  float         battery; //battery voltage in volts
} Node;

Node knownNodes[64];      //up the 64 nodes
uint8_t numberNodes = 0;  //number of known nodes
boolean newNode = false;
boolean emergencyOff = false;


OLEDDisplayUi ui     ( &display );

void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0, 0, "Nodes: " + String(numberNodes));

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128, 0, String("Battery:") + String(knownNodes[0].battery) + String("V"));
}

uint8_t emergencyFrame = 0;
void drawFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y, uint16_t currentFrame) {
  // Demonstrates the 3 included default sizes. The fonts come from SSD1306Fonts.h file
  // Besides the default fonts there will be a program to convert TrueType fonts into this format

  if (emergencyOff) {
    /*if (emergencyFrame != currentFrame) {
      emergencyOff = false;
    }*/
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_16);
    display->drawString(0 + x, 10 + y, "Node ID: " + String(knownNodes[currentFrame].nodeId));
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(64 + x, 38 + y, "Node Lost!");
    //emergencyOff = false;
  } else {
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_16);

    if (newNode && currentFrame == numberNodes) {
      display->drawString(0 + x, 10 + y, "New Node: " + String(knownNodes[currentFrame].nodeId));
      newNode = false;
    } else if (currentFrame == 0) {
      display->drawString(0 + x, 10 + y, "My ID: " + String(knownNodes[currentFrame].nodeId));
    } else {
      display->drawString(0 + x, 10 + y, "Node ID: " + String(knownNodes[currentFrame].nodeId));
    }

    display->setFont(ArialMT_Plain_10);
    display->drawString(0 + x, 28 + y, "Battery Voltage: " + String(knownNodes[currentFrame].battery));
    display->drawString(0 + x, 40 + y, "Last  seen: " + String(millis() - knownNodes[currentFrame].lastseen) + "ms");
  }
}




// This array keeps function pointers to all frames
// frames are the single views that slide in
FrameCallback frames[64] = {drawFrame};

// Overlays are statically drawn on top of a frame eg. a clock
OverlayCallback overlays[] = { msOverlay };
int overlaysCount = 1;

RFM69 radio(RF69_SPI_CS, 5);
bool promiscuousMode = false; //set to 'true' to sniff all packets on the same network

long lastPeriod = -1;

void setup() {
  pinMode(LED, OUTPUT);
  ui.setTargetFPS(30);
  delay(10);
  netConf.nodeId = (ESP8266TrueRandom.randomByte() << 8) | (ESP8266TrueRandom.randomByte() << 1); //leave out 0, as this is emergency off
  radio.initialize(FREQUENCY,255,netConf.networkId); //broadcast
  radio.setHighPower(); //uncomment only for RFM69HW!
  //radio.encrypt(KEY);
  //radio.promiscuous(promiscuousMode);

  // Customize the active and inactive symbol
  ui.setActiveSymbol(activeSymbol);
  ui.setInactiveSymbol(inactiveSymbol);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  // Add frames
  ui.setFrames(frames, numberNodes + 1);

  // Add overlays
  ui.setOverlays(overlays, overlaysCount);

  ui.setTimePerFrame(2000);
  ui.setTimePerTransition(200);

  ui.disableAutoTransition();

  knownNodes[0].nodeId = netConf.nodeId; // we know ourselfes
  knownNodes[0].timeout = TIMEOUT; // we know ourselfes
  knownNodes[0].lastseen = millis(); // we know ourselfes




  // Initialising the UI will init the display too.
  ui.init();

  display.flipScreenVertically();

  digitalWrite(LED, HIGH);
  pinMode(3, INPUT_PULLUP);
}

uint8_t nodeCounter = 0;

void loop() {
  nodeCounter++;
  if (nodeCounter > numberNodes) {
    nodeCounter = 0;
  }
  if(digitalRead(3) == 0) {
    while(digitalRead(3) == 0) {
        delay(100);
    }

    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.clear();
    display.drawString(64, 8, "Stopped!");
    display.drawString(64, 30, "Press again");
    display.drawString(64, 48, "to resume");
    display.display();

    while(digitalRead(3) == 1) {
        delay(100);
    }
    ESP.restart();
    delay(1000);
    ESP.reset();
  }

  if (millis() - knownNodes[nodeCounter].lastseen > knownNodes[nodeCounter].timeout) {
    emergencyOff = true;
    ui.setTimePerFrame(10000);
    ui.switchToFrame(nodeCounter);
    emergencyFrame = nodeCounter;
  }

  if (radio.receiveDone())
  {
    if (radio.DATALEN == sizeof(Payload))
    {
      theData = *(Payload*)radio.DATA; //assume radio.DATA actually contains our struct and not something else

      boolean newNode = true;
      for (uint8_t i = 0; i <= numberNodes; i++) { //check if node is already known
        if (knownNodes[i].nodeId == theData.nodeId) {
          newNode = false;
          knownNodes[i].lastseen = millis();
          knownNodes[i].timeout = theData.timeout;
          knownNodes[i].battery = theData.battery;
          break;
        }
      }

      if (newNode && millis() > 4000) {
        numberNodes++;
        knownNodes[numberNodes].nodeId = theData.nodeId;
        knownNodes[numberNodes].lastseen = millis();
        knownNodes[numberNodes].timeout = theData.timeout + 2000;
        knownNodes[numberNodes].battery = theData.battery;

        frames[numberNodes] = drawFrame;
        ui.setFrames(frames, numberNodes + 1);
        ui.setTimePerFrame(3000);
        ui.transitionToFrame(numberNodes);
        ui.enableAutoTransition();
        newNode = true;
      }
    }
  } else {
    ui.update();
  }

  int currPeriod = millis()/TRANSMITPERIOD;
  if (currPeriod != lastPeriod)
  {
    knownNodes[0].lastseen = millis(); // we know ourselfes
    knownNodes[0].battery = analogRead(A0) * 0.00543; // we know ourselfes
    theData.nodeId = netConf.nodeId;
    theData.timeout = TIMEOUT;
    theData.battery = 39.23;
    if (millis() < 1000) {
      theData.command = 111;
    } else {
      theData.command = 0;
    }

    radio.receiveDone();
    if (!emergencyOff) {
     radio.send(255, (const void*)(&theData), sizeof(theData), false);
    }
    lastPeriod=currPeriod;
  }
}
