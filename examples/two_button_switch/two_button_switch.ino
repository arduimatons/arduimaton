#include <arduimaton.h>

// initalize RF24 Radio
RF24 radio(9,10);

// initalize Arduimaton class with radio, passed by reference.
Arduimaton switches(radio);

void buttonInterval();
void networkHandler(RF24NetworkHeader&);

void sendJsonPL(char*, int );

void setup() {
  Serial.begin(9600);
  // doesnt really matter at this point, but sends an array with heartbeat container this info base
  // str:name, str:version, int:type
  switches.setInfo("switches", "0.0.1", 15);
  
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
  
  // configure arduimaton pins used in intervals and network handlers.
  switches.button_a_p = 4;
  switches.button_b_p = 5;
  switches.led_a_p = 7;
  switches.led_b_p = 6;
  switches.rs_a_p = 3;
  switches.rs_b_p = 8;

  // setup pins
  pinMode(switches.button_a_p, INPUT);
  pinMode(switches.button_b_p, INPUT);
  
  pinMode(switches.led_a_p, OUTPUT);
  pinMode(switches.led_b_p, OUTPUT);
  
  pinMode(switches.rs_a_p, OUTPUT);
  pinMode(switches.rs_b_p, OUTPUT);

  // set initial LED state
  digitalWrite(switches.led_a_p, switches.led_a);
  digitalWrite(switches.led_b_p, switches.led_b);

  // set initial RELAY state, oposite of LED
  digitalWrite(switches.rs_a_p, !switches.led_a);
  digitalWrite(switches.rs_b_p, !switches.led_b);

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
    char encodedPL[90];
    // fill encoded payload buffer with a generated payload
    size_t plLen = switches.genPayload(encodedPL, json, 90 );
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
      switches.led_b = !switches.led_b;
      digitalWrite(switches.led_b_p, switches.led_b);
      digitalWrite(switches.rs_b_p, !switches.led_b);  // should be the 'same' as LED, in the case of relays ON is LOW, OFF is HIGH         
    } else {
       // Serial.println("got other payload!");
        Serial.println(dataPL);
       switches.led_a = !switches.led_a;
      digitalWrite(switches.led_a_p, switches.led_a);
      digitalWrite(switches.rs_a_p, !switches.led_a);  // should be the 'same' as LED, in the case of relays ON is LOW, OFF is HIGH 
    }
   } else {
    Serial.println("Invalid payload");
    }

}


// button handling interval example
void buttonInterval()
{
  int readingG = digitalRead(switches.button_a_p);
  int readingR = digitalRead(switches.button_b_p);

    // If the switch changed, due to noise or pressing:
  if (readingR !=  switches.button_a[1]) {
    // reset red debouncing timer
    switches.a_debounce = millis();
  } else if(readingG != switches.button_b[1] ) {
    // reset green debouncing timer
    switches.b_debounce = millis();
  }

  if ((millis() - switches.a_debounce) > switches.debounceDelay || (millis() -  switches.b_debounce) > switches.debounceDelay ) {
    // whatever the reading is at, it's been there for longer
    // than the debounce delay, so take it as the actual current state:
    // if the button state has changed:
    if (readingG != switches.button_a[0]) {
      switches.button_a[0] = readingG;
      // only toggle the LED if the new button state is HIGH
      if (switches.button_a[0] == HIGH) {
        // toggle the Green LED & Relay from button press
        Serial.println("green button pressed!");
        switches.led_a = !switches.led_a;
        digitalWrite(switches.led_a_p, switches.led_a);
        digitalWrite(switches.rs_a_p, !switches.led_a);  // should be the 'same' as LED, in the case of relays ON is LOW, OFF is HIGH
        // send an encoded payload back to base
        sendJsonPL("green", (int)switches.led_a);
       }
    }

    if( readingR != switches.button_b[0]){
      switches.button_b[0] = readingR;
      if (switches.button_b[0] == HIGH) {
        // toggle the Green LED & Relay from button press :
        switches.led_b = !switches.led_b;
        char redPL[] = "red button pressed";
        digitalWrite(switches.led_b_p, switches.led_b);
        digitalWrite(switches.rs_b_p, !switches.led_b);
         // send an encoded payload back to base
         sendJsonPL("red", (int)switches.led_b);
      }
    }
  }

  switches.button_a[1] = readingR;
  switches.button_b[1] = readingG;

}
