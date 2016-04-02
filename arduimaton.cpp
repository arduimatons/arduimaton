#include "arduimaton.h"

void Arduimaton::default_handler(RF24NetworkHeader& header)
{
  char payload[100];
  size_t plSize = _network.read(header, &payload, sizeof(payload));
  payload[plSize] = '\0';
  char validPayload[80];
  size_t valid_pl_len = this->getPayload(validPayload, payload, plSize);
  if(valid_pl_len > 0){
      validPayload[valid_pl_len] = '\0';
      Serial.print("Got Message Type: "); Serial.println(header.type);
      Serial.print("From node: ");        Serial.println(header.from_node);
      Serial.print("w/ Payload: ");       Serial.println(validPayload);
  } else {
    Serial.print("Invalid Payload"); 
  }
}

void Arduimaton::setInfo(char* n, char* v, uint8_t t)
{
  this->name = n;
  this->version = v;
  this->type = t;
}

Arduimaton::Arduimaton(RF24Network& network): _network(network)
{
  function1 = NULL;
  function2 = NULL;
}

Arduimaton::~Arduimaton()
{
  
}

bool Arduimaton::sendInfo()
{   
    StaticJsonBuffer<75> jsonBuffer;
    JsonArray& array = jsonBuffer.createArray();
    array.add(this->name);
    array.add(this->version);
    array.add(this->type);
    char jsonData[75]; //create buffer to store json
    size_t sz = array.printTo(jsonData, sizeof(jsonData)); 
    jsonData[sz]  = '\0'; // add null terminator
    RF24NetworkHeader header(/*to node*/ 00, /*type*/ NODE_STATUS);
    #ifdef SERIAL_DEBUG
      Serial.println("Sending info to master!");
      Serial.println(jsonData);
    #endif
    char toSend[120]; //create large buffer to store entire encoded msg
    size_t plSize = this->genPayload(toSend, jsonData); // generate payload, returns size of generated payload
    return _network.write(header, &toSend, strlen(toSend));
}

// could probably store these in a vector? 
bool Arduimaton::regHandler(rf_handler _handler)
{
  if(handlerCount < 2){
    switch(handlerCount){
      case 0: this->function1 = _handler; break;
      case 1: this->function2 = _handler; break;
    }
    handlerCount+=1;
    return true;
  } else {
    return false;
  }
}

void Arduimaton::handle_HB(RF24NetworkHeader& header)
{
  char payload[64]; // little bigger than it needs to be.
  size_t full_len = _network.read(header, &payload, sizeof(payload));
  payload[full_len] = '\0';
  char dataPL[11]; // 10 + null
  size_t validBeat = getPayload(dataPL, payload, full_len); 
  if(validBeat > 0){
    this->last_beat = millis(); // so we can see if it is our first heartbeat.
    // set beat as long from char*
    this->beat = atol(dataPL);
    #ifdef SERIAL_DEBUG
      Serial.print("Got a heartbeat: "); Serial.println(this->beat);
    #endif
  }
;}

long  Arduimaton::heartBeat(){
  return this->beat + (millis() - this->last_beat)/1000;
}

void Arduimaton::loop()
{
  this->now = millis(); // keep track of 'now'
  // keep network updated
  _network.update();
  while ( _network.available() )  {                      // Is there anything ready for us?
    RF24NetworkHeader header;                            // If so, take a look at it
    _network.peek(header);
      switch (header.type){                              // Dispatch the message to the correct handler.
        case FUNCTION_1: 
            (this->function1 == NULL) ? this->default_handler(header): this->function1(header);
            break;
        case FUNCTION_2: 
            (this->function2 == NULL) ? this->default_handler(header): this->function2(header);
            break;
        case BEAT:
            handle_HB(header);  // handle heartbeat messages
            break;
        default:  
              #ifdef SERIAL_DEBUG
                Serial.print("*** WARNING *** Unknown message type: "); Serial.println(header.type);
              #endif
              _network.read(header,0,0);
              break;
      };
    }

    // if not registered by have got a beat;
    if(!this->registered && this->beat > 0) {
      this->sendInfo();
      this->registered = true; // just make sure this doesnt go again...will trigger on M_INTERVAL.
    }

    if((this->now - sent_alive_msg) > M_INTERVAL){
        this->sendInfo();
        this->sent_alive_msg = now;  
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
size_t Arduimaton::genPayload(char* output, char* to_encode_and_sign)
{
  // calculate encoded length of message
  size_t encodedLenMSG = base64_enc_len(strlen(to_encode_and_sign));
  // get heartbeat, make it a string
  char myHB[sizeof(long)*8+1];
  // long to a char*
  ltoa(this->heartBeat(), myHB, 10);
  // calculate encoded length of heartbeat
  size_t encodedLenHead = base64_enc_len(strlen(myHB));
  // calculate encoded length of hash, without null terminator
  size_t encodedLenHASH = base64_enc_len(DIGEST_SIZE_HEX-1);
  // calculate length of string we are hashing
  size_t toHashLen = (encodedLenHead + 1 + encodedLenMSG);
  // we know enough to calculate entire payload length
  size_t full_pl_len = (toHashLen + 1 + encodedLenHASH);
  // create temporary buffers to store parts of message we are going to encode and hash
  char encodedHead[encodedLenHead];
  char encodedMSG[encodedLenMSG];
  char toHash[toHashLen+1];
  // encode heartbeat
  base64_encode(encodedHead, myHB, strlen(myHB));
  // null byte
  encodedHead[encodedLenHead] = '\0'; 
  // move encoded hb to temp buffer
  strncpy(toHash,encodedHead, encodedLenHead+1);  
  // encode message
  base64_encode(encodedMSG, to_encode_and_sign, strlen(to_encode_and_sign)); 
  // null byte
  encodedMSG[encodedLenMSG] = '\0';      
  // concat period
  strncat(toHash, ".", 2);              
  // concat encoded message, grab null terminator.
  strncat(toHash, encodedMSG,encodedLenMSG+1); 
  // start moving payload to output buffer
  strncpy(output, toHash, strlen(toHash)+1); 
  // period before hash
  strncat(output, ".", 2);
  // create buffer for hash of payload         
  char payloadHash[DIGEST_SIZE_HEX];
  // generate hash of payload    
  createHash(payloadHash, toHash);
  // create buffer for encoded hash
  char encodedHASH[encodedLenHASH];
  // encode hash
  base64_encode(encodedHASH, payloadHash, strlen(payloadHash));   
  // null byte
  encodedHASH[encodedLenHASH] = '\0';
  // finish output buffer, include null terminator.
  strncat(output, encodedHASH, encodedLenHASH+1); 
  #ifdef SERIAL_DEBUG
   Serial.println("generated payload: ");
   Serial.println(output);
  #endif
  // return str length of output buffer.
 return strlen(output);
}

// decode and test if payload is valid, returns >0 if the payload is good, will be on payloadData buff.
size_t Arduimaton::getPayload(char* payloadData, char* rawPayload,  size_t full_pl_len)
{
  // pretty sure this is always going to be 16, encoded length of heartbeat.
  size_t raw_enc_head_len = 16;
  // msg payload can change, scan string started at end of encoded heartbeat to determine length of encoded payload
  size_t raw_enc_body_len = strcspn(rawPayload+raw_enc_head_len+1, "."); 
  // calculate length of hash based on full message length minus header body and periods
  size_t raw_enc_hash_len = full_pl_len-(raw_enc_head_len+raw_enc_body_len+2); 
  // the length of the message we need to hash is sum of head, body and a period
  size_t hashedMsg_len = (raw_enc_head_len + raw_enc_body_len + 1);
  // temp buf for hash test, we need to generate
  char enc_head_body[hashedMsg_len+1]; 
  // temp buf for hash test, that came with message
  char enc_hash[raw_enc_hash_len+1];   
  // grab encoded header+payload, copy to enc_head_body buf
  strncpy(enc_head_body, rawPayload, hashedMsg_len); 
  enc_head_body[hashedMsg_len] = '\0'; 
  // grap encoded hash, copy to enc_hash buf      
  strncpy(enc_hash,rawPayload+hashedMsg_len+1,raw_enc_hash_len); 
  enc_hash[raw_enc_hash_len] = '\0';
  // get length of decoded hash
  size_t decodedHashLEN = base64_dec_len(enc_hash, raw_enc_hash_len);
  base64_decode(enc_hash, enc_hash, raw_enc_hash_len);
  // create hash of encoded header.payload
  char my_hash[decodedHashLEN+1];
  this->createHash(my_hash,enc_head_body);
  // test if hashes match.
  if(strncmp(my_hash,enc_hash, decodedHashLEN) == 0){
    long msg_recieved_at = this->heartBeat(); // cool we got a message at?
    // buff to store message beat
    char msg_beat[11];
    // decode beat
    base64_decode(msg_beat, rawPayload, 16);
    // null terminate
    msg_beat[10] = '\0';
    // calculate if this message was just delivered, or is some kinda replay
    long diff = atol(msg_beat) - msg_recieved_at;
    #ifdef SERIAL_DEBUG
        Serial.print("MyBeat: "); Serial.println(msg_recieved_at); 
        Serial.print("MsgBeat: "); Serial.println(atol(msg_beat)); 
        Serial.print("HB Sway: "); Serial.println(diff); 
    #endif
    // will let a diff slide if we have not got a beat
    if(diff <= 1 || this->beat == 0){    
      // calculate length of decoded body
      size_t decodedBodyLEN = base64_dec_len(rawPayload+raw_enc_head_len+1, raw_enc_body_len);
      // decode body, put it on output buf, return length of decoded message, we got a payload!
      base64_decode(payloadData, rawPayload+raw_enc_head_len+1, raw_enc_body_len);
      return decodedBodyLEN;
    } else {
      #ifdef SERIAL_DEBUG
        Serial.println(INVALID_PAYLOAD_MSG); //bad beat
      #endif
      return 0;
    } 
  } else {
    #ifdef SERIAL_DEBUG
      Serial.println(INVALID_PAYLOAD_MSG); // bad mac
    #endif
    return 0;
  }
}