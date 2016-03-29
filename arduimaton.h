#ifndef __ARDUIMATON_H__
#define __ARDUIMATON_H__

#include "arduimaton_config.h"
#include <Arduino.h>

#include <Base64.h>
#include <RF24.h> 
#include <RF24Network.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Crypto.h>
#include <BLAKE2s.h>


// enums to for message types 0-5
enum NO_ACK_MSG_TYPES { NODE_STATUS = 0, NODE_MSG1, NODE_MSG2, NODE_MSG3, NODE_MSG4, BEAT };
// 65-67
enum ACK_MSG_TYPES { FUNCTION_1 = 65, FUNCTION_2, FUNCTION_3};

// function pointers
typedef void (*network_handler)(RF24NetworkHeader&);
typedef void (*inverval_handler)();

// class def
class Arduimaton : public RF24Network
{

  public:
    using RF24Network::RF24Network;  //Inherit RF24Network Constructor & Descontructor

    // button states, current,last for debounce
    bool button_a[2] = {LOW,LOW};
    unsigned long a_debounce = 0;  // the last time the output pin was toggled
    byte button_a_p;
    // button states, current,last for debounce
    bool button_b[2] = {LOW,LOW};
    unsigned long b_debounce = 0;  // the last time the output pin was toggled
    byte button_b_p;
    // button states, current,last for debounce
    bool button_c[2] = {LOW,LOW};
    unsigned long c_debounce = 0;  // the last time the output pin was toggled
    byte button_c_p;
    // button states, current,last for debounce
    bool button_d[2] = {LOW,LOW};
    unsigned long d_debounce = 0;  // the last time the output pin was toggled
    byte button_d_p;
    unsigned int debounceDelay = 80;    // the debounce time; increase if the output flickers
    // led or relay states...
    bool led_a = LOW;
    byte led_a_p;
    bool led_b = LOW;
    byte led_b_p;
    bool led_c = LOW;
    byte led_c_pin;
    bool led_d = LOW;
    byte led_d_pin;
    // relay or sensor pins
    byte rs_a_p;
    byte rs_b_p;
    byte rs_c_p;
    byte rs_d_p;

    void setInfo(char[10], char[10], uint8_t);
    bool sendInfo();
    bool regHandler(network_handler);
    bool regInterval(inverval_handler);

    size_t encodedPayloadLen(char*);

    void loop();

    unsigned long heartBeat();

    size_t getPayload(char*, char*, size_t, size_t);
    
    size_t genPayload(char*, char*, size_t);

  private:
    char* name;
    char* version;
    uint8_t type;

    uint8_t handlerCount = 0;
    uint8_t intervalCount = 0;

    unsigned long main_interval; 

    unsigned long now; 

    void createHash(char*, char*);

    void default_handler(RF24NetworkHeader&);
    void default_interval();
     
    uint16_t node_id;

    network_handler function1;
    network_handler function2;
    network_handler function3;

    inverval_handler interval1;
    unsigned int interval1_timer = DEFAULT_INTERVAL;
    unsigned long lt_interval1; 
     
    inverval_handler interval2;
    unsigned int interval2_timer;
    unsigned long lt_interval2; 
     
    inverval_handler interval3;
    unsigned int interval3_timer;
    unsigned long lt_interval3;  

    //heart beat variables
    bool registered = false;
    unsigned long beat = 0;
    unsigned long last_beat = 0;

    void handle_HB(RF24NetworkHeader&);

    // blake2s object for hashing
    BLAKE2s blake2s;
};

#endif // __KOTO_H__
