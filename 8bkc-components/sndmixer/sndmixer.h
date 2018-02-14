#pragma once
#include <stdint.h>

typedef struct {
	//Initialize the sound source Returns size of data returned per call of fill_buffer.
	int (*init_source)(const void *data_start, const void *data_end, int req_sample_rate, void **ctx);
	//Get the actual sample rate the source returns data at
	int (*get_sample_rate)(void *ctx);
	//Decode a bufferful of data. Returns 0 when file ended or something went wrong. Returns amount of bytes in buffer (normally what init_source returned) otherwise.
	int (*fill_buffer)(void *ctx, int8_t *buffer);
	//Destroy source, free resources
	void (*deinit_source)(void *ctx);
} sndmixer_source_t;

int sndmixer_init(int no_channels, int samplerate);
int sndmixer_queue_wav(const void *wav_start, const void *wav_end, int evictable);
int sndmixer_queue_mod(const void *mod_start, const void *mod_end);
void sndmixer_set_loop(int id, int loop);
void sndmixer_set_volume(int id, int volume);
void sndmixer_play(int id);
void sndmixer_pause(int id);
void sndmixer_stop(int id);
void sndmixer_pause_all();
void sndmixer_resume_all();
