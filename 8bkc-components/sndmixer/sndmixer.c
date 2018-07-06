#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/portmacro.h"

#include "8bkc-hal.h"
#include "sdkconfig.h"

#include "snd_source_wav.h"
#include "snd_source_mod.h"

#define CHFL_EVICTABLE (1<<0)
#define CHFL_PAUSED (1<<1)
#define CHFL_LOOP (1<<2)

typedef enum {
	CMD_QUEUE_WAV	=	1,
	CMD_QUEUE_MOD,
	CMD_LOOP,
	CMD_VOLUME,
	CMD_PLAY,
	CMD_PAUSE,
	CMD_STOP,
	CMD_PAUSE_ALL,
	CMD_RESUME_ALL
} sndmixer_cmd_ins_t;

typedef struct {
	sndmixer_cmd_ins_t cmd;
	int id;
	union {
		struct {
			const void *queue_file_start;
			const void *queue_file_end;
			int flags;
		};
		struct {
			int param;
		};
	};
} sndmixer_cmd_t;

typedef struct {
	int id;
	const sndmixer_source_t *source; //or NULL if channel unused
	void *src_ctx;
	int volume; //0-256
	int flags;
	int8_t *buffer;
	int chunksz;
	uint32_t dds_rate; //Rate; 16.16 fixed
	uint32_t dds_acc; //DDS accumulator, 16.16 fixed
} sndmixer_channel_t;

static sndmixer_channel_t *channel;
static int no_channels;
static int samplerate;
static volatile uint32_t curr_id=0;
static QueueHandle_t cmd_queue;

//Grabs a new ID by atomically increasing curr_id and returning its value. This is called outside of the audio playing thread, hence the atomicity.
static uint32_t new_id() {
	uint32_t old_id, new_id;
	do {
		old_id=curr_id;
		new_id=old_id+1;
		//compares curr_id with old_id, sets to new_id if same, returns old val in new_id
		uxPortCompareSet(&curr_id, old_id, &new_id);
	} while (new_id!=old_id);
	return old_id+1;
}

static void clean_up_channel(int ch) {
	if (channel[ch].source) {
		channel[ch].source->deinit_source(channel[ch].src_ctx);
		channel[ch].source=NULL;
	}
	free(channel[ch].buffer);
	channel[ch].buffer=NULL;
	channel[ch].flags=0;
	printf("Sndmixer: %d: cleaning up done\n", channel[ch].id); 
	channel[ch].id=0;
}

static int find_free_channel() {
	for (int x=0; x<no_channels; x++) {
		if (channel[x].source==NULL) return x;
	}
	//No free channels. Maybe one is evictable?
	for (int x=0; x<no_channels; x++) {
		if (channel[x].flags & CHFL_EVICTABLE) {
			clean_up_channel(x);
			return x;
		}
	}
	return -1; //nothing found :/
}

static int init_source(int ch, const sndmixer_source_t *srcfns, const void *data_start, const void *data_end) {
	int chunksz=srcfns->init_source(data_start, data_end, samplerate, &channel[ch].src_ctx);
	if (chunksz<=0) return 0; //failed
	channel[ch].source=srcfns;
	channel[ch].volume=256;
	channel[ch].buffer=malloc(chunksz);
	if (!channel[ch].buffer) {
		clean_up_channel(ch);
		return 0;
	}
	channel[ch].chunksz=chunksz;
	int real_rate=srcfns->get_sample_rate(channel[ch].src_ctx);
	channel[ch].dds_rate=(real_rate<<16)/samplerate;
	channel[ch].dds_acc=chunksz<<16; //to force the main thread to get new data
	return 1;
}

static void handle_cmd(sndmixer_cmd_t *cmd) {
	if (cmd->cmd==CMD_QUEUE_WAV || cmd->cmd==CMD_QUEUE_MOD) {
		int ch=find_free_channel();
		if (ch<0) return; //no free channels
		int r=0;
		printf("Sndmixer: %d: initing source\n", cmd->id); 
		if (cmd->cmd==CMD_QUEUE_WAV) {
			r=init_source(ch, &sndmixer_source_wav, cmd->queue_file_start, cmd->queue_file_end);
		} else if (cmd->cmd==CMD_QUEUE_MOD) {
			r=init_source(ch, &sndmixer_source_mod, cmd->queue_file_start, cmd->queue_file_end);
		}
		if (!r) {
			printf("Sndmixer: Failed to start decoder for id %d\n", cmd->id);
			return; //fail
		}
		channel[ch].id=cmd->id; //success; set ID
		channel[ch].flags=cmd->flags;
	} else if (cmd->cmd==CMD_PAUSE_ALL) {
		for (int x=0; x<no_channels; x++) channel[x].flags|=CHFL_PAUSED;
	} else if (cmd->cmd==CMD_RESUME_ALL) {
		for (int x=0; x<no_channels; x++) channel[x].flags&=~CHFL_PAUSED;
	} else {
		//Rest are all commands that act on a certain ID. Look up if we have a channel with that ID first.
		int ch=-1;
		for (int x=0; x<no_channels; x++) {
			if (channel[x].id==cmd->id) {
				ch=x;
				break;
			}
		}
		if (ch==-1) return; //not playing/queued; can't do any of the following commands.
		if (cmd->cmd==CMD_LOOP) {
			if (cmd->param) channel[ch].flags|=CHFL_LOOP; else channel[ch].flags&=~CHFL_LOOP;
		} else if (cmd->cmd==CMD_VOLUME) {
			channel[ch].volume=cmd->param;
		} else if (cmd->cmd==CMD_PLAY) {
			channel[ch].flags&=~CHFL_PAUSED;
		} else if (cmd->cmd==CMD_PAUSE) {
			channel[ch].flags|=CHFL_PAUSED;
		} else if (cmd->cmd==CMD_STOP) {
			printf("Sndmixer: %d: cleaning up source because of ext request\n", cmd->id); 
			clean_up_channel(ch);
		}
	}
}

#define CHUNK_SIZE 64

//Sound mixer main loop.
static void sndmixer_task(void *arg) {
	uint8_t mixbuf[CHUNK_SIZE];
	printf("Sndmixer task up.\n");
	while(1) {
		//Handle any commands that are sent to us.
		sndmixer_cmd_t cmd;
		while(xQueueReceive(cmd_queue, &cmd, 0) == pdTRUE) {
			handle_cmd(&cmd);
		}

		//Assemble CHUNK_SIZE worth of samples and dump it into the I2S subsystem.
		for (int i=0; i<CHUNK_SIZE; i++) {
			int s=0; //current sample value, multiplied by 256 (because of multiplies by channel volume)
			for (int ch=0; ch<no_channels; ch++) {
				if (channel[ch].source) {
					//Channel is active.
					channel[ch].dds_acc+=channel[ch].dds_rate; //select next sample
					//dds_acc>>16 now gives us which sample to get from the buffer.
					if ((channel[ch].dds_acc>>16)>=channel[ch].chunksz) {
						//That value is outside the channels chunk buffer. Refill that first.
						int r=channel[ch].source->fill_buffer(channel[ch].src_ctx, channel[ch].buffer);
						if (r==0) {
							//Source is done.
							printf("Sndmixer: %d: cleaning up source because of EOF\n", channel[ch].id); 
							clean_up_channel(ch);
							continue;
						}
						channel[ch].dds_acc-=(channel[ch].chunksz<<16); //reset dds acc; we have parsed chunksize samples.
						channel[ch].chunksz=r; //save new chunksize
					}
					//Multiply by volume, add to cumulative sample
					s+=channel[ch].buffer[channel[ch].dds_acc>>16]*channel[ch].volume;
				}
			}
			//Bring back to -128-127. Volume did *256, channels did *no_channels.
			s=(s/no_channels)>>8;
			mixbuf[i]=s+128; //because samples are signed, mix_buf is unsigned
		}
		kchal_sound_push(mixbuf, CHUNK_SIZE);
	}
	//ToDo: de-init channels/buffers/... if we ever implement a deinit cmd
	vTaskDelete(NULL);
}

//Run on core 1 if enabled, core 0 if not.
#define MY_CORE (portNUM_PROCESSORS-1)

int sndmixer_init(int p_no_channels, int p_samplerate) {
	no_channels=p_no_channels;
	samplerate=p_samplerate;
	kchal_sound_start(samplerate, 1024);
	channel=calloc(sizeof(sndmixer_channel_t), no_channels);
	if (!channel) return 0;
	curr_id=0;
	cmd_queue=xQueueCreate(10, sizeof(sndmixer_cmd_t));
	if (cmd_queue==NULL) {
		free(channel);
		return 0;
	}
	int r=xTaskCreatePinnedToCore(&sndmixer_task, "sndmixer", 2048, NULL, 5, NULL, MY_CORE);
	if (!r) {
		free(channel);
		vQueueDelete(cmd_queue);
		return 0;
	}
	return 1;
}

// The following functions all are essentially wrappers for the axt of pushing a command into the command queue.

int sndmixer_queue_wav(const void *wav_start, const void *wav_end, int evictable) {
	int id=new_id();
	sndmixer_cmd_t cmd={
		.id=id,
		.cmd=CMD_QUEUE_WAV,
		.queue_file_start=wav_start,
		.queue_file_end=wav_end,
		.flags=CHFL_PAUSED|(evictable?CHFL_EVICTABLE:0)
	};
	xQueueSend(cmd_queue, &cmd, portMAX_DELAY);
	return id;
}

int sndmixer_queue_mod(const void *mod_start, const void *mod_end) {
	int id=new_id();
	sndmixer_cmd_t cmd={
		.id=id,
		.cmd=CMD_QUEUE_MOD,
		.queue_file_start=mod_start,
		.queue_file_end=mod_end,
		.flags=CHFL_PAUSED
	};
	xQueueSend(cmd_queue, &cmd, portMAX_DELAY);
	return id;
}

void sndmixer_set_loop(int id, int do_loop) {
	sndmixer_cmd_t cmd={
		.cmd=CMD_LOOP,
		.id=id,
		.param=do_loop
	};
	xQueueSend(cmd_queue, &cmd, portMAX_DELAY);
}

void sndmixer_set_volume(int id, int volume) {
	sndmixer_cmd_t cmd={
		.cmd=CMD_VOLUME,
		.id=id,
		.param=volume
	};
	xQueueSend(cmd_queue, &cmd, portMAX_DELAY);
}

void sndmixer_play(int id) {
	sndmixer_cmd_t cmd={
		.cmd=CMD_PLAY,
		.id=id,
	};
	xQueueSend(cmd_queue, &cmd, portMAX_DELAY);
}

void sndmixer_pause(int id) {
	sndmixer_cmd_t cmd={
		.cmd=CMD_PAUSE,
		.id=id,
	};
	xQueueSend(cmd_queue, &cmd, portMAX_DELAY);
}

void sndmixer_stop(int id) {
	sndmixer_cmd_t cmd={
		.cmd=CMD_STOP,
		.id=id,
	};
	xQueueSend(cmd_queue, &cmd, portMAX_DELAY);
}

void sndmixer_pause_all() {
	sndmixer_cmd_t cmd={
		.cmd=CMD_PAUSE_ALL,
	};
	xQueueSend(cmd_queue, &cmd, portMAX_DELAY);
}

void sndmixer_resume_all() {
	sndmixer_cmd_t cmd={
		.cmd=CMD_RESUME_ALL,
	};
	xQueueSend(cmd_queue, &cmd, portMAX_DELAY);
}
