#include "LinxNode.h"

void LinxNode::begin(const char lynx_name[], const char vers[], short lynx_type )
{ 
  name = lynx_name;
  version = vers;
  type = lynx_type;
   //_network.multicastRelay = true;
  // set node to multicast relay to children nodes by default

  // set multicast level to the type of node so all "MOTION" nodes can be address with a multicast to level MOTION
  // can have up to 6 levels which enables up to 6 unique types, 
  // nodes can be more than one type IE MOTION & TEMP , or SWITCH & TEMP but will only listen on one multicast pipe
  // might be able to be more than 2 types, but two seems reasonable...
 // _network.multicastLevel(this->types[0]);
}

bool LinxNode::sendInfo()
{   
    // measure size of json, create char to fit + null
    RF24NetworkHeader header(/*to node*/ 00, /*type*/ NODE_STATUS);
    size_t info_len = strlen(name) + strlen(version) + 4;
    char info_buff[info_len]; 
    sprintf(info_buff,"%s,%s,%i", name, version, type );
    #ifdef SERIAL_DEBUG
      Serial.println("Sending info to master!");
      Serial.println(info_buff);
    #endif
    char toSend[120]; //create large buffer to store entire encoded msg
    size_t plSize = encode(toSend, info_buff); // generate payload, returns size of generated payload
    return _network.write(header, &toSend, strlen(toSend));
}

long  LinxNode::currentBeat(){
    return network_beat + (millis() - last_network_beat)/1000;
}

void LinxNode::beat_heart()
{
  _network.update();
  while ( _network.available() )  {                      // check if we got something
    RF24NetworkHeader header;                            
    _network.peek(header); // peek at it
    if(header.type == BEAT) // if it is a beat, handle it.
    {
      char beat_payload[75];
      size_t beat_len = _network.read(header, beat_payload, sizeof(beat_payload));
      beat_payload[beat_len] = '\0';
      char raw_beat[10];
      size_t valid_msg_size = decode(raw_beat, beat_payload, strlen(beat_payload), false);
      if(valid_msg_size > 0)
      {
        last_network_beat = millis(); // so we can see if it is our first heartbeat.
        network_beat = atol(raw_beat);
        #ifdef SERIAL_DEBUG
          Serial.print("Got beat: ");Serial.println(network_beat);
        #endif
      }
      
    } 
    break;    
  }

    long now = millis();
    // if not registered but have got a beat;
    if(network_beat > 0 && !linx_registered) {
        sendInfo();
        // just make sure this doesnt go again...will trigger on S_INTERVAL.
        linx_registered = true; 
    }

    if((now - last_ping_msg > 120000) && linx_registered){
        sendInfo();
        last_ping_msg = now;  
     }
}

// destination, source
size_t LinxNode::createHash(char* hash, char* toHash )
{
  uint8_t hashedMsg[DIGEST_SIZE];
  blake2s.reset(SECRET_KEY, sizeof(SECRET_KEY), DIGEST_SIZE);
  blake2s.update(toHash, strlen(toHash)); //need to use strlen here to catch /0 and not go by size of buffer...
  blake2s.finalize(hashedMsg, DIGEST_SIZE);
  sprintf(hash, "%x", hashedMsg[0]);
  char hex_char[3];
  for(int i=1;i<DIGEST_SIZE; i++){
    sprintf(hex_char, "%x", hashedMsg[i]);
    strncat(hash, hex_char, strlen(hex_char)+1);
  }
  return strlen(hash);
}

// destination, source - returns actual bytes in payload.
size_t LinxNode::encode(char* output, char* to_encode_and_sign)
{
  // calculate encoded length of message
  size_t encodedLenMSG = base64_enc_len(strlen(to_encode_and_sign));
  // get heartbeat, make it a string
  char myHB[sizeof(long)*8+1];
  // long to a char*
  ltoa(currentBeat(), myHB, 10);
  // calculate length of string we are hashing
  size_t toHashLen = (10 + 1 + encodedLenMSG);
  // we know enough to calculate entire payload length
  size_t full_pl_len = (toHashLen + 1 + DIGEST_SIZE_HEX-1);
  // create temporary buffers to store parts of message we are going to encode and hash
  char encodedMSG[encodedLenMSG];
  char toHash[toHashLen+1];
  // encode heartbeat
  // null byte
  // move encoded hb to temp buffer
  strncpy(toHash,myHB, 10); 
  toHash[10] = '\0';
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
  //char encodedHASH[encodedLenHASH];
  // encode hash
  //base64_encode(encodedHASH, payloadHash, strlen(payloadHash));   
  // null byte
  //encodedHASH[encodedLenHASH] = '\0';
  // finish output buffer, include null terminator.
  strncat(output, payloadHash, DIGEST_SIZE_HEX+1); 
  #ifdef SERIAL_DEBUG
   Serial.println("generated payload: ");
   Serial.println(output);
  #endif
  // return str length of output buffer.
 return strlen(output);
}
// decode and test if payload is valid, returns >0 if the payload is good, will be on payloadData buff.
size_t LinxNode::decode(char* output, char* raw_pl, size_t raw_pl_len, bool check_beat)
{
  long msg_recieved_at = currentBeat(); // cool we got a message at?
  // buff to store message beat
  char msg_beat[11];
  //grab beat off message
  strncpy(msg_beat, raw_pl, 10); 
  // null terminate
  msg_beat[10] = '\0';
  long diff = atol(msg_beat) - msg_recieved_at;

  size_t raw_head_len = 10;
  // msg payload can change, scan string started at end of encoded heartbeat to determine length of encoded payload
  size_t raw_enc_body_len = strcspn(raw_pl+raw_head_len+1, "."); 
  // calculate length of hash based on full message length minus header body and periods
  size_t hash_len = raw_pl_len-(raw_head_len+raw_enc_body_len+2); 
  // the length of the message we need to hash is sum of head, body and a period
  size_t pl_to_hash_len = (raw_head_len + raw_enc_body_len + 1);
  // temp buf for hash test, we need to generate
  char pl_to_hash[pl_to_hash_len+1]; 
  // temp buf for hash test, that came with message
  char delivered_hash[hash_len+1];   
  // grab encoded header+payload, copy to enc_head_body buf
  strncpy(pl_to_hash, raw_pl, pl_to_hash_len); 
  pl_to_hash[pl_to_hash_len] = '\0'; 
  // grap encoded hash, copy to enc_hash buf      
  strncpy(delivered_hash,raw_pl+pl_to_hash_len+1, hash_len); 
  delivered_hash[hash_len] = '\0';
  // create hash of encoded header.payload
  char generated_hash[hash_len+1];
  createHash(generated_hash,pl_to_hash);
  // test if hashes match.
  if(strncmp(generated_hash,delivered_hash, hash_len) == 0){
    // calculate if this message was just delivered, or is some kinda replay
    #ifdef SERIAL_DEBUG
        Serial.print("MyBeat: "); Serial.println(msg_recieved_at); 
        Serial.print("MsgBeat: "); Serial.println(atol(msg_beat)); 
        Serial.print("HB Sway: "); Serial.println(diff); 
    #endif
    // will let a diff slide if we have not got a beat
     // calculate hops, to determine a reasonable delay
  
    uint8_t valid_diff = (_network.parent() > 0) ? 5 : 1;
    if(diff <= valid_diff || !check_beat){    
      // calculate length of decoded body
      size_t decodedBodyLEN = base64_dec_len(raw_pl+raw_head_len+1, raw_enc_body_len);
      // decode body, put it on output buf, return length of decoded message, we got a payload!
      base64_decode(output, raw_pl+raw_head_len+1, raw_enc_body_len);
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