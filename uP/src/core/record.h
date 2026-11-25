#pragma once

#include "packet.h"

constexpr int RING_BUFFER_SIZE = 512;

typedef struct
{
    uint64_t t_us;
    Frame frame;
} Record;

typedef struct
{
    Record buffer[RING_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuffer;

void ringBufferInit(RingBuffer *rb);

bool ringBufferWrite(RingBuffer *rb, uint16_t value);

bool ringBufferRead(RingBuffer *rb, uint16_t *value);

int ringBufferCount(const RingBuffer *rb);