#define _XOPEN_SOURCE 700
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash_api.h"

#define RETURN_TYPE uint16_t

typedef struct
{
    unsigned int words[2];
    unsigned long state[5];
} u8p5_ctx;

void init(void *ctx)
{
    u8p5_ctx *context = (u8p5_ctx *)ctx;

    context->words[0] = 3;
    context->words[1] = 4;

    context->state[0] = 0;
    context->state[1] = 1;
    context->state[2] = 2;
    context->state[3] = 3;
    context->state[4] = 4;
}

void update(void *ctx,
            const unsigned char *data,
            size_t len)
{

    u8p5_ctx *context = (u8p5_ctx *)ctx;
    if (len > 0)
    {
        context->words[0] = data[0];
        context->words[1] = (uint8_t)len;
    }
}

void final(void *ctx,
           unsigned char *out)
{
    u8p5_ctx *context = (u8p5_ctx *)ctx;
    RETURN_TYPE r;
    context->words[0]++;
    r = rand() % 2;
    memcpy(out, &r, sizeof(RETURN_TYPE));
}

static const HashAPI api = {
    .name = "uint8_t + 5",

    .ctx_size = sizeof(u8p5_ctx),
    .out_size = sizeof(RETURN_TYPE),

    .init = init,
    .update = update,
    .final = final};

HashAPI hash_api(void)
{
    return api;
}