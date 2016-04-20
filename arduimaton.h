#ifndef __ARDUIMATON_H__
#define __ARDUIMATON_H__

#include "arduimaton_config.h"
#include <Arduino.h>
#include <Base64.h>
#include <RF24Network.h>
#include <RF24.h> 
#include <ArduinoJson.h>
#include <SPI.h>
#include <Crypto.h>
#include <BLAKE2s.h>

#define INVALID_PAYLOAD_MSG "Invalid Payload"

// enums to for message types 0-5
enum NO_ACK_MSG_TYPES { NODE_STATUS = 0, NODE_MSG1, NODE_MSG2, NODE_MSG3, NODE_MSG4, BEAT };
// 65-67
enum ACK_MSG_TYPES { FUNCTION_1 = 65, FUNCTION_2, FUNCTION_3};
enum NODE_TYPES { TEMP = 1, MOTION, SWITCH, RGB};

// function pointers
typedef void (*rf_handler)(RF24NetworkHeader&);
//typedef void (*inverval_handler)();

// class def
class Arduimaton
{
  public:
    //Inherit RF24Network Constructor & Descontructor
    Arduimaton(RF24Network &);
    ~Arduimaton();
  
    void setInfo(char[11], char[9]);
    void setType(uint8_t t);
   
    bool regHandler(rf_handler);

    size_t encodedPayloadLen(char*);

    void loop();

    long heartBeat();

    size_t getPayload(char*, char*, size_t);
    
    size_t genPayload(char*, char*);

    void begin(uint16_t node_id);


  private:
    bool sendInfo();
    char* name;
    char* version;

    // can have a maximum of 2 types, between 1-6
    uint8_t types[2] = {0,0};
    
    RF24Network& _network;

    uint8_t handlerCount = 0;
   
    void createHash(char*, char*);

    void default_handler(RF24NetworkHeader&);

    void default_interval();
    long last_def_interval; 
      
    rf_handler function1;
    rf_handler function2;


    // heart beat variables
    // alive msg; 
    long sent_alive_msg;
    bool registered = false;
    long beat = 0;
    long last_beat = 0;


    void handle_HB(RF24NetworkHeader&);

    // blake2s object for hashing
    BLAKE2s blake2s;
};

#endif // __KOTO_H__
