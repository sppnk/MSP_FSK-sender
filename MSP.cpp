
#if defined(PROTOCOL_MSP)
#include "MSP.h"

#define MWCSERIALBUFFERSIZE 256
static uint8_t MSPserialBuffer[MWCSERIALBUFFERSIZE];
static uint8_t MSPreadIndex;
static uint8_t MSPreceiverIndex;
static uint8_t MSPdataSize;
static uint8_t MSPcmd;
static uint8_t MSPrcvChecksum;

static uint32_t modeMSPRequests;
static uint32_t pos_packets;
static uint32_t chk_error;
static uint32_t chk_ok;


int32_t      uav_lat = 12312313;                    // latitude
int32_t      uav_lon = 2342342;                    // longitude
uint8_t      uav_satellites_visible = 3;     // number of satelites
uint8_t      uav_fix_type = 0;               // GPS lock 0-1=no fix, 2=2D, 3=3D
int32_t      uav_alt = 300;                    // altitude (cm)
uint8_t      uav_rssi = 0;                   // radio RSSI (%)
int16_t      pMeterSum = 0;                    // altitude (cm)
uint8_t      MwVBat = 0;                   // radio RSSI (%)
int          uav_groundspeed = 0;            // ground speed

long lastpacketreceived;


uint8_t read8()  {
  return MSPserialBuffer[MSPreadIndex++];
}

uint16_t read16() {
  uint16_t t = read8();
  t |= (uint16_t)read8() << 8;
  return t;
}

uint32_t read32() {
  uint32_t t = read16();
  t |= (uint32_t)read16() << 16;
  return t;
}

void msp_read() {

  //Serial.print("msp_read ()");
  uint8_t c;

  static enum _serial_state {
    IDLE,
    HEADER_START,
    HEADER_M,
    HEADER_ARROW,
    HEADER_SIZE,
    HEADER_CMD
  }
  c_state = IDLE;

  while (modem.available()) {        // detect msp commands
    c = modem.read();
    //Serial.write(c);

    if (c_state == IDLE) {
      c_state = (c == '$') ? HEADER_START : IDLE;
    }
    else if (c_state == HEADER_START) {
      c_state = (c == 'M') ? HEADER_M : IDLE;
    }
    else if (c_state == HEADER_M) {
      c_state = (c == '>') ? HEADER_ARROW : IDLE;
    }
    else if (c_state == HEADER_ARROW) {
      if (c > MWCSERIALBUFFERSIZE)  {  // now we are expecting the payload size
        c_state = IDLE;
      }
      else {
        MSPdataSize = c;
        c_state = HEADER_SIZE;
        MSPrcvChecksum = c;
      }
    }
    else if (c_state == HEADER_SIZE) {
      c_state = HEADER_CMD;
      MSPcmd = c;
      MSPrcvChecksum ^= c;
      MSPreceiverIndex = 0;
    }
    else if (c_state == HEADER_CMD) {
      MSPrcvChecksum ^= c;
      if (MSPreceiverIndex == MSPdataSize) { // received checksum byte
        if (MSPrcvChecksum == 0) {
          lastpacketreceived = millis();
          msp_check();
          chk_ok++; //ok packet --debug--
        }

        c_state = IDLE;
      }

      else MSPserialBuffer[MSPreceiverIndex++] = c;
      //chk_error++; //error chk, packet dropped
    }
  }
}

// --------------------------------------------------------------------------------------
// Here are decoded received commands from MultiWii
void msp_check() {
  //Serial.print("MSPcmd() : "); Serial.println(MSPcmd);
  MSPreadIndex = 0;

  if (MSPcmd == MSP_IDENT)
  {
    // possible use later
    // MwVersion= read8();                             // MultiWii Firmware version
    // modeMSPRequests &=~ REQ_MSP_IDENT;
  }

  if (MSPcmd == MSP_STATUS)
  {
    //possible use later
  }
  if (MSPcmd == MSP_RAW_GPS)
  {
    pos_packets++;
    //Serial.println("MSP_RAWGPS");
    uav_fix_type = read8();
    uav_satellites_visible = read8();
    uav_lat = (int32_t)read32();
    uav_lon = (int32_t)read32();
#ifndef BARO_ALT
    uav_alt = (int32_t)(read16() * 100);
#endif

    uav_groundspeed = read16();
  }

  if (MSPcmd == MSP_COMP_GPS)
  {
    //    GPS_distanceToHome=read16();
    //    GPS_directionToHome=read16();
  }

  if (MSPcmd == MSP_ATTITUDE)
  {
    //  possible use later
    //    for(uint8_t i=0;i<2;i++)
    //      MwAngle[i] = read16();
    //    MwHeading = read16();
    //    read16();
  }

  if (MSPcmd == MSP_ALTITUDE)
  {
#ifdef BARO_ALT
    uav_alt = (int32_t)read32();
#endif
    //uav_vario = read16();
  }

  if (MSPcmd == MSP_ANALOG)
  {
    //Serial.println("MSP_ANALOG");
    // for later
    MwVBat = read8();
    pMeterSum = read16();
    uav_rssi = read16();
    //uav_rssi = map(MwRssi,0,100,0,100); // remap in %
  }
}


// End of decoded received commands from MultiWii
// --------------------------------------------------------------------------------------

// Request data to Multiwii ( unused yet )
void msp_request(char requestMSP)
{
  Serial.write('$');
  Serial.write('M');
  Serial.write('<');
  Serial.write((byte)0x00);//size is 0, as this is request
  Serial.write(requestMSP);//command
  Serial.write(requestMSP);//checksum is xor from size to data in this protocol
}

//########################################### TX OSD OUTPUT###############################################################
void setMspRequests() {
  modeMSPRequests =
    REQ_MSP_IDENT |
    REQ_MSP_STATUS |
    REQ_MSP_RAW_GPS |
    REQ_MSP_ANALOG |
    REQ_MSP_ALTITUDE;
}

//static uint8_t MSPtxChecksum;
//
//void serialize8(uint8_t a) {
//  SerialPort2.write(a);
//  MSPtxChecksum ^= a;
//}
//void serialize16(int16_t a) {
//  serialize8((a   ) & 0xFF);
//  serialize8((a>>8) & 0xFF);
//}
//void serialize32(uint32_t a) {
//  serialize8((a    ) & 0xFF);
//  serialize8((a>> 8) & 0xFF);
//  serialize8((a>>16) & 0xFF);
//  serialize8((a>>24) & 0xFF);
//}
//void headSerialResponse(uint8_t err, uint8_t s) {
//  serialize8('$');
//  serialize8('M');
//  serialize8(err ? '!' : '>');
//  MSPtxChecksum = 0; // start calculating a new checksum
//  serialize8(s);
//  serialize8(cmdMSP[CURRENTPORT]);
//}
//
//void headSerialReply(uint8_t s) {
//  headSerialResponse(0, s);
//}
//void inline headSerialError(uint8_t s) {
//  headSerialResponse(1, s);
//}
//
//void tailSerialReply() {
//  serialize8(MSPtxChecksum)s;
//}
//
//void serializeNames(PGM_P s) {
//  headSerialReply(strlen_P(s));
//  for (PGM_P c = s; pgm_read_byte(c); c++) {
//    serialize8(pgm_read_byte(c));
//  }
//}
//
//void  s_struct(uint8_t *cb,uint8_t siz) {
//  headSerialReply(siz);
//  while(siz--) serialize8(*cb++);
//}
//
//void s_struct_w(uint8_t *cb,uint8_t siz) {
// headSerialReply(0);
//  while(siz--) *cb++ = read8();
//}
//
//
//void MSPsendPacket(uint8_t MSPtxCmd) {
//  uint32_t tmp=0;
//
//  switch(MSPtxCmd) {
//
//
//  }
//}
//
//
//

#endif
