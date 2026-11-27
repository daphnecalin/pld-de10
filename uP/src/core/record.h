#ifndef RECORD_H_
#define RECORD_H_

#include "packet.h"

constexpr int RING_BUFFER_SIZE = 512;

typedef struct
{
    uint64_t t_us;
    packet::Frame frame;
} Record;

typedef struct
{
    Record buffer[RING_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuffer;

void ringBufferInit(RingBuffer *rb);

bool ringBufferWrite(RingBuffer *rb, Record value);

bool ringBufferRead(RingBuffer *rb, Record *value);

int ringBufferCount(const RingBuffer *rb);

#endif