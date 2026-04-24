#ifndef CALLBACK_H
#define CALLBACK_H

#include "services/RotorService.h"

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
  const RotorService* rotor;  // used to guard against writing during POLL_SENT
};

// Sends a G5500 LLP command only when the serial bus is free.
// If called during POLL_SENT, the command is silently dropped to avoid
// corrupting the in-flight status response frame.
void sendG5500Command(void* context) {
    G5500CommandContext* ctx = (G5500CommandContext*)context;
    if (ctx->rotor && !ctx->rotor->isSerialFree()) return;
    DataPack output;
    output.addData(ctx->header, ctx->command);
    output.write(*ctx->serial);
}

#endif
