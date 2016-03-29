#include "arduimaton.h"

void Arduimaton::default_handler(RF24NetworkHeader& header)
{
  char payload[100];
  size_t plSize = this->read(header, &payload, sizeof(payload));
  payload[plSize] = '\0';
  size_t eMsgLen = encodedPayloadLen(payload);
  char validPayload[eMsgLen];
  size_t valid_pl_len = this->getPayload(validPayload, payload, eMsgLen, plSize);
  if(valid_pl_len > 0){
      validPayload[valid_pl_len] = '\0';
      Serial.print("Got Message Type: "); Serial.println(header.type);
      Serial.print("From node: ");        Serial.println(header.from_node);
      Serial.print("w/ Payload: ");       Serial.println(validPayload);
  } else {
    Serial.print("Invalid Payload"); 
  }
}

void Arduimaton::default_interval()
{
  if ( this->now - this->lt_interval1 >= (this->interval1_timer*1000) ){
    this->lt_interval1 = this->now;
    Serial.println("default interval triggered"); 
  }
}

void Arduimaton::setInfo(char* n, char* v, uint8_t t)
{
  this->name = n;
  this->version = v;
  this->type = t;
}

bool Arduimaton::sendInfo()
{   
    StaticJsonBuffer<45> jsonBuffer;
    JsonArray& array = jsonBuffer.createArray();
    array.add(this->name);
    array.add(this->version);
    array.add(this->type);
    array.add(this->heartBeat());
    char jsonData[40]; //create buffer to store json
    size_t sz = array.printTo(jsonData, sizeof(jsonData)); 
    jsonData[sz]  = '\0'; // add null terminator
    RF24NetworkHeader header(/*to node*/ 00, /*type*/ NODE_STATUS /*State*/);
    char toSend[75]; //create larger buffer to store entire encoded msg
    size_t plSize = this->genPayload(toSend, jsonData, 75);
    toSend[plSize] = '\0'; // add null terminator
    return this->write(header, &toSend, strlen(toSend));
}

// could probably store these in a vector? 
bool Arduimaton::regHandler(network_handler net_handler)
{
  if(handlerCount < 3){
    switch(handlerCount){
      case 0: this->function1 = net_handler; break;
      case 1: this->function2 = net_handler; break;
      case 2: this->function3 = net_handler; break;
    }
    handlerCount+=1;
    return true;
  } else {
    return false;
  }
}

bool Arduimaton::regInterval(inverval_handler int_handler)
{
  if(intervalCount < 3){
    switch(intervalCount){
      case 0: this->interval1 = int_handler; break;
      case 1: this->interval1 = int_handler; break;
      case 2: this->interval1 = int_handler; break;
    }
    intervalCount+=1;
    return true;
  } else {
    return false;
  }
}


void Arduimaton::handle_HB(RF24NetworkHeader& header)
{
  unsigned long incomingBeat = last_beat; // hold onto last beat incase it is an invalid beat
  this->last_beat = millis();                   // set last beat asap
  char payload[50];
  size_t full_len = this->read(header, &payload, sizeof(payload));
  payload[full_len] = '\0';
  size_t eMsgLen = encodedPayloadLen(payload);
 //size_t decodedLen = base64_dec_len(forMsgB64, toDecode.length());
  char dataPL[eMsgLen]; // will be smaller than encoded length...
  size_t validBeat = getPayload(dataPL, payload, eMsgLen, full_len);  // should always be 10.
  if(validBeat > 0){
    dataPL[validBeat] = '\0';
    // string to unsigned long
    this->beat = strtoul(dataPL, NULL, 10);
    #ifdef SERIAL_DEBUG
      Serial.print("Got a heartbeat!: "); Serial.println(this->beat);
    #endif
  } else {
    #ifdef SERIAL_DEBUG
      Serial.print("invalid beat: "); Serial.println(dataPL);
    #endif
    this->last_beat = incomingBeat;
  }
;}

unsigned long  Arduimaton::heartBeat(){
  return this->beat + (millis() - this->last_beat)/1000;
}

void Arduimaton::loop()
{
  this->now = millis(); // keep track of 'now'
  // keep network updated
  this->update();
  while ( this -> available() )  {                      // Is there anything ready for us?
    RF24NetworkHeader header;                            // If so, take a look at it
    RF24Network::peek(header);
      switch (header.type){                              // Dispatch the message to the correct handler.
        case FUNCTION_1: 
            (this->function1 == NULL) ? this->default_handler(header): this->function1(header);
            break;
        case FUNCTION_2: 
            (this->function2 == NULL) ? this->default_handler(header): this->function2(header);
            break;
        case FUNCTION_3: 
            (this->function3 == NULL) ? this->default_handler(header): this->function3(header);
            break;
        case BEAT:
            handle_HB(header);  // handle heartbeat messages
            break;
        default:  
              #ifdef SERIAL_DEBUG
                Serial.print("*** WARNING *** Unknown message type: "); Serial.println(header.type);
              #endif
              RF24Network::read(header,0,0);
              break;
      };
    }

   (this->interval1 == NULL) ? this->default_interval(): this->interval1();
   (this->interval2 == NULL) ? this->default_interval(): this->interval2();
   (this->interval3 == NULL) ? this->default_interval(): this->interval3();


    // do this once
  (!this->registered && (this->beat > 0) ) ? this->registered = this->sendInfo(): this->registered=this->registered;

  // keep doing it on an interval
  if ( (this->now - this->main_interval) >= (DEFAULT_INTERVAL*1000))
  {
    this->main_interval = this->now;
    this->sendInfo();
  }

}

// destination, source
void Arduimaton::createHash(char* hash, char* toHash )
{
  uint8_t hashedMsg[DIGEST_SIZE];
  blake2s.reset(SECRET_KEY, sizeof(SECRET_KEY), DIGEST_SIZE);
  blake2s.update(toHash, strlen(toHash)); //need to use strlen here to catch /0 and not go by size of buffer...
  blake2s.finalize(hashedMsg, DIGEST_SIZE);
  String hS; // temporary string to make hex version to store as char[DIGEST_SIZE*2]
  for(int i=0;i<DIGEST_SIZE; i++){ hS += String(+hashedMsg[i], HEX);}
  hS.toCharArray(hash, hS.length()+1);
  hash[hS.length()] = '\0';
}

// destination, source - returns actual bytes in payload.
size_t Arduimaton::genPayload(char* output, char* to_encode_and_sign, size_t output_buf_size)
{
  size_t inputLen = strlen(to_encode_and_sign);

  char inCopy[inputLen+1];
  strncpy(inCopy, to_encode_and_sign, inputLen);
  inCopy[inputLen] = '\0';
  
  #ifdef SERIAL_DEBUG
   Serial.println("payload to sign and encode: ");
   Serial.println(inCopy);
  #endif

  char payloadHash[DIGEST_SIZE_HEX];
  createHash(payloadHash, inCopy);

  #ifdef SERIAL_DEBUG
   Serial.println("Hashed payload: ");
   Serial.println(payloadHash);
  #endif

  size_t encodedLenMSG = base64_enc_len(inputLen);
  char encodedMSG[encodedLenMSG];
  base64_encode(encodedMSG, to_encode_and_sign, inputLen);       // encode payload
  encodedMSG[encodedLenMSG]= '\0';
  strncpy (output, encodedMSG ,encodedLenMSG+1);                 // move encoded MSG to output
  strncat (output, ".", 2);                                      // add period
 
  size_t encodedLenHASH = base64_enc_len(strlen(payloadHash));
  char encodedHASH[encodedLenHASH];
  base64_encode(encodedHASH, payloadHash, strlen(payloadHash));   // encode hash
  encodedHASH[encodedLenHASH] = '\0';
  strncat (output, encodedHASH, encodedLenHASH);                  // add the hash

  #ifdef SERIAL_DEBUG
   Serial.println("generated payload: ");
   Serial.println(output);
  #endif

  return strlen(output);
}

// just to find out how long an encoded payload is, wherever the . is in the full payload.
size_t Arduimaton::encodedPayloadLen(char* incoming_payload)
{
  return strcspn(incoming_payload, ".");  
}

// decode and test if payload is valid, returns >0 if the payload is good, will be on payloadData buff.
size_t Arduimaton::getPayload(char* payloadData, char* rawPayload, size_t encoded_msg_len, size_t full_pl_len)
{
  // invalid payload no period
  if(encoded_msg_len >= full_pl_len) return false;                 
  // create buffers to hold payload and hash
  // delivered as base64 encoded strings delimited by a '.'
  // MTQ1OTAxMzA0Nw.ODI3MWNhM2QyNzYyNGE4Yw
  char encodedMSG[encoded_msg_len];
  char encodedHSH[(full_pl_len-encoded_msg_len)+1];

  // move the payload of the message up to the peroid into encodedMSG
  memmove (encodedMSG, rawPayload, encoded_msg_len+1 );
  encodedMSG[encoded_msg_len] = '\0';

  // move payload hash everything after the period into encodedHSH
  memmove (encodedHSH, (rawPayload+encoded_msg_len)+1, (full_pl_len-encoded_msg_len)+1 );
  encodedHSH[full_pl_len-encoded_msg_len] = '\0';

  // calculate decoded lengths of base64 encoded payloads
  size_t decodedHashLEN = base64_dec_len(encodedHSH, strlen(encodedHSH));
  size_t decodedMSGLEN = base64_dec_len(encodedMSG, strlen(encodedMSG));

  //decode payloads
  base64_decode(encodedHSH, encodedHSH,  strlen(encodedHSH));
  encodedHSH[(DIGEST_SIZE*2)] = '\0';
  
  #ifdef SERIAL_DEBUG
   Serial.print("decoded incoming hash: "); Serial.println(encodedHSH);
  #endif

  base64_decode(encodedMSG, encodedMSG,  strlen(encodedMSG));
  encodedMSG[decodedMSGLEN] = '\0';

  #ifdef SERIAL_DEBUG
   Serial.print("decoded incoming message: "); Serial.println(encodedMSG);
  #endif

  // create new buffer to store generated hash
  char generatedHash[DIGEST_SIZE_HEX];  // hash string will always be 2x size of digest bytes + null
  createHash(generatedHash, encodedMSG);
  
  // compare generated vs delivered hash, if valid copy the valid payload into the supplied payloadData buffer
  if(strncmp(generatedHash,encodedHSH, strlen(encodedHSH)) == 0){
    strncpy(payloadData, encodedMSG, strlen(encodedMSG) );
    return strlen(encodedMSG);
  } else {
    return 0;
  }
}

