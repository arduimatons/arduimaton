#include <arduimaton.h>
#include "DHT.h"
// Example testing sketch for various DHT humidity/temperature sensors
// Written by ladyada, public domain



#define DHTPIN 2     // what digital pin we're connected to

// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

// Initialize DHT sensor.
// Note that older versions of this library took an optional third parameter to
// tweak the timings for faster processors.  This parameter is no longer needed
// as the current DHT reading algorithm adjusts itself to work on faster procs.
DHT dht(DHTPIN, DHTTYPE);


// initalize RF24 Radio
RF24 radio(9,10);
RF24Network network(radio);

// initalize Arduimaton class with radio, passed by reference.
Arduimaton arduimaton(network);

// function declarations
void tempInterval();
void sendJsonPL();

long last_sent_temp;


void setup() {
  Serial.begin(9600);
  arduimaton.setInfo("Den1", "0.0.1", 5);
  SPI.begin();            
  radio.begin();
  // can configure RF24 radio, channel, PAlevel etc
  radio.setChannel(115);
  radio.setPALevel(RF24_PA_MAX);
  network.begin(02);
  dht.begin();

 }

void loop() {
  // Wait a few seconds between measurements.
  arduimaton.loop();
 
 if((millis() - last_sent_temp) > S_INTERVAL){
    sendJsonPL();
    last_sent_temp = millis();
  }

}

// a function to construct a custom message payload
void sendJsonPL(){
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (!isnan(h) || !isnan(t)) {
   // Compute heat index in Celsius (isFahreheit = false)
    float hic = dht.computeHeatIndex(t, h, false);
    StaticJsonBuffer<80> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["t"] = t;
    root["i"] = hic;
    root["h"] =  h;
    // grab heartbeat from arduimaton object
    char json[80];
    size_t sz = root.printTo(json, sizeof(json)); 
    json[sz] = '\0'; 
    Serial.println(json);
    char encodedPL[100];
    // timestamp, encode and hash payload, moves to encodedPL buf as null terminated string 
    size_t plLen = arduimaton.genPayload(encodedPL, json);
    //create RF24 network header, destined for the master node, of NODE_MSG1 which does not expect an ACK
    RF24NetworkHeader header(00, NODE_MSG1);
    if(network.write(header, &encodedPL, strlen(encodedPL)))
    {
      Serial.println("sent encoded payload!");
      Serial.println(encodedPL);
      Serial.println(strlen(encodedPL));
    }
 }  
}

