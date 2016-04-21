#include <Automaton.h>
#include <Atm_button.h>
#include <Att_relay.h>
#include <Atm_RF24Network.h>
#include <LinxNode.h>

Att_button btn1,btn2;    // An Atm_button machine
Att_relay relay1;
Att_relay relay2; // Custom Atm_relay machine

// initalize RF24 Radio & network
RF24 radio(9, 10);
// initalize RF24Network Machine
Att_RF24Network att_net(radio);

LinxNode linx(att_net.network);


// message callback declaration
void msgcb();

// button callback declaration
void btn_change( int press, int idx );

// And finally a factory to house them in
TinyFactory factory; 

void setup() {
  Serial.begin( 9600 );
  
  
  // init relays: relay pin, led pin
  relay1.begin( 5, 7 );
  relay2.begin( 6, 8 );

  btn1.begin(3).onPress(btn_change, 1);
  btn2.begin(4).onPress(btn_change, 2);

  // register custom message handler
  att_net.onMsg(msgcb);
  // begin RF24Network within machine, pass nodes address
  att_net.begin(115,04);

  linx.begin("switch", "0.1.0", SWITCH);

  factory
  .add(att_net)
  .add(relay1)
  .add(relay2)
  .add(btn1)
  .add(btn2);
}

void loop() {
  factory.cycle();
  linx.beat_heart();
  // put your main code here, to run repeatedly:
}



// button callback
void btn_change( int press, int idx )
{
  if ( press ) {
      if(idx == 1)
      {
        Serial.println(relay1.state());
        relay1.setBlink(0,0).setBlink(0,1);
        relay1.trigger(relay1.EVT_TOGGLE);
        char pl[] = "test payload";
        char to_send[80];
        size_t msg_len = linx.encode(to_send, pl);
        att_net.send(to_send, msg_len, MSG_OUT_1);
      }

      if(idx == 2)
      {
      Serial.println(relay2.state());
      relay2.setBlink(0,0).setBlink(0,1);
      relay2.trigger(relay2.EVT_TOGGLE);
      char pl[] = "test payload";
      char to_send[80];
      size_t msg_len = linx.encode(to_send, pl);
      att_net.send(to_send, msg_len, MSG_OUT_2);
      }

  }
}


// message callback definition
void msgcb()
{
  RF24NetworkHeader header;
  att_net.network.peek(header);
  switch(header.type)
  {
    case MSG_IN_1: 
      {
        char green_msg[] = "GREEN";
        char red_msg[] = "RED";
        char payload[65];
        size_t full_len = att_net.network.read(header, &payload, sizeof(payload));
        payload[full_len] = '\0';
        size_t msg_len = linx.decode(payload, payload, full_len);
        if(msg_len > 0){
          if(strncmp(payload, "GREEN", 5) == 0){
            relay1.setBlink(10,0).setBlink(6,1);
            relay1.trigger(relay1.EVT_TOGGLE);
          } else if(strncmp(payload, "RED", 3) == 0){
            relay2.setBlink(10,0).setBlink(6,1);
            relay2.trigger(relay2.EVT_TOGGLE);
          }
        }
      } 

    default:
      {
        att_net.network.read(header,0,0);
        #ifdef SERIAL_DEBUG
          char err_msg[42];
          sprintf(err_msg, "*** WARNING *** Unknown message type %c", header.type);
        #endif  
        break;
      }
  }
}