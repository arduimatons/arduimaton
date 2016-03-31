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
typedef void (*rf_handler)(RF24NetworkHeader&);
//typedef void (*inverval_handler)();

// class def
class Arduimaton
{
  public:
     //Inherit RF24Network Constructor & Descontructor
     Arduimaton(RF24Network &);
     ~Arduimaton();

  

    void setInfo(char[10], char[10], uint8_t);
    bool sendInfo();
    bool regHandler(rf_handler);

    size_t encodedPayloadLen(char*);

    void loop();

    unsigned long heartBeat();

    size_t getPayload(char*, char*, size_t, size_t);
    
    size_t genPayload(char*, char*, size_t);

  private:
    char* name;
    char* version;
    uint8_t type;
    
    RF24Network& _network;

    uint8_t handlerCount = 0;
    unsigned long now; 

    void createHash(char*, char*);

    void default_handler(RF24NetworkHeader&);

    void default_interval();
    unsigned long last_def_interval; 
     
    uint16_t node_id;
   
    rf_handler function1;
    rf_handler function2;


    //heart beat variables
    bool registered = false;
    unsigned long beat = 0;
    unsigned long last_beat = 0;

    void handle_HB(RF24NetworkHeader&);

    // blake2s object for hashing
    BLAKE2s blake2s;
};

#endif // __KOTO_H__
