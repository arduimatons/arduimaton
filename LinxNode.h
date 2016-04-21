#ifndef __LINXNODE_H__
#define __LINXNODE_H__

#include "LinxNode_config.h"
#include <Arduino.h>
#include <Base64.h>
#include <RF24Network.h>
#include <Crypto.h>
#include <BLAKE2s.h>

#define VS_INTERVAL 15000   // 10 seconds
#define S_INTERVAL  45000   // 45 seconds
#define M_INTERVAL  180000  // 3 minutes
#define L_INTERVAL  360000  // 6 minutes
#define VL_INTERVAL 900000  // 15 minutes

// enums to for message types 0-5
enum { NODE_STATUS = 0, MSG_OUT_1, MSG_OUT_2, MSG_OUT_3, MSG_OUT_4, BEAT } NO_ACK_MSG_TYPES;
// 65-67
enum { MSG_IN_1 = 65, MSG_IN_2, MSG_IN_3, MSG_IN_4, MSG_IN_5} ACK_MSG_TYPES;
enum { TEMP = 1, MOTION, SWITCH, RGB} LYNX_TYPES;

#ifdef SERIAL_DEBUG
    #define INVALID_PAYLOAD_MSG "Invalid Payload"
#endif

class LinxNode
{
    public:
        LinxNode(RF24Network &net): _network(net) {};
        //~LinxNode();
        void begin(const char lynx_name[], const char vers[], short lynx_type );
        void beat_heart();
        size_t decode(char*, char*, size_t, bool check_beat=true);
        size_t encode(char*, char*);

    private:
        const char * name;
        const char * version;
        short type;
        long currentBeat();
        bool sendInfo();
        size_t createHash(char*, char*);
        void syncBeat(long net_beat);
        bool linx_registered = false;
        long network_beat = 0;
        long last_network_beat = 0;
        long last_ping_msg;
        RF24Network& _network;

        BLAKE2s blake2s;
};

#endif // __LINXNODE_H__
