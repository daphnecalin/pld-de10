#include "record.h"

void ringBufferInit(RingBuffer *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

bool ringBufferWrite(RingBuffer *rb, uint16_t value)
{
    uint16_t nextHead = (rb->head + 1) % RING_BUFFER_SIZE;

    // check if the buffer is full
    if (nextHead == rb->tail)
    {
        return false;
    }

    rb->buffer[rb->head] = value;
    rb->head = nextHead;
    return true;
}

bool ringBufferRead(RingBuffer *rb, uint16_t *value)
{
    // check if the buffer is empty
    if (rb->head == rb->tail)
    {
        return false
    }

    *value = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
    return true;
}

int ringBufferCount(const RingBuffer *rb)
{
    if (rb->head >= rb->tail)
    {
        return rb->head - rb->tail;
    }
    return RING_BUFFER_SIZE - (rb->tail - rb->head);
}

int ringBuffer