#include <arduimaton.h>

// initalize RF24 Radio
RF24 radio(9,10);
RF24Network network(radio);
// initalize Arduimaton class with radio, passed by reference.
Arduimaton switches(network);

void buttonInterval();
void networkHandler(RF24NetworkHeader&);
void sendJsonPL(char*, int );


const int greenB = 4;  // the pin numbelsr of the pushbutton
const int redB = 5;    // the pin number of the red pushbutton

const int redL = 6;    // the pin number of the red LED
const int greenL = 7;  // the pin number of the LED

const int relayG = 3;  // the pin number of the green relay
const int relayR = 2;  // the number of the Red relay

// Variables will change:
int GledState = LOW;         // the current state of the output pin
int GbuttonState;             // the current reading from the input pin
int GlastButtonState = LOW;   // the previous reading from the input pin

// Variables will change:
int RledState = LOW;         // the current state of the output pin
int RbuttonState;             // the current reading from the input pin
int RlastButtonState = LOW;   // the previous reading from the input pin

// the following variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
unsigned long RlastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long GlastDebounceTime = 0;  // the last time the output pin was toggled
unsigned int debounceDelay = 80;    // the debounce time; increase if the output flickers


void setup() {
  Serial.begin(9600);
  // doesnt really matter at this point, but sends an array with heartbeat container this info base
  // str:name, str:version, int:type
  switches.setInfo("switches", "0.0.3", 15);
  
  // pass a function pointer through to the register handler function to register a network handler
  // the first handler will catch the first of the ACK_MSG_TYPES
  // first registered handler will catch msg type 65, the second 66, the third 67
  // depending on message payload will loggle and LED and corresponding relay
  switches.regHandler(networkHandler);
  
  // intervals run within the network update loop to react to environmental stimuli
  // in this case watches two buttons and will send a message if either is pressed
  // buttons also control the state of a relay
  switches.regInterval(buttonInterval);

  SPI.begin();            
  radio.begin();
  
  // can configure RF24 radio or network here
  radio.setChannel(115);
  radio.setPALevel(RF24_PA_MAX);
  
  // Begin RF24Network
  switches.begin(02);


  // setup pins
  pinMode(greenB, INPUT);
  pinMode(redB, INPUT);
  
  pinMode(redL, OUTPUT);
  pinMode(greenL, OUTPUT);
  
  pinMode(relayG, OUTPUT);
  pinMode(relayR, OUTPUT);

  // set initial LED state
  digitalWrite(redL,  RledState);
  digitalWrite(greenL, GledState);

  // set initial RELAY state, oposite of LED
  digitalWrite(relayG, !GledState);
  digitalWrite(relayR, !RledState);

}


// start arduimaton loop.
void loop() {
  switches.loop();
}


// simple json payload with a single key and integer value
void sendJsonPL(char* key, int val ){
    StaticJsonBuffer<60> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root[key] = val;
    // grab heartbeat from arduimaton object
    root["hb"] = switches.heartBeat();
    char json[50];
    size_t sz = root.printTo(json, sizeof(json)); 
    json[sz] = '\0'; 
    char encodedPL[110];
    // fill encoded payload buffer with a generated payload
    size_t plLen = switches.genPayload(encodedPL, json, 110 );
    encodedPL[plLen] = '\0';
    //create RF24 network header, destined for the master node, of NODE_MSG1 which does not expect an ACK
    RF24NetworkHeader header(00, NODE_MSG1);
    if(switches.write(header, &encodedPL, strlen(encodedPL)))
    {
      Serial.println("sent encoded payload!");
     // Serial.println(encodedPL);
    }
}

// handler function to be passed to register handler arduimaton method
// can register multiple handlers that can support 3 different message types [65,66,67]
// or use one type and act based on payload.
void networkHandler(RF24NetworkHeader& header)
{
  char payload[120];
  size_t plSize = switches.read(header, &payload, sizeof(payload));
  payload[plSize] = '\0';
  //Serial.println(payload);
  size_t eMsgLen = switches.encodedPayloadLen(payload);
  char dataPL[eMsgLen]; // will be smaller than encoded length...
  size_t goodPayload = switches.getPayload(dataPL, payload, eMsgLen, plSize);
   if(goodPayload > 0){
    dataPL[goodPayload] = '\0';
    //Serial.println(dataPL);
    if(strncmp(dataPL, "Red", 3) == 0){
      //Serial.println("got red payload!");
      RledState = !RledState;
      digitalWrite(redL, RledState);
      digitalWrite(relayR, !RledState);  // should be the 'same' as LED, in the case of relays ON is LOW, OFF is HIGH         
    } else {
       // Serial.println("got other payload!");
        Serial.println(dataPL);
       GledState = !GledState;
      digitalWrite(greenL, GledState);
      digitalWrite(relayR, !GledState);  // should be the 'same' as LED, in the case of relays ON is LOW, OFF is HIGH 
    }
   } else {
    Serial.println("Invalid payload");
    }

}


// button handling interval example
void buttonInterval()
{
  int readingG = digitalRead(greenB);
  int readingR = digitalRead(redB);

    // If the switch changed, due to noise or pressing:
  if (readingR !=  RlastButtonState) {
    // reset red debouncing timer
    RlastDebounceTime = millis();
  } else if(readingG != GlastButtonState ) {
    // reset green debouncing timer
    GlastDebounceTime = millis();
  }

  if ((millis() - GlastDebounceTime) > debounceDelay || (millis() -  RlastDebounceTime) > debounceDelay ) {
    // whatever the reading is at, it's been there for longer
    // than the debounce delay, so take it as the actual current state:
    // if the button state has changed:
    if (readingG != GbuttonState) {
      GbuttonState = readingG;
      // only toggle the LED if the new button state is HIGH
      if (GbuttonState == HIGH) {
        // toggle the Green LED & Relay from button press
        Serial.println("green button pressed!");
        GledState = !GledState;
        digitalWrite(greenL, GledState);
        digitalWrite(relayG, !GledState);  // should be the 'same' as LED, in the case of relays ON is LOW, OFF is HIGH
        // send an encoded payload back to base
        sendJsonPL("green", (int)GledState);
       }
    }

    if( readingR != RbuttonState){
      RbuttonState = readingR;
      if (RbuttonState == HIGH) {
        // toggle the Green LED & Relay from button press :
        RledState = !RledState;
        char redPL[] = "red button pressed";
        digitalWrite(redL, RledState);
        digitalWrite(relayR, !RledState);
         // send an encoded payload back to base
         sendJsonPL("red", (int)RledState);
      }
    }
  }

  RlastButtonState = readingR;
  GlastButtonState = readingG;

}