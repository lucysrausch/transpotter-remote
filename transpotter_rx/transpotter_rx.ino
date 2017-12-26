#include <Wire.h>
#include <RFM69.h>
#include <SPI.h>
#include "ESP8266TrueRandom.h"
#include <Adafruit_NeoPixel.h>

#define FREQUENCY      RF69_868MHZ
#define KEY            "SuperSecretKey" //super secret key
#define TIMEOUT        1000                //timeout we transmit, after that we expect to be known dead
#define TRANSMITPERIOD 300                //enough time to be heard two times within timeout
#define LED            2

Adafruit_NeoPixel strip = Adafruit_NeoPixel(23, 4, NEO_GRB + NEO_KHZ800);
RFM69 radio(RF69_SPI_CS, 5);
bool promiscuousMode = false; //set to 'true' to sniff all packets on the same network

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

long lastPeriod = -1;
boolean ledState;

void setup() {
  netConf.nodeId = (ESP8266TrueRandom.randomByte() << 8) | (ESP8266TrueRandom.randomByte() << 1); //leave out 0, as this is emergency off
  radio.initialize(FREQUENCY,255,netConf.networkId); //broadcast
  //radio.setHighPower(); //uncomment only for RFM69HW!
  //radio.encrypt(KEY);
  //radio.promiscuous(promiscuousMode);

  //Wire.pins(1, 3); // Select TXD as SDA and RXD as SCL
  //Wire.begin(42);
  //Wire.onRequest(requestEvent); // register event

  //Serial.begin(115200);
  digitalWrite(1, LOW);
  pinMode(1, INPUT);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  strip.begin();
  strip.show();

  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 255));
  }
  strip.show();
}


uint8_t nodeCounter = 0;
boolean emergencyOff = false;

void loop() {
  while(digitalRead(1) && !emergencyOff) {
    for(uint16_t i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(255, 0, 0));
    }
    strip.show();
    delay(100);
  }

  nodeCounter++;
  if (nodeCounter > numberNodes) {
    nodeCounter = 0;
  }

  if (((millis() - knownNodes[nodeCounter].lastseen) > knownNodes[nodeCounter].timeout) && nodeCounter > 0 && millis() > 3000) {
    emergencyOff = true;
    pinMode(1, OUTPUT);
    digitalWrite(1, HIGH);
    //digitalWrite(LED, LOW);
    for(uint16_t i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(255, 0, 0));
    }
    strip.show();
  }

  if (radio.receiveDone())
  {
    if (radio.DATALEN == sizeof(Payload))
    {
      theData = *(Payload*)radio.DATA; //assume radio.DATA actually contains our struct and not something else
      boolean newNode = true;
      ledState = !ledState;
      digitalWrite(LED, ledState);

      for (uint8_t i = 0; i <= numberNodes; i++) { //check if node is already known
        if (knownNodes[i].nodeId == theData.nodeId) {

          if (theData.command == 111 && millis() > 3000) {
            for(uint16_t i=0; i<strip.numPixels(); i++) {
              strip.setPixelColor(i, strip.Color(0, 0, 255));
            }
            strip.show();
            delay(random(1000,7000));
            ESP.restart();
            delay(1000);
            ESP.reset();
          }
          newNode = false;
          knownNodes[i].lastseen = millis();
          knownNodes[i].timeout = theData.timeout;
          knownNodes[i].battery = theData.battery;
          break;
        }
      }

      if (newNode && (millis() > random(2500, 3500))) {
        numberNodes++;
        knownNodes[numberNodes].nodeId = theData.nodeId;
        knownNodes[numberNodes].lastseen = millis();
        knownNodes[numberNodes].timeout = theData.timeout + 2000;
        knownNodes[numberNodes].battery = theData.battery;

        for(uint16_t i=0; i<strip.numPixels(); i++) {
          strip.setPixelColor(i, strip.Color(0, 255, 0));
        }
        strip.show();
      }
    }
  }

  int currPeriod = millis()/TRANSMITPERIOD;
  if (currPeriod != lastPeriod && millis() > random(1000, 2000))
  {
    theData.nodeId = netConf.nodeId;
    theData.timeout = TIMEOUT;
    theData.battery = 39.23;
    theData.command = 0;

    radio.send(255, (const void*)(&theData), sizeof(theData), false);

    lastPeriod=currPeriod;
    strip.show();
  }
}

void requestEvent() {
  byte statusbyte = 42;
  if (emergencyOff) {
    statusbyte = 0;
  }
  Wire.write(statusbyte); // respond with message of 6 bytes
  // as expected by master
}
