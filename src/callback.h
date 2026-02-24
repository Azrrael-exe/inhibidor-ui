#ifndef CALLBACK_H
#define CALLBACK_H

#include "llp.h"

#define AZIMUTH_HEADER 0xAA
#define AZIMUTH_STOP 0xA0
#define AZIMUTH_FORWARD 0xA1
#define AZIMUTH_BACKWARD 0xA2

#define ELEVATION_HEADER 0xBB
#define ELEVATION_STOP 0xB0
#define ELEVATION_FORWARD 0xB1
#define ELEVATION_BACKWARD 0xB2

struct G5500CommandContext {
  HardwareSerial* serial;
  uint8_t header;
  uint8_t command;
};

void sendG5500Command(void* context) {
    DataPack output;
    G5500CommandContext* ctx = (G5500CommandContext*)context;
    output.addData(ctx->header, ctx->command);
    output.write(*ctx->serial);
}

#endif
