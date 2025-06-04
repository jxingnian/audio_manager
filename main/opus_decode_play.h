#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void opus_decode_play_start(void);
void opus_decode_play_stop(void);
int opus_decode_play_write(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif