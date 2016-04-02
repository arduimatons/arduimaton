// include arduimaton header file
#include <arduimaton.h>

// initalize RF24 Radio
RF24 radio(9,10);
RF24Network network(radio);

// initalize Arduimaton class with radio, passed by reference.
Arduimaton motionPIR(network);


byte pir = 2;
int pirState = LOW; 

// function declarations
void motionInterval();
void sendJsonPL(char*, int);

void setup() {
  Serial.begin(9600);
  // set unique node name, verison and type
  motionPIR.setInfo("PIR_1", "0.0.1", 10);

  // register an interval to send a message when motion starts and stops
 // motionPIR.regInterval(motionInterval);
  
  // start spi, radio
  SPI.begin();            
  radio.begin();
  
  // can configure RF24 radio, channel, PAlevel etc
  radio.setChannel(115);
  radio.setPALevel(RF24_PA_MAX);
  
  // Begin RF24Network passing RF24Network NodeID!
  // does not have to relate to node name,version or type 
  network.begin(03);

  // set some pin variables
  // pir sensor is on arduimaton button_a_pin 2 - 

  pinMode(pir, INPUT);

}

void loop() {
  // run arduimaton loop
  motionPIR.loop();

  motionInterval();

}

// function definitions

// interval functions return nothing and are executed at configurable intervals
void motionInterval(){ 
  int val = digitalRead(pir);
  if ( val == HIGH) {            // check if the input is HIGH
    if (pirState == LOW) {
      // we have just turned on
      Serial.println("Motion detected!");
      sendJsonPL("motion", 1);
      // We only want to print on the output change, not state
      pirState = HIGH;
    }
  } else {
    if (pirState == HIGH){
      // we have just turned of
      Serial.println("Motion ended!");
      sendJsonPL("motion", 0);
      // We only want to print on the output change, not state
      pirState = LOW;
    }
  }

}

// a function to construct a custom message payload
void sendJsonPL(char* key, int val ){
    StaticJsonBuffer<50> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root[key] = val;
    // grab heartbeat from arduimaton object
    char json[50];
    size_t sz = root.printTo(json, sizeof(json)); 
    // make sure string sent to genPayload is null terminated
    json[sz] = '\0'; 
    char encodedPL[60];
    // timestamp, encode and hash payload, moves to encodedPL buf as null terminated string 
    size_t plLen = motionPIR.genPayload(encodedPL, json);
    //create RF24 network header, destined for the master node, of NODE_MSG1 which does not expect an ACK
    RF24NetworkHeader header(00, NODE_MSG1);
    if(network.write(header, &encodedPL, strlen(encodedPL)))
    {
      Serial.println("sent encoded payload!");
      Serial.println(encodedPL);
      Serial.println(strlen(encodedPL));
    }
}