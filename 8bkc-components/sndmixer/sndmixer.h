#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Structure describing a sound source
 */
typedef struct {
	/*! Initialize the sound source. Returns size of data returned per call of fill_buffer. */
	int (*init_source)(const void *data_start, const void *data_end, int req_sample_rate, void **ctx);
	/*! Get the actual sample rate at which the source returns data */
	int (*get_sample_rate)(void *ctx);
	/*! Decode a bufferful of data. Returns 0 when file ended or something went wrong. Returns amount of bytes in buffer (normally what init_source returned) otherwise. */
	int (*fill_buffer)(void *ctx, int8_t *buffer);
	/*! Destroy source, free resources */
	void (*deinit_source)(void *ctx);
} sndmixer_source_t;

/**
 * @brief Initialize the sound mixer
 *
 * @note This function internally calls kchal_sound_start, there is no need to do this in your program
 *       if you use this function to initialize the sound mixer.
 *
 * @param no_channels Amount if sounds to be able to be played simultaneously.
 * @param samplerate Sample rate to mix all sources to
 */
int sndmixer_init(int no_channels, int samplerate);

/**
 * @brief Queue the data of a .wav file to be played
 *
 * This queues a sound to be played. It will not be actually played until sndmixer_play is called.
 *
 * @param wav_start Start of the wav-file data
 * @param wav_end End of the wav-file data
 * @param evictable If true, if all audio channels are filled and a new sound is queued, this
 *                  sound can be stopped to make room for the new sound.
 * @return The ID of the queued sound, for use with the other functions.
 */
int sndmixer_queue_wav(const void *wav_start, const void *wav_end, int evictable);

/**
 * @brief Queue the data of a .mod/.xm/.s3m file to be played
 *
 * This queues a piece of tracked music to be played. It will not be actually played until sndmixer_play is called.
 *
 * @param wav_start Start of the filedata
 * @param wav_end End of the filedata
 * @return The ID of the queued sound, for use with the other functions.
 */
int sndmixer_queue_mod(const void *mod_start, const void *mod_end);

/**
 * @brief Set or unset a sound to looping mode
 *
 * @warning This may not be implemented yet. TODO: remove this warning when implemented.
 *
 * @param id ID of the sound, obtained when queueing it
 * @param loop If true, the sound will loop back to the beginning when it ends.
 */
void sndmixer_set_loop(int id, int loop);

/**
 * @brief Set volume of a sound
 *
 * A queued sound will always start off with a volume setting of 255 (max volume). This call can be
 * used to adjust the volume of the sound at any time afterwards.
 *
 * @param id ID of the sound, obtained when queueing it
 * @param volume New volume, between 0 (muted) and 255 (full sound).
 */
void sndmixer_set_volume(int id, int volume);

/**
 * @brief Play a sound
 * 
 * When a sound is queued, it is not playing yet. Use this call to start playback. You can also
 * use this call to resume a sound paused by sndmixer_pause or sndmixer_pause_all.
 *
 * @param id ID of the sound, obtained when queueing it
 */
void sndmixer_play(int id);

/**
 * @brief Pause a sound
 * 
 * Stops playback of the sound. The sound can be resumed with sndmixer_play().
 *
 * @param id ID of the sound, obtained when queueing it
 */
void sndmixer_pause(int id);

/**
 * @brief Stop a sound, free the sound source and channel it used.
 * 
 * Stops playback of the sound and frees all associated structures.
 *
 * @param id ID of the sound, obtained when queueing it
 */
void sndmixer_stop(int id);

/**
 * @brief Pause all playing sounds
 * 
 * This can be used when e.g. the game is paused. Sounds can be individually un-paused afterwards and new sounds
 * can still be queued and played, given enough free/evictable channels.
 */
void sndmixer_pause_all();

/**
 * @brief Resume all paused sounds
 *
 * This can be used to undo a sndmixer_pause_all() call.
 */
void sndmixer_resume_all();


#ifdef __cplusplus
}
#endif
