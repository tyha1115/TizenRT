/****************************************************************************
 *
 * Copyright 2018 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
#include <tinyara/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <tinyara/audio/audio.h>
#include <tinyalsa/tinyalsa.h>

#include "audio_manager.h"
#include "resample/samplerate.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#ifndef CONFIG_AUDIO_DEV_PATH
#define CONFIG_AUDIO_DEV_PATH "/dev/audio"
#endif

#define AUDIO_DEV_PATH_LENGTH 11
#define AUDIO_PCM_PATH_LENGTH 9	//length of pcmC%uD%u%c + 1;
#define AUDIO_DEVICE_FULL_PATH_LENGTH (AUDIO_DEV_PATH_LENGTH + AUDIO_PCM_PATH_LENGTH)

#define AUDIO_DEVICE_PROCESS_QUEUE_PATH "/mqueue"
#define AUDIO_DEVICE_PROCESS_QUEUE_PATH_LENGTH AUDIO_DEVICE_FULL_PATH_LENGTH + strlen(AUDIO_DEVICE_PROCESS_QUEUE_PATH)

#define AUDIO_STREAM_BUFFER_SHORT_PERIOD 1024
#define AUDIO_STREAM_BUFFER_LONG_PERIOD 2048

#define AUDIO_STREAM_BUFFER_SHORT_PERIOD_COUNT 4
#define AUDIO_STREAM_BUFFER_LONG_PERIOD_COUNT 8

#define AUDIO_STREAM_CHANNEL_MONO 1
#define AUDIO_STREAM_CHANNEL_STEREO 2

#define AUDIO_STREAM_MEDIA_PERIOD_SIZE AUDIO_STREAM_BUFFER_LONG_PERIOD
#define AUDIO_STREAM_MEDIA_PERIOD_COUNT 8
#define AUDIO_STREAM_MEDIA_SAMPLE_RATE AUDIO_SAMP_RATE_44K
#define AUDIO_STREAM_MEDIA_CHANNEL AUDIO_STREAM_CHANNEL_STEREO

#define AUDIO_STREAM_VOIP_PERIOD_SIZE AUDIO_STREAM_BUFFER_LONG_PERIOD
#define AUDIO_STREAM_VOIP_PERIOD_COUNT AUDIO_STREAM_BUFFER_LONG_PERIOD_COUNT
#define AUDIO_STREAM_VOIP_SAMPLE_RATE AUDIO_SAMP_RATE_16K
#define AUDIO_STREAM_VOIP_CHANNEL AUDIO_STREAM_CHANNEL_STEREO

#define AUDIO_STREAM_VOICE_RECOGNITION_PERIOD_SIZE AUDIO_STREAM_BUFFER_SHORT_PERIOD
#define AUDIO_STREAM_VOICE_RECOGNITION_PERIOD_COUNT AUDIO_STREAM_BUFFER_SHORT_PERIOD_COUNT
#define AUDIO_STREAM_VOICE_RECOGNITION_SAMPLE_RATE AUDIO_SAMP_RATE_16K
#define AUDIO_STREAM_VOICE_RECOGNITION_CHANNEL AUDIO_STREAM_CHANNEL_STEREO

#define AUDIO_STREAM_RETRY_COUNT 2

#define AUDIO_DEVICE_MAX_VOLUME 10

#ifndef CONFIG_AUDIO_MAX_INPUT_CARD_NUM
#define CONFIG_AUDIO_MAX_INPUT_CARD_NUM 2
#endif

#ifndef CONFIG_AUDIO_MAX_OUTPUT_CARD_NUM
#define CONFIG_AUDIO_MAX_OUTPUT_CARD_NUM 2
#endif

#ifndef CONFIG_AUDIO_MAX_DEVICE_NUM
#define CONFIG_AUDIO_MAX_DEVICE_NUM 3
#endif

#ifndef CONFIG_AUDIO_RESAMPLER_BUFSIZE
#define CONFIG_AUDIO_RESAMPLER_BUFSIZE 4096
#endif

#define INVALID_ID -1

/****************************************************************************
 * Private Types
 ****************************************************************************/
enum audio_card_status_e {
	AUDIO_CARD_NONE = 0,		// card not found yet, shouldn't be assigned explicitly.
	AUDIO_CARD_IDLE = 1,
	AUDIO_CARD_READY,
	AUDIO_CARD_RUNNING,
	AUDIO_CARD_PAUSE
};

enum audio_io_direction_e {
	INPUT = 0,
	OUTPUT = 1
};

struct audio_device_config_s {
	enum audio_card_status_e status;
	uint8_t volume;
	uint8_t max_volume;
	audio_device_type_t device_type;
	device_process_type_t device_process_type;
	mqd_t process_handler;
};

struct audio_resample_s {
	bool necessary;
	uint8_t samprate_types;
	unsigned int from;
	unsigned int to;
	void *buffer;
	unsigned int buffer_size;
	float ratio;
	src_handle_t handle;
};

struct audio_card_info_s {
	uint8_t card_id;			//current card id
	uint8_t device_id;			//current device id
	struct audio_device_config_s config[CONFIG_AUDIO_MAX_DEVICE_NUM];
	struct pcm *pcm;
	audio_manager_stream_policy_t policy;
	struct audio_resample_s resample;
	pthread_mutex_t card_mutex;
};

struct audio_samprate_map_entry_s {
	uint8_t samprate_types;
	unsigned int samprate;
};

typedef enum audio_card_status_e audio_card_status_t;
typedef enum audio_io_direction_e audio_io_direction_t;
typedef struct audio_device_config_s audio_config_t;
typedef struct audio_card_info_s audio_card_info_t;

static audio_card_info_t g_audio_in_cards[CONFIG_AUDIO_MAX_INPUT_CARD_NUM];
static audio_card_info_t g_audio_out_cards[CONFIG_AUDIO_MAX_OUTPUT_CARD_NUM];

static int g_actual_audio_in_card_id = INVALID_ID;
static int g_actual_audio_out_card_id = INVALID_ID;

static const struct audio_samprate_map_entry_s g_audio_samprate_entry[] = {
	{AUDIO_SAMP_RATE_TYPE_8K, AUDIO_SAMP_RATE_8K},
	{AUDIO_SAMP_RATE_TYPE_11K, AUDIO_SAMP_RATE_11K},
	{AUDIO_SAMP_RATE_TYPE_16K, AUDIO_SAMP_RATE_16K},
	{AUDIO_SAMP_RATE_TYPE_22K, AUDIO_SAMP_RATE_22K},
	{AUDIO_SAMP_RATE_TYPE_32K, AUDIO_SAMP_RATE_32K},
	{AUDIO_SAMP_RATE_TYPE_44K, AUDIO_SAMP_RATE_44K},
	{AUDIO_SAMP_RATE_TYPE_48K, AUDIO_SAMP_RATE_48K},
	{AUDIO_SAMP_RATE_TYPE_96K, AUDIO_SAMP_RATE_96K}
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static audio_manager_result_t find_audio_card(audio_io_direction_t direct);
static audio_manager_result_t get_actual_audio_in_card_id(void);
static audio_manager_result_t get_actual_audio_out_card_id(void);
static void get_hardware_params(audio_card_info_t *card, audio_io_direction_t direct);
static audio_manager_result_t get_supported_sample_rate(int fd, uint8_t *sample_type, audio_io_direction_t direct);
static audio_manager_result_t get_supported_process_type(int fd, audio_config_t *config, audio_io_direction_t direct);
static uint32_t get_closest_samprate(unsigned origin_samprate, audio_io_direction_t direct);
static unsigned int resample_stream_in(audio_card_info_t *card, void *data, unsigned int frames);
static unsigned int resample_stream_out(audio_card_info_t *card, void *data, unsigned int frames);
static audio_manager_result_t get_audio_volume(int fd, audio_config_t *config, audio_io_direction_t direct);
static audio_manager_result_t set_audio_volume(audio_io_direction_t direct, uint8_t volume);

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static void get_card_path(char *card_path, uint8_t card_id, uint8_t device_id, audio_io_direction_t direct)
{
	char type_chr = 'p';
	if (direct == INPUT) {
		type_chr = 'c';
	}

	snprintf(card_path, (AUDIO_DEVICE_FULL_PATH_LENGTH), "%s/pcmC%uD%u%c", CONFIG_AUDIO_DEV_PATH, card_id, device_id, type_chr);
	medvdbg("card_path : %s\n", card_path);
}

static audio_manager_result_t init_audio_stream(audio_io_direction_t direct)
{
	if (((direct == INPUT) && (g_actual_audio_in_card_id >= 0)) || ((direct == OUTPUT) && (g_actual_audio_out_card_id >= 0))) {
		return AUDIO_MANAGER_SUCCESS;
	}

	/* check available cards and try to find all cards if there's no one */
	if (find_audio_card(direct) != AUDIO_MANAGER_SUCCESS) {
		meddbg("Found no active %s audio card\n", (direct == OUTPUT ? "output" : "input"));
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}

	return AUDIO_MANAGER_SUCCESS;
}

static audio_manager_result_t find_audio_card(audio_io_direction_t direct)
{
	audio_manager_result_t ret = AUDIO_MANAGER_SUCCESS;
	int card_id = INVALID_ID;
	int device_id = INVALID_ID;
	char type;
	struct dirent *dir_entry;
	DIR *dir_info;
	audio_card_info_t *card = g_audio_out_cards;
	int max_card_num = CONFIG_AUDIO_MAX_OUTPUT_CARD_NUM;
	char type_chr = 'p';
	int found_cards = 0;

	if (!(dir_info = opendir(CONFIG_AUDIO_DEV_PATH))) {
		return AUDIO_MANAGER_DEVICE_FAIL;
	}

	if (direct == INPUT) {
		max_card_num = CONFIG_AUDIO_MAX_INPUT_CARD_NUM;
		type_chr = 'c';
		card = g_audio_in_cards;
	}

	while ((dir_entry = readdir(dir_info)) != NULL) {
		// TODO: Add cases for various drivers. Currently, identify 'pcm' drivers only.
		if ((dir_entry->d_name[0] != 'p') || (dir_entry->d_name[1] != 'c') || (dir_entry->d_name[2] != 'm') || (sscanf(&dir_entry->d_name[3], "C%uD%u%c", &card_id, &device_id, &type) != 3)) {
			continue;
		}

		/* Find next card if driver name isn't follow naming policy */
		if ((card_id < 0) || (card_id >= max_card_num) || (device_id < 0) || (device_id >= CONFIG_AUDIO_MAX_DEVICE_NUM)) {
			meddbg("Found wrong card card_id : %d device_id : %d\n", card_id, device_id);
			continue;
		}

		if (type == type_chr) {
			pthread_mutex_init(&(card[card_id].card_mutex), NULL);
			pthread_mutex_lock(&(card[card_id].card_mutex));
			card[card_id].config[device_id].status = AUDIO_CARD_IDLE;
			card[card_id].card_id = card_id;
			card[card_id].device_id = device_id;
			found_cards++;
			medvdbg("Found an audio card, total card : %d id : %d device : %d\n", found_cards, card_id, device_id);

			get_hardware_params(&card[card_id], direct);

			pthread_mutex_unlock(&(card[card_id].card_mutex));
		}
	}

	/* If nothing founded */
	if (found_cards == 0) {
		meddbg("Failed to find card\n");
		ret = AUDIO_MANAGER_NO_AVAIL_CARD;
		goto error_out;
	}

	/* TODO update an actual card here, but mutex needed?? */
	if (direct == INPUT) {
		if (get_actual_audio_in_card_id() != AUDIO_MANAGER_SUCCESS) {
			ret = AUDIO_MANAGER_NO_AVAIL_CARD;
		}
	} else {
		if (get_actual_audio_out_card_id() != AUDIO_MANAGER_SUCCESS) {
			ret = AUDIO_MANAGER_NO_AVAIL_CARD;
		}
	}

error_out:
	closedir(dir_info);
	return ret;
}

static void get_hardware_params(audio_card_info_t *card, audio_io_direction_t direct)
{
	int fd;
	int ret;
	audio_config_t *config;
	char card_path[AUDIO_DEVICE_FULL_PATH_LENGTH];
	get_card_path(card_path, card->card_id, card->device_id, direct);
	fd = open(card_path, O_RDONLY);
	if (fd < 0) {
		meddbg("Failed to open audio driver, path : %s errno : %d\n", card_path, errno);
		return;
	}

	config = &card->config[card->device_id];

	/* get gain/volume of current card first */
	ret = get_audio_volume(fd, config, direct);

	/* If it is failed, it doesn't return because we should get next h/w params */
	if (ret != AUDIO_MANAGER_SUCCESS) {
		meddbg("Get volume failed card path : %s direct : %d ret : %d\n", card_path, direct, ret);
	}

	medvdbg("Max volume: %d, Current Volume : %d card path : %s\n", config->max_volume, config->volume, card_path);

	/* get supported sample rate type */
	ret = get_supported_sample_rate(fd, &card->resample.samprate_types, direct);
	if (ret != AUDIO_MANAGER_SUCCESS) {
		meddbg("Get supported sample rate failed card path : %s direct : %d ret : %d\n", card_path, direct, ret);
	}
	medvdbg("card path : %s Supported sample rate: %x\n", card_path, card->resample.samprate_types);

	/** TODO card type handling logic should be added **/
	get_supported_process_type(fd, config, direct);

	close(fd);
}

static audio_manager_result_t get_supported_sample_rate(int fd, uint8_t *sample_type, audio_io_direction_t direct)
{
	struct audio_caps_desc_s caps_desc;

	caps_desc.caps.ac_len = sizeof(struct audio_caps_s);
	pthread_mutex_t *card_mutex;

	if (direct == INPUT) {
		card_mutex = &g_audio_in_cards[g_actual_audio_in_card_id].card_mutex;
		caps_desc.caps.ac_type = AUDIO_TYPE_INPUT;
	} else {
		card_mutex = &g_audio_out_cards[g_actual_audio_out_card_id].card_mutex;
		caps_desc.caps.ac_type = AUDIO_TYPE_OUTPUT;
	}
	caps_desc.caps.ac_subtype = AUDIO_TYPE_QUERY;
	pthread_mutex_lock(card_mutex);
	if (ioctl(fd, AUDIOIOC_GETCAPS, (unsigned long)&caps_desc.caps) < 0) {
		meddbg("Fail to ioctl AUDIOIOC_GETCAPS\n");
		pthread_mutex_unlock(card_mutex);
		return AUDIO_MANAGER_DEVICE_FAIL;
	}

	*sample_type = caps_desc.caps.ac_controls.b[0];
	pthread_mutex_unlock(card_mutex);
	return AUDIO_MANAGER_SUCCESS;
}

/* TODO this function should consider priority of each function in future */
static audio_manager_result_t get_actual_audio_in_card_id()
{
	int i, j;
	int cnt = 0;

	for (j = 0; j < CONFIG_AUDIO_MAX_DEVICE_NUM; j++) {
		for (i = 0; i < CONFIG_AUDIO_MAX_INPUT_CARD_NUM; i++) {
			if (g_audio_in_cards[i].config[j].status != AUDIO_CARD_NONE) {
				if (g_audio_in_cards[i].config[j].status == AUDIO_CARD_IDLE) {
					cnt++;
				} else {
					/* found activated one */
					g_audio_in_cards[i].device_id = j;
					g_actual_audio_in_card_id = i;
					return AUDIO_MANAGER_SUCCESS;
				}
			}
		}
	}
	/* Return basic one If we found device & all devices in each card are IDLE */
	if (cnt > 0) {
		g_actual_audio_in_card_id = 0;
		g_audio_in_cards[0].device_id = 0;
		return AUDIO_MANAGER_SUCCESS;
	}
	return AUDIO_MANAGER_NO_AVAIL_CARD;
}

static audio_manager_result_t get_actual_audio_out_card_id()
{
	int i, j;
	int cnt = 0;

	for (j = 0; j < CONFIG_AUDIO_MAX_DEVICE_NUM; j++) {
		for (i = 0; i < CONFIG_AUDIO_MAX_OUTPUT_CARD_NUM; i++) {
			if (g_audio_out_cards[i].config[j].status != AUDIO_CARD_NONE) {
				if (g_audio_out_cards[i].config[j].status == AUDIO_CARD_IDLE) {
					cnt++;
				} else {
					/* found activated one */
					g_audio_out_cards[i].device_id = j;
					g_actual_audio_out_card_id = i;
					return AUDIO_MANAGER_SUCCESS;

				}
			}
		}
	}
	/* Return basic one If we found device & all devices in each card are IDLE */
	if (cnt > 0) {
		g_actual_audio_out_card_id = 0;
		g_audio_out_cards[0].device_id = 0;
		return AUDIO_MANAGER_SUCCESS;
	}
	return AUDIO_MANAGER_NO_AVAIL_CARD;
}

static uint32_t get_closest_samprate(unsigned int origin_samprate, audio_io_direction_t direct)
{
	int i;
	uint32_t result = 0;
	uint32_t samprate_types;
	int count = sizeof(g_audio_samprate_entry) / sizeof(struct audio_samprate_map_entry_s);

	ASSERT(count > 0);

	if (direct == INPUT) {
		ASSERT(g_actual_audio_in_card_id >= 0);
		samprate_types = g_audio_in_cards[g_actual_audio_in_card_id].resample.samprate_types;
	} else {
		ASSERT(g_actual_audio_out_card_id >= 0);
		samprate_types = g_audio_out_cards[g_actual_audio_out_card_id].resample.samprate_types;
	}

	for (i = 0; i < count; i++) {
		if (g_audio_samprate_entry[i].samprate_types & samprate_types) {
			result = g_audio_samprate_entry[i].samprate;
			if (g_audio_samprate_entry[i].samprate >= origin_samprate) {
				break;
			}
		}
	}

	return result;
}

static unsigned int resample_stream_in(audio_card_info_t *card, void *data, unsigned int frames)
{
	unsigned int used_frames = 0;
	unsigned int resampled_frames = 0;
	src_data_t srcData = { 0, };

	srcData.channels_num = 2;
	srcData.origin_sample_rate = card->resample.from;
	srcData.origin_sample_width = SAMPLE_WIDTH_16BITS;
	srcData.desired_sample_rate = card->resample.to;
	srcData.desired_sample_width = SAMPLE_WIDTH_16BITS;
	srcData.input_frames_used = 0;
	srcData.data_out = data;
	srcData.out_buf_length = card->resample.buffer_size;

	while (frames > used_frames) {
		srcData.data_in = card->resample.buffer + get_input_frames_to_byte(used_frames);
		srcData.input_frames = frames - used_frames;
		srcData.data_out = data + get_input_frames_to_byte(resampled_frames);
		medvdbg("data_in addr = 0x%x + %d\t", srcData.data_in, get_input_frames_to_byte(used_frames));
		if (src_simple(card->resample.handle, &srcData) != SRC_ERR_NO_ERROR) {
			meddbg("Fail to resample in:%d/%d, out:%d, to %u from %u\n", used_frames, frames, srcData.desired_sample_rate, srcData.origin_sample_rate);
			return AUDIO_MANAGER_RESAMPLE_FAIL;
		}

		if (srcData.output_frames_gen > 0) {
			resampled_frames += srcData.output_frames_gen;
			used_frames += srcData.input_frames_used;
			medvdbg("Record resampled in:%d/%d, out:%d\n", used_frames, frames, resampled_frames);
		} else {
			meddbg("Wrong output_frames_gen : %d\n", srcData.output_frames_gen);
			return AUDIO_MANAGER_RESAMPLE_FAIL;
		}
	}

	return resampled_frames;
}

static unsigned int resample_stream_out(audio_card_info_t *card, void *data, unsigned int frames)
{
	unsigned int used_frames = 0;
	unsigned int resampled_frames = 0;
	src_data_t srcData = { 0, };

	srcData.channels_num = 2;
	srcData.origin_sample_rate = card->resample.from;
	srcData.origin_sample_width = SAMPLE_WIDTH_16BITS;
	srcData.desired_sample_rate = card->resample.to;
	srcData.desired_sample_width = SAMPLE_WIDTH_16BITS;
	srcData.input_frames_used = 0;
	srcData.data_out = card->resample.buffer;
	srcData.out_buf_length = card->resample.buffer_size;
	medvdbg("resampler buffer_size = %d, buffer_addr = 0x%x\n", card->resample.buffer_size, card->resample.buffer);

	while (frames > used_frames) {
		srcData.data_in = data + get_output_frames_to_byte(used_frames);
		srcData.input_frames = frames - used_frames;
		srcData.data_out = card->resample.buffer + get_output_frames_to_byte(resampled_frames);
		medvdbg("buffer addr = 0x%x   data_out addr = 0x%x   ", card->resample.buffer, srcData.data_out);
		if (src_simple(card->resample.handle, &srcData) != SRC_ERR_NO_ERROR) {
			meddbg("Fail to resample in:%d/%d, out:%d, to %u from %u\n", used_frames, frames, srcData.desired_sample_rate, srcData.origin_sample_rate);
			return AUDIO_MANAGER_RESAMPLE_FAIL;
		}

		if (srcData.output_frames_gen > 0) {
			resampled_frames += srcData.output_frames_gen;
			used_frames += srcData.input_frames_used;
		} else {
			meddbg("Wrong output_frames_gen : %d\n", srcData.output_frames_gen);
			return AUDIO_MANAGER_RESAMPLE_FAIL;
		}
		medvdbg("%d resampled from (%d/%d) @ 0x%x\t", resampled_frames, used_frames, frames, srcData.data_out);
	}
	medvdbg("Resample finished\n");

	return resampled_frames;
}

static audio_manager_result_t get_audio_volume(int fd, audio_config_t *config, audio_io_direction_t direct)
{
	struct audio_caps_desc_s caps_desc;
	uint8_t max_volume;
	uint8_t cur_volume;
	pthread_mutex_t *card_mutex;

	caps_desc.caps.ac_len = sizeof(struct audio_caps_s);
	caps_desc.caps.ac_type = AUDIO_TYPE_FEATURE;

	if (direct == INPUT) {
		caps_desc.caps.ac_format.hw = AUDIO_FU_INP_GAIN;
		card_mutex = &g_audio_in_cards[g_actual_audio_in_card_id].card_mutex;
	} else {
		caps_desc.caps.ac_format.hw = AUDIO_FU_VOLUME;
		card_mutex = &g_audio_out_cards[g_actual_audio_out_card_id].card_mutex;
	}

	pthread_mutex_lock(card_mutex);
	if (ioctl(fd, AUDIOIOC_GETCAPS, (unsigned long)&caps_desc.caps) < 0) {
		meddbg("An ioctl error occurs\n");
		pthread_mutex_unlock(card_mutex);
		return AUDIO_MANAGER_DEVICE_FAIL;
	}

	max_volume = caps_desc.caps.ac_controls.hw[0];
	cur_volume = caps_desc.caps.ac_controls.hw[1];

	/* scale here */
	cur_volume = cur_volume * AUDIO_DEVICE_MAX_VOLUME / (max_volume - (max_volume % AUDIO_DEVICE_MAX_VOLUME));

	config->max_volume = max_volume;
	config->volume = (uint8_t) cur_volume;
	pthread_mutex_unlock(card_mutex);
	medvdbg("Max_vol = %d,  cur_vol = %d\n", config->max_volume, config->volume);

	return AUDIO_MANAGER_SUCCESS;
}

static audio_manager_result_t set_audio_volume(audio_io_direction_t direct, uint8_t volume)
{
	int ret;
	int fd;
	struct audio_caps_desc_s caps_desc;
	audio_card_info_t *card;
	audio_config_t *config;
	char card_path[AUDIO_DEVICE_FULL_PATH_LENGTH];
	pthread_mutex_t *card_mutex;

	if (volume > AUDIO_DEVICE_MAX_VOLUME) {
		volume = AUDIO_DEVICE_MAX_VOLUME;
	}

	if (direct == INPUT) {
		caps_desc.caps.ac_format.hw = AUDIO_FU_INP_GAIN;
		card = &g_audio_in_cards[g_actual_audio_in_card_id];
		card_mutex = &g_audio_in_cards[g_actual_audio_in_card_id].card_mutex;
	} else {
		caps_desc.caps.ac_format.hw = AUDIO_FU_VOLUME;
		card = &g_audio_out_cards[g_actual_audio_out_card_id];
		card_mutex = &g_audio_out_cards[g_actual_audio_out_card_id].card_mutex;
	}

	config = &card->config[card->device_id];
	caps_desc.caps.ac_controls.hw[0] = volume * (config->max_volume / AUDIO_DEVICE_MAX_VOLUME);
	caps_desc.caps.ac_len = sizeof(struct audio_caps_s);
	caps_desc.caps.ac_type = AUDIO_TYPE_FEATURE;

	get_card_path(card_path, card->card_id, card->device_id, direct);

	pthread_mutex_lock(card_mutex);
	fd = open(card_path, O_RDONLY);
	if (fd < 0) {
		meddbg("Failed to open device path : %s type : %d\n", card_path, direct);
		ret = AUDIO_MANAGER_DEVICE_FAIL;
		goto errout;
	}

	ret = ioctl(fd, AUDIOIOC_CONFIGURE, (unsigned long)&caps_desc);
	if (ret < 0) {
		meddbg("Fail to set a volume, ret = %d\n", ret);
		if (ret == -ENOSYS) {
			ret = AUDIO_MANAGER_DEVICE_NOT_SUPPORT;
		} else {
			ret = AUDIO_MANAGER_DEVICE_FAIL;
		}
		goto errout_with_fd;
	}

	ret = AUDIO_MANAGER_SUCCESS;
	config->volume = volume;
	medvdbg("Volume = %d\n", volume);

errout_with_fd:
	close(fd);
errout:
	pthread_mutex_unlock(card_mutex);
	return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
audio_manager_result_t init_audio_stream_in(void)
{
	return init_audio_stream(INPUT);
}

audio_manager_result_t init_audio_stream_out(void)
{
	return init_audio_stream(OUTPUT);
}

audio_manager_result_t set_audio_stream_in(unsigned int channels, unsigned int sample_rate, int format)
{
	audio_card_info_t *card;
	struct pcm_config config;
	audio_manager_result_t ret = AUDIO_MANAGER_SUCCESS;

	if (g_actual_audio_in_card_id < 0) {
		meddbg("Found no active input audio card\n");
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}

	if ((channels == 0) || (sample_rate == 0)) {
		return AUDIO_MANAGER_INVALID_PARAM;
	}
	card = &g_audio_in_cards[g_actual_audio_in_card_id];
	if ((card->config[card->device_id].status == AUDIO_CARD_PAUSE)) {
		medvdbg("reset previous preparing\n");
		reset_audio_stream_out();
	}

	pthread_mutex_lock(&(card->card_mutex));

	/* TODO Resample channel logic & sample rate based on policy should be added */
	if (channels > AUDIO_STREAM_CHANNEL_MONO) {
		channels = AUDIO_STREAM_CHANNEL_STEREO;
	}

	memset(&config, 0, sizeof(struct pcm_config));
	config.channels = channels;
	config.rate = get_closest_samprate(sample_rate, INPUT);
	config.format = format;
	config.period_size = AUDIO_STREAM_VOICE_RECOGNITION_PERIOD_SIZE;
	config.period_count = AUDIO_STREAM_VOICE_RECOGNITION_PERIOD_COUNT;

	medvdbg("[IN] Device samplerate: %u, User requested: %u\n", config.rate, sample_rate);
	card->pcm = pcm_open(g_actual_audio_in_card_id, card->device_id, PCM_IN, &config);
	if (!pcm_is_ready(card->pcm)) {
		meddbg("fail to pcm_is_ready() error : %s", pcm_get_error(card->pcm));
		ret = AUDIO_MANAGER_CARD_NOT_READY;
		goto error_with_pcm;
	}

	card->config[card->device_id].status = AUDIO_CARD_READY;

	if (sample_rate == config.rate) {
		card->resample.necessary = false;
	} else {
		card->resample.necessary = true;
		card->resample.from = config.rate;
		card->resample.to = sample_rate;
		card->resample.ratio = (float)sample_rate / (float)config.rate;
		card->resample.handle = src_init(CONFIG_AUDIO_RESAMPLER_BUFSIZE);
		card->resample.buffer = malloc((int)((float)get_input_frames_to_byte(get_input_frame_count()) / card->resample.ratio) + 1);	// +1 for floating point margin
		if (!card->resample.buffer) {
			meddbg("malloc for a resampling buffer(stream_in) is failed\n");
			ret = AUDIO_MANAGER_RESAMPLE_FAIL;
			goto error_with_pcm;
		}
	}

	pthread_mutex_unlock(&(card->card_mutex));
	return ret;

error_with_pcm:
	pcm_close(card->pcm);
	pthread_mutex_unlock(&(card->card_mutex));
	return ret;
}

audio_manager_result_t set_audio_stream_out(unsigned int channels, unsigned int sample_rate, int format)
{
	audio_card_info_t *card;
	struct pcm_config config;
	audio_manager_result_t ret = AUDIO_MANAGER_SUCCESS;

	if (g_actual_audio_out_card_id < 0) {
		meddbg("Found no active output audio card\n");
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}

	if ((channels == 0) || (sample_rate == 0)) {
		return AUDIO_MANAGER_INVALID_PARAM;
	}
	card = &g_audio_out_cards[g_actual_audio_out_card_id];

	if (card->config[card->device_id].status == AUDIO_CARD_PAUSE) {
		medvdbg("reset previous preparing\n");
		reset_audio_stream_out();
	}

	pthread_mutex_lock(&(card->card_mutex));

	/* TODO Resample channel logic & sample rate based on policy should be added */
	if (channels > AUDIO_STREAM_CHANNEL_MONO) {
		channels = AUDIO_STREAM_CHANNEL_STEREO;
	}

	memset(&config, 0, sizeof(struct pcm_config));
	config.channels = channels;
	config.rate = get_closest_samprate(sample_rate, OUTPUT);
	config.format = format;
	config.period_size = AUDIO_STREAM_VOICE_RECOGNITION_PERIOD_SIZE;
	config.period_count = AUDIO_STREAM_VOICE_RECOGNITION_PERIOD_COUNT;

	if (sample_rate == config.rate) {
		card->resample.necessary = false;
	} else {
		card->resample.necessary = true;
		card->resample.from = sample_rate;
		card->resample.to = config.rate;
		card->resample.ratio = (float)config.rate / (float)sample_rate;
	}

	medvdbg("[OUT] Device samplerate: %u, User requested: %u, channels: %u\n", config.rate, sample_rate, channels);

	medvdbg("actual output card id = %d\n", g_actual_audio_out_card_id);
	card->pcm = pcm_open(g_actual_audio_out_card_id, card->device_id, PCM_OUT, &config);

	if (!pcm_is_ready(card->pcm)) {
		meddbg("fail to pcm_is_ready() error : %s", pcm_get_error(card->pcm));
		ret = AUDIO_MANAGER_CARD_NOT_READY;
		goto error_with_pcm;
	}

	card->config[card->device_id].status = AUDIO_CARD_READY;

	if (card->resample.necessary) {
		card->resample.handle = src_init(CONFIG_AUDIO_RESAMPLER_BUFSIZE);
		card->resample.buffer_size = (int)((float)get_output_frames_to_byte(get_output_frame_count()) * card->resample.ratio) + 1;	// +1 for floating point margin
		card->resample.buffer = malloc(card->resample.buffer_size);
		if (!card->resample.buffer) {
			meddbg("malloc for a resampling buffer(stream_out) is failed\n");
			ret = AUDIO_MANAGER_RESAMPLE_FAIL;
			goto error_with_pcm;
		}
		medvdbg("resampler ratio = %f, buffer_size = %d\t", card->resample.ratio, card->resample.buffer_size);
		medvdbg("buffer address = 0x%x\n", card->resample.buffer);
	}

	pthread_mutex_unlock(&(card->card_mutex));
	return ret;

error_with_pcm:
	pcm_close(card->pcm);
	pthread_mutex_unlock(&(card->card_mutex));
	return ret;
}

int start_audio_stream_in(void *data, unsigned int frames)
{
	int ret;
	int prepare_retry = AUDIO_STREAM_RETRY_COUNT;
	audio_card_info_t *card;

	if (g_actual_audio_in_card_id < 0) {
		meddbg("Found no active input audio card\n");
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}

	card = &g_audio_in_cards[g_actual_audio_in_card_id];
	pthread_mutex_lock(&(card->card_mutex));

	if (card->config[card->device_id].status == AUDIO_CARD_PAUSE) {
		ret = ioctl(pcm_get_file_descriptor(card->pcm), AUDIOIOC_RESUME, 0UL);
		if (ret < 0) {
			meddbg("Fail to ioctl AUDIOIOC_RESUME, ret = %d\n", ret);
			ret = AUDIO_MANAGER_DEVICE_FAIL;
			goto error_with_lock;
		}

		card->config[card->device_id].status = AUDIO_CARD_RUNNING;
		medvdbg("Resume the input audio card!!\n");
	}

	do {
		// Todo: Logic needs to be changed.
		//       From: 1 pcm_readi - 1 resampling
		//       To  : * pcm_readi - 1 resampling
		if (card->resample.necessary) {
			unsigned int frames_to_read = (unsigned int)((float)frames / card->resample.ratio);
			if (frames_to_read > (get_input_frame_count() / card->resample.ratio)) {
				frames_to_read = (get_input_frame_count() / card->resample.ratio);
			}

			card->resample.buffer_size = get_input_frames_to_byte(frames);

			ret = pcm_readi(card->pcm, card->resample.buffer, frames_to_read);
			medvdbg("Read frames (%d/%u) for resample to %u\n", ret, frames_to_read, frames);
		} else {
			ret = pcm_readi(card->pcm, data, frames);
			medvdbg("Read %d frames\n", ret);
		}

		if (ret == -EPIPE) {
			ret = pcm_prepare(card->pcm);
			meddbg("PCM is reprepared\n");
			if (ret != OK) {
				meddbg("Fail to pcm_prepare()\n");
				ret = AUDIO_MANAGER_XRUN_STATE;
				goto error_with_lock;
			}
		} else if (ret == -EINVAL) {
			ret = AUDIO_MANAGER_INVALID_PARAM;
			goto error_with_lock;
		} else {
			break;
		}
	} while ((ret == OK) && (prepare_retry--));

	if (card->resample.necessary) {
		ret = resample_stream_in(card, data, ret);
		medvdbg("Resampled frames = %d\n", ret);
	}

error_with_lock:
	pthread_mutex_unlock(&(card->card_mutex));

	return ret;
}

int start_audio_stream_out(void *data, unsigned int frames)
{
	int ret;
	unsigned int resampled_frames = 0;
	int prepare_retry = AUDIO_STREAM_RETRY_COUNT;
	audio_card_info_t *card;
	medvdbg("start_audio_stream_out(%u)\n", frames);

	if (g_actual_audio_out_card_id < 0) {
		meddbg("Found no active output audio card\n");
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}

	card = &g_audio_out_cards[g_actual_audio_out_card_id];

	pthread_mutex_lock(&(card->card_mutex));

	if (card->config[card->device_id].status == AUDIO_CARD_PAUSE) {
		ret = ioctl(pcm_get_file_descriptor(card->pcm), AUDIOIOC_RESUME, 0UL);
		if (ret < 0) {
			meddbg("Fail to ioctl AUDIOIOC_RESUME, ret = %d\n", ret);
			ret = AUDIO_MANAGER_DEVICE_FAIL;
			goto error_with_lock;
		}

		card->config[card->device_id].status = AUDIO_CARD_RUNNING;
		medvdbg("Resume the output audio card!!\n");
	}

	if (card->resample.necessary) {
		resampled_frames = resample_stream_out(card, data, frames);
	}

	do {
		medvdbg("Start Playing!! Resample : %d\t", card->resample.necessary);
		if (card->resample.necessary) {
			ret = pcm_writei(card->pcm, card->resample.buffer, resampled_frames);
		} else {
			ret = pcm_writei(card->pcm, data, frames);
		}

		if (ret < 0) {
			if (ret == -EPIPE) {
				if (prepare_retry > 0) {
					ret = pcm_prepare(card->pcm);
					if (ret != OK) {
						meddbg("Fail to pcm_prepare()\n");
						ret = AUDIO_MANAGER_XRUN_STATE;
						goto error_with_lock;
					}
					prepare_retry--;
				} else {
					meddbg("prepare_retry = 0\n");
					ret = AUDIO_MANAGER_XRUN_STATE;
					goto error_with_lock;
				}
			} else if (ret == -EINVAL) {
				meddbg("pcm_writei = -EINVAL\n");
				ret = AUDIO_MANAGER_INVALID_PARAM;
				goto error_with_lock;
			} else {
				ret = AUDIO_MANAGER_OPERATION_FAIL;
				goto error_with_lock;
			}
		}
	} while (ret == OK);

error_with_lock:
	pthread_mutex_unlock(&(card->card_mutex));
	return ret;
}

audio_manager_result_t pause_audio_stream_in(void)
{
	int ret;
	audio_card_info_t *card;

	if (g_actual_audio_in_card_id < 0) {
		meddbg("Found no active input audio card\n");
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}

	card = &g_audio_in_cards[g_actual_audio_in_card_id];
	pthread_mutex_lock(&(card->card_mutex));

	ret = ioctl(pcm_get_file_descriptor(card->pcm), AUDIOIOC_PAUSE, 0UL);
	if (ret < 0) {
		meddbg("Fail to ioctl AUDIOIOC_PAUSE, ret = %d\n", ret);
		pthread_mutex_unlock(&(g_audio_in_cards[g_actual_audio_in_card_id].card_mutex));
		return AUDIO_MANAGER_DEVICE_FAIL;
	}

	card->config[card->device_id].status = AUDIO_CARD_PAUSE;

	pthread_mutex_unlock(&(card->card_mutex));

	return AUDIO_MANAGER_SUCCESS;
}

audio_manager_result_t pause_audio_stream_out(void)
{
	int ret;
	audio_card_info_t *card;

	if (g_actual_audio_out_card_id < 0) {
		meddbg("Found no active output audio card\n");
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}

	card = &g_audio_out_cards[g_actual_audio_out_card_id];
	pthread_mutex_lock(&(card->card_mutex));

	ret = ioctl(pcm_get_file_descriptor(card->pcm), AUDIOIOC_PAUSE, 0UL);
	if (ret < 0) {
		meddbg("Fail to ioctl AUDIOIOC_PAUSE, ret = %d\n", ret);
		pthread_mutex_unlock(&(card->card_mutex));
		return AUDIO_MANAGER_DEVICE_FAIL;
	}

	card->config[card->device_id].status = AUDIO_CARD_PAUSE;

	pthread_mutex_unlock(&(card->card_mutex));

	return AUDIO_MANAGER_SUCCESS;
}

audio_manager_result_t stop_audio_stream_in(void)
{
	int ret;
	audio_card_info_t *card;

	if (g_actual_audio_in_card_id < 0) {
		meddbg("Found no active input audio card\n");
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}

	card = &g_audio_in_cards[g_actual_audio_in_card_id];
	pthread_mutex_lock(&(card->card_mutex));

	ret = pcm_drop(card->pcm);
	if (ret < 0) {
		pthread_mutex_unlock(&(card->card_mutex));
		meddbg("pcm_drop Failed, ret = %d\n", ret);
		return AUDIO_MANAGER_DEVICE_FAIL;
	}

	card->config[card->device_id].status = AUDIO_CARD_READY;

	pthread_mutex_unlock(&(card->card_mutex));

	return AUDIO_MANAGER_SUCCESS;
}

audio_manager_result_t stop_audio_stream_out(void)
{
	int ret;
	audio_card_info_t *card;

	if (g_actual_audio_out_card_id < 0) {
		meddbg("Found no active output audio card\n");
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}

	card = &g_audio_out_cards[g_actual_audio_out_card_id];
	pthread_mutex_lock(&(card->card_mutex));

	if (card->config[card->device_id].status == AUDIO_CARD_PAUSE) {
		if ((ret = pcm_drop(card->pcm)) < 0) {
			meddbg("pcm_drop faled, ret = %d\n", ret);
		}
	} else {
		if ((ret = pcm_drain(card->pcm)) < 0) {
			meddbg("pcm_drain faled, ret = %d\n", ret);
		}
	}
	card->config[card->device_id].status = AUDIO_CARD_READY;

	pthread_mutex_unlock(&(card->card_mutex));

	if (ret < 0) {
		return AUDIO_MANAGER_DEVICE_FAIL;
	}
	return AUDIO_MANAGER_SUCCESS;
}

audio_manager_result_t reset_audio_stream_in(void)
{
	audio_card_info_t *card;

	if (g_actual_audio_in_card_id < 0) {
		meddbg("Found no active input audio card\n");
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}

	card = &g_audio_in_cards[g_actual_audio_in_card_id];
	pthread_mutex_lock(&(card->card_mutex));

	pcm_close(card->pcm);
	card->pcm = NULL;

	if (card->resample.necessary) {
		if (card->resample.buffer) {
			free(card->resample.buffer);
		}
		src_destroy(card->resample.handle);
	}

	card->config[card->device_id].status = AUDIO_CARD_IDLE;
	card->policy = AUDIO_MANAGER_STREAM_TYPE_MEDIA;
	pthread_mutex_unlock(&(card->card_mutex));

	return AUDIO_MANAGER_SUCCESS;
}

audio_manager_result_t reset_audio_stream_out(void)
{
	audio_card_info_t *card;

	if (g_actual_audio_out_card_id < 0) {
		meddbg("Found no active output audio card\n");
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}

	card = &g_audio_out_cards[g_actual_audio_out_card_id];
	pthread_mutex_lock(&(g_audio_out_cards[g_actual_audio_out_card_id].card_mutex));

	pcm_close(card->pcm);
	card->pcm = NULL;

	if (card->resample.necessary) {
		if (card->resample.buffer) {
			free(card->resample.buffer);
		}
		src_destroy(card->resample.handle);
	}

	card->config[card->device_id].status = AUDIO_CARD_IDLE;
	card->policy = AUDIO_MANAGER_STREAM_TYPE_MEDIA;

	pthread_mutex_unlock(&(card->card_mutex));

	return AUDIO_MANAGER_SUCCESS;
}

unsigned int get_input_frame_count(void)
{
	if (g_actual_audio_in_card_id < 0) {
		meddbg("No input audio card is active.\n");
		return 0;
	}

	return pcm_get_buffer_size(g_audio_in_cards[g_actual_audio_in_card_id].pcm);
}

unsigned int get_input_frames_to_byte(unsigned int frames)
{
	if ((g_actual_audio_in_card_id < 0) || (frames == 0)) {
		return 0;
	}

	return pcm_frames_to_bytes(g_audio_in_cards[g_actual_audio_in_card_id].pcm, frames);
}

unsigned int get_input_bytes_to_frame(unsigned int bytes)
{
	if ((g_actual_audio_in_card_id < 0) || (bytes == 0)) {
		return 0;
	}

	return pcm_bytes_to_frames(g_audio_in_cards[g_actual_audio_in_card_id].pcm, bytes);
}

unsigned int get_output_frame_count(void)
{
	if (g_actual_audio_out_card_id < 0) {
		meddbg("No output audio card is active.\n");
		return 0;
	}

	return pcm_get_buffer_size(g_audio_out_cards[g_actual_audio_out_card_id].pcm);
}

unsigned int get_output_frames_to_byte(unsigned int frames)
{
	if ((g_actual_audio_out_card_id < 0) || (frames == 0)) {
		return 0;
	}

	return pcm_frames_to_bytes(g_audio_out_cards[g_actual_audio_out_card_id].pcm, frames);
}

uint32_t get_output_bytes_to_frame(unsigned int bytes)
{
	if ((g_actual_audio_out_card_id < 0) || (bytes == 0)) {
		return 0;
	}

	return pcm_bytes_to_frames(g_audio_out_cards[g_actual_audio_out_card_id].pcm, bytes);
}

audio_manager_result_t get_max_audio_volume(uint8_t *volume)
{
	// ToDo: Add logics to consider 'decibel'.
	if (volume == NULL) {
		return AUDIO_MANAGER_INVALID_PARAM;
	}

	*volume = AUDIO_DEVICE_MAX_VOLUME;

	return AUDIO_MANAGER_SUCCESS;
}

audio_manager_result_t get_input_audio_gain(uint8_t *gain)
{
	int ret;
	int fd;
	char card_path[AUDIO_DEV_PATH_LENGTH + AUDIO_PCM_PATH_LENGTH];
	audio_card_info_t *card;
	audio_config_t *config;

	if (gain == NULL) {
		return AUDIO_MANAGER_INVALID_PARAM;
	}
	card = &g_audio_in_cards[g_actual_audio_in_card_id];
	get_card_path(card_path, card->card_id, card->device_id, INPUT);

	fd = open(card_path, O_RDONLY);
	if (fd < 0) {
		meddbg("Failed to open audio driver, path : %s errno : %d\n", card_path, errno);
		pthread_mutex_unlock(&card->card_mutex);
		return AUDIO_MANAGER_CARD_NOT_READY;
	}

	/* get the gain of the current card first */
	config = &card->config[card->device_id];
	ret = get_audio_volume(fd, config, INPUT);
	medvdbg("ret = %d, Max gain: %d, Current gain : %d card path : %s\n", ret, config->max_volume, config->volume, card_path);

	*gain = config->volume;

	close(fd);

	return ret;
}

audio_manager_result_t get_output_audio_volume(uint8_t *volume)
{
	int ret = AUDIO_MANAGER_SUCCESS;
	int fd;
	char card_path[AUDIO_DEV_PATH_LENGTH + AUDIO_PCM_PATH_LENGTH];
	audio_card_info_t *card;
	audio_config_t *config;

	if (volume == NULL) {
		return AUDIO_MANAGER_INVALID_PARAM;
	}
	card = &g_audio_out_cards[g_actual_audio_out_card_id];

	get_card_path(card_path, card->card_id, card->device_id, OUTPUT);

	fd = open(card_path, O_RDONLY);
	if (fd < 0) {
		meddbg("Failed to open audio driver, path : %s errno : %d\n", card_path, errno);
		return AUDIO_MANAGER_CARD_NOT_READY;
	}
	config = &card->config[card->device_id];

	/* get the volume of the current card first */
	ret = get_audio_volume(fd, config, OUTPUT);
	medvdbg("ret = %d, Max volume: %d, Current Volume : %d card path : %s\n", ret, config->max_volume, config->volume, card_path);

	*volume = config->volume;
	close(fd);
	return ret;
}

audio_manager_result_t set_input_audio_gain(uint8_t gain)
{
	return set_audio_volume(INPUT, gain);
}

audio_manager_result_t set_output_audio_volume(uint8_t volume)
{
	return set_audio_volume(OUTPUT, volume);
}

uint8_t get_process_type_audio_param_value(device_process_type_t type)
{
	switch (type) {
	case AUDIO_DEVICE_PROCESS_TYPE_NONE:
		return AUDIO_PU_UNDEF;
	case AUDIO_DEVICE_PROCESS_TYPE_STEREO_EXTENDER:
		return AUDIO_PU_STEREO_EXTENDER;
	case AUDIO_DEVICE_PROCESS_TYPE_SPEECH_DETECTOR:
		return AUDIO_PU_SPEECH_DETECT;
	default:
		return AUDIO_PU_UNDEF;
	}
}

uint8_t get_subprocess_type_audio_param_value(device_process_subtype_t type)
{
	switch (type) {
	case AUDIO_DEVICE_STEREO_EXTENDER_NONE:
		return AUDIO_STEXT_UNDEF;
	case AUDIO_DEVICE_STEREO_EXTENDER_ENABLE:
		return AUDIO_STEXT_ENABLE;
	case AUDIO_DEVICE_STEREO_EXTENDER_WIDTH:
		return AUDIO_STEXT_WIDTH;
	case AUDIO_DEVICE_STEREO_EXTENDER_UNDERFLOW:
		return AUDIO_STEXT_UNDERFLOW;
	case AUDIO_DEVICE_STEREO_EXTENDER_OVERFLOW:
		return AUDIO_STEXT_OVERFLOW;
	case AUDIO_DEVICE_STEREO_EXTENDER_LATENCY:
		return AUDIO_STEXT_LATENCY;
	case AUDIO_DEVICE_SPEECH_DETECT_NONE:
		return AUDIO_SD_UNDEF;
	case AUDIO_DEVICE_SPEECH_DETECT_EPD:
		return AUDIO_SD_ENDPOINT_DETECT;
	case AUDIO_DEVICE_SPEECH_DETECT_KD:
		return AUDIO_SD_KEYWORD_DETECT;
	case AUDIO_DEVICE_SPEECH_DETECT_NS:
		return AUDIO_SD_NS;
	case AUDIO_DEVICE_SPEECH_DETECT_CLEAR:
		return AUDIO_SD_CLEAR;
	default:
		return AUDIO_PU_UNDEF;
	}
}

static audio_manager_result_t get_supported_process_type(int fd, audio_config_t *config, audio_io_direction_t direct)
{
	struct audio_caps_desc_s caps_desc;
	pthread_mutex_t *card_mutex;

	if (direct == OUTPUT) {		// TODO add output logic in future.
		medvdbg("Do not support output for now...\n");
		return AUDIO_MANAGER_SUCCESS;
	} else {
		card_mutex = &g_audio_in_cards[g_actual_audio_in_card_id].card_mutex;
	}

	caps_desc.caps.ac_len = sizeof(struct audio_caps_s);
	caps_desc.caps.ac_type = AUDIO_TYPE_PROCESSING;
	caps_desc.caps.ac_subtype = AUDIO_PU_UNDEF;
	pthread_mutex_lock(card_mutex);
	if (ioctl(fd, AUDIOIOC_GETCAPS, (unsigned long)&caps_desc.caps) < 0) {
		meddbg("An ioctl error occurs\n");
		pthread_mutex_unlock(card_mutex);
		return AUDIO_MANAGER_DEVICE_FAIL;
	}

	config->device_process_type = caps_desc.caps.ac_controls.b[0];
	pthread_mutex_unlock(card_mutex);
	medvdbg("process type = 0x%x\n", config->device_process_type);

	return AUDIO_MANAGER_SUCCESS;
}

/* TODO below functions should be changed,
  * currently we using static array for cards, but below should be modified when we change to handler structure */
audio_manager_result_t find_stream_in_device_with_process_type(device_process_type_t type, device_process_subtype_t subtype, int *card_id, int *device_id)
{
	int i, j;
	int fd;
	uint8_t process_type;
	uint8_t subprocess_type;
	audio_manager_result_t ret;
	char path[AUDIO_DEVICE_FULL_PATH_LENGTH];
	struct audio_caps_desc_s caps_desc;
	audio_card_info_t *card;

	ret = init_audio_stream(INPUT);
	if (ret != AUDIO_MANAGER_SUCCESS) {
		meddbg("init stream failed\n");
		return ret;
	}

	process_type = get_process_type_audio_param_value(type);
	caps_desc.caps.ac_len = sizeof(struct audio_caps_s);
	caps_desc.caps.ac_type = AUDIO_TYPE_PROCESSING;
	caps_desc.caps.ac_subtype = process_type;

	for (j = 0; j < CONFIG_AUDIO_MAX_DEVICE_NUM; j++) {
		for (i = 0; i < CONFIG_AUDIO_MAX_INPUT_CARD_NUM; i++) {
			card = &g_audio_in_cards[i];

			/* If process type is matched */
			if ((card->config[j].device_process_type & process_type) != 0) {
				get_card_path(path, i, j, INPUT);
				pthread_mutex_lock(&(card->card_mutex));
				fd = open(path, O_RDONLY);
				if (fd < 0) {
					meddbg("open failed, path : %s process type : 0x%x\n", path, type);
					/* open failed but check next one anyway */
					pthread_mutex_unlock(&(card->card_mutex));
					continue;
				}

				if (ioctl(fd, AUDIOIOC_GETCAPS, (unsigned long)&caps_desc.caps) < 0) {
					meddbg("An ioctl error occurs\n");
					close(fd);
					/* ioctl failed, try next one */
					pthread_mutex_unlock(&(card->card_mutex));
					continue;
				}

				subprocess_type = get_subprocess_type_audio_param_value(subtype);

				/* Now we get subtype from h/w, compare it and subtype(param) */
				if ((subprocess_type & caps_desc.caps.ac_controls.b[0]) != 0) {
					*card_id = i;
					*device_id = j;
					close(fd);
					pthread_mutex_unlock(&(card->card_mutex));
					return AUDIO_MANAGER_SUCCESS;
				}
				close(fd);
				pthread_mutex_unlock(&(card->card_mutex));
			}
		}
	}

	meddbg("We didn't find any device with process type : %d subtype : %d process_type : %d subprocess_type : %d\n", type, subtype, process_type, subprocess_type);
	return AUDIO_MANAGER_DEVICE_NOT_SUPPORT;
}

audio_manager_result_t request_stream_in_device_process(int card_id, int device_id, int cmd)
{
	char path[AUDIO_DEVICE_FULL_PATH_LENGTH];
	int fd;
	audio_card_info_t *card;
	audio_config_t *config;
	int ret;

	card = &g_audio_in_cards[card_id];
	config = &card->config[device_id];

	/* Check card register state first */
	if (config->process_handler == NULL) {
		return AUDIO_MANAGER_CARD_NOT_READY;
	}

	get_card_path(path, card_id, device_id, INPUT);
	pthread_mutex_lock(&(card->card_mutex));
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		meddbg("Failed to open audio driver, path : %s errno : %d\n", path, errno);
		pthread_mutex_unlock(&(card->card_mutex));
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}

	ret = ioctl(fd, cmd, 0);
	close(fd);
	pthread_mutex_unlock(&(card->card_mutex));
	if (ret < 0) {
		meddbg("Failed to start process ret : %d\n", ret);
		return AUDIO_MANAGER_DEVICE_FAIL;
	}
	return AUDIO_MANAGER_SUCCESS;
}

audio_manager_result_t start_stream_in_device_process(int card_id, int device_id)
{
	return request_stream_in_device_process(card_id, device_id, AUDIOIOC_STARTPROCESS);
}

audio_manager_result_t stop_stream_in_device_process(int card_id, int device_id)
{
	audio_card_info_t *card;
	audio_manager_result_t result;
	struct audio_msg_s msg;
	int size;
	int prio;
	struct timespec st_time;

	card = &g_audio_in_cards[card_id];
	
	/* Stop First */
	result = request_stream_in_device_process(card_id, device_id, AUDIOIOC_STOPPROCESS);

	/* Consume all remained msg */
	if (result == AUDIO_MANAGER_SUCCESS) {
		do {
			clock_gettime(CLOCK_REALTIME, &st_time);
			size = mq_timedreceive(card->config[device_id].process_handler, (FAR char *)&msg, sizeof(msg), &prio, &st_time);
		} while (size > 0);
	}
	return result;
}

audio_manager_result_t register_stream_in_device_process_type(int card_id, int device_id, device_process_type_t type, device_process_subtype_t subtype)
{
	char path[AUDIO_DEVICE_FULL_PATH_LENGTH];
	int fd;
	int ret;
	audio_card_info_t *card;
	audio_config_t *config;
	uint8_t process_type;
	uint8_t subprocess_type;
	struct audio_caps_desc_s cap_desc;

	card = &g_audio_in_cards[card_id];
	config = &card->config[device_id];
	get_card_path(path, card_id, device_id, INPUT);

	pthread_mutex_lock(&(card->card_mutex));
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		meddbg("Failed to open audio driver, path : %s errno : %d\n", path, errno);
		pthread_mutex_unlock(&(card->card_mutex));
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}
	process_type = get_process_type_audio_param_value(type);

	subprocess_type = get_subprocess_type_audio_param_value(subtype);
	cap_desc.caps.ac_len = sizeof(struct audio_caps_s);
	cap_desc.caps.ac_type = process_type;
	cap_desc.caps.ac_subtype = subprocess_type;

	ret = ioctl(fd, AUDIOIOC_CONFIGURE, (unsigned long)&cap_desc);
	if (ret < 0) {
		if (ret == -EINVAL) {
			meddbg("Device doesn't support it!\n");
			ret = AUDIO_MANAGER_DEVICE_NOT_SUPPORT;
		} else {
			meddbg("ioctl failed ret : %d\n", ret);
			ret = AUDIO_MANAGER_OPERATION_FAIL;
		}
		close(fd);
		pthread_mutex_unlock(&(card->card_mutex));
		return ret;
	}
	close(fd);
	pthread_mutex_unlock(&(card->card_mutex));
	return AUDIO_MANAGER_SUCCESS;
}

audio_manager_result_t register_stream_in_device_process_handler(int card_id, int device_id, device_process_type_t type)
{
	char path[AUDIO_DEVICE_FULL_PATH_LENGTH];
	char mq_path[AUDIO_DEVICE_PROCESS_QUEUE_PATH_LENGTH];
	int fd;
	audio_card_info_t *card;
	audio_config_t *config;
	uint8_t process_type;
	int ret;
	struct mq_attr attr;

	card = &g_audio_in_cards[card_id];
	config = &card->config[device_id];
	get_card_path(path, card_id, device_id, INPUT);

	pthread_mutex_lock(&(card->card_mutex));
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		meddbg("Failed to open audio driver, path : %s errno : %d\n", path, errno);
		pthread_mutex_unlock(&(card->card_mutex));
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}
	process_type = get_process_type_audio_param_value(type);

	pthread_mutex_unlock(&(card->card_mutex));

	if ((get_supported_process_type(fd, config, INPUT) != AUDIO_MANAGER_SUCCESS) || (((config->device_process_type) & process_type) == 0)) {
		meddbg("Device doesn't support it\n");
		close(fd);
		return AUDIO_MANAGER_DEVICE_NOT_SUPPORT;
	}

	pthread_mutex_lock(&(card->card_mutex));

	/* Create a message queue */
	attr.mq_maxmsg = 16;
	attr.mq_msgsize = sizeof(struct audio_msg_s);
	attr.mq_curmsgs = 0;
	attr.mq_flags = 0;
	snprintf(mq_path, AUDIO_DEVICE_PROCESS_QUEUE_PATH_LENGTH, "%s%s", path, AUDIO_DEVICE_PROCESS_QUEUE_PATH);
	config->process_handler = mq_open(path, O_RDWR | O_CREAT, 0644, &attr);

	/* Register our message queue with the audio device */
	ret = ioctl(fd, AUDIOIOC_REGISTERPROCESS, (unsigned long)config->process_handler);
	close(fd);
	pthread_mutex_unlock(&(card->card_mutex));

	if (ret < 0) {
		if (ret == -EBUSY) {
			meddbg("Already Registered\n");
			ret = AUDIO_MANAGER_DEVICE_ALREADY_IN_USE;
		} else if (ret == -EINVAL) {
			meddbg("Device doesn't support it!\n");
			ret = AUDIO_MANAGER_DEVICE_NOT_SUPPORT;
		} else {
			meddbg("Register Handler Error : %d\n", ret);
			ret = AUDIO_MANAGER_DEVICE_FAIL;
		}
		return ret;
	}
	return AUDIO_MANAGER_SUCCESS;
}

audio_manager_result_t unregister_stream_in_device_process(int card_id, int device_id)
{
	char path[AUDIO_DEVICE_FULL_PATH_LENGTH];
	int ret;
	int fd;
	audio_card_info_t *card;
	audio_config_t *config;

	card = &g_audio_in_cards[card_id];
	config = &card->config[device_id];

	/* Check card register state first */
	if (config->process_handler == NULL) {
		meddbg("Handler is not registered\n");
		return AUDIO_MANAGER_INVALID_DEVICE;
	}
	get_card_path(path, card_id, device_id, INPUT);

	pthread_mutex_lock(&(card->card_mutex));
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		meddbg("Failed to open audio driver, path : %s errno : %d\n", path, errno);
		pthread_mutex_unlock(&(card->card_mutex));
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}

	/* Register our message queue with the audio device */
	ret = ioctl(fd, AUDIOIOC_UNREGISTERPROCESS, 0);
	close(fd);
	pthread_mutex_unlock(&(card->card_mutex));
	if (ret < 0) {
		meddbg("Unregister Failed : %d\n", ret);
		return AUDIO_MANAGER_OPERATION_FAIL;
	}
	return AUDIO_MANAGER_SUCCESS;

}

audio_manager_result_t get_device_process_handler_message(int card_id, int device_id, uint16_t *msgId)
{
	struct audio_msg_s msg;
	int prio;
	ssize_t size = 0;
	struct timespec st_time;
	audio_card_info_t *card;

	card = &g_audio_in_cards[card_id];
	if (card->config[device_id].process_handler == NULL) {
		meddbg("Device doesn't registered process handler\n");
		return AUDIO_MANAGER_INVALID_DEVICE;
	}
	clock_gettime(CLOCK_REALTIME, &st_time);
	size = mq_timedreceive(card->config[device_id].process_handler, (FAR char *)&msg, sizeof(msg), &prio, &st_time);

	if (size != sizeof(msg)) {
		medvdbg("wrong message id : %ld\n", msg.msgId);
		return AUDIO_MANAGER_OPERATION_FAIL;
	} else {
		switch (msg.msgId) {
		case AUDIO_MSG_EPD:
			*msgId = (uint16_t) AUDIO_DEVICE_SPEECH_DETECT_EPD;
			break;
		case AUDIO_MSG_KD:
			*msgId = (uint16_t) AUDIO_DEVICE_SPEECH_DETECT_KD;
			break;
		default:
			*msgId = (uint16_t) AUDIO_DEVICE_SPEECH_DETECT_NONE;
			break;
		}
		medvdbg("size : %d sizeofmsg : %d id : %ld\n", size, sizeof(msg), msg.msgId);
	}
	return AUDIO_MANAGER_SUCCESS;

}

audio_manager_result_t change_stream_device(int card_id, int device_id, audio_io_direction_t direct)
{
	char path[AUDIO_DEVICE_FULL_PATH_LENGTH];
	struct stat st;
	audio_card_info_t *card;
	audio_manager_result_t ret;
	int status;

	/* update card first, this can be called before actual card set */
	ret = init_audio_stream(direct);
	if (ret != AUDIO_MANAGER_SUCCESS) {
		meddbg("init stream failed\n");
		return ret;
	}

	if (direct == INPUT) {
		card = &g_audio_in_cards[card_id];
	} else {
		card = &g_audio_out_cards[card_id];
	}

	/* Check current card status first */
	status = card->config[card->device_id].status;
	if (status != AUDIO_CARD_IDLE) {
		meddbg("card status is not IDLE status : %d card_id : %d device_id : %d\n", status, card_id, device_id);
		return AUDIO_MANAGER_CARD_NOT_READY;
	}

	get_card_path(path, card_id, device_id, direct);

	/* check target path is available as a driver path */
	if ((stat(path, &st) != OK) && (errno == ENOENT)) {
		meddbg("no available stream device path : %s\n", path);
		return AUDIO_MANAGER_INVALID_DEVICE;
	}
	pthread_mutex_lock(&(card->card_mutex));

	/* update new card */
	if (direct == INPUT) {
		g_actual_audio_in_card_id = card_id;
		card = &g_audio_in_cards[card_id];
	} else {
		g_actual_audio_out_card_id = card_id;
		card = &g_audio_out_cards[card_id];
	}

	card->card_id = card_id;
	card->device_id = device_id;

	/* update state as same as previous state */
	card->config[card->device_id].status = AUDIO_CARD_IDLE;

	pthread_mutex_unlock(&(card->card_mutex));
	medvdbg("Card changed path  : %s\n", path);
	return AUDIO_MANAGER_SUCCESS;
}

audio_manager_result_t change_stream_in_device(int card_id, int device_id)
{
	if (card_id >= CONFIG_AUDIO_MAX_INPUT_CARD_NUM) {
		meddbg("invalid card id : %d\n", card_id);
		return AUDIO_MANAGER_INVALID_PARAM;
	}

	if (device_id >= CONFIG_AUDIO_MAX_DEVICE_NUM) {
		meddbg("invalid device id : %d\n", device_id);
		return AUDIO_MANAGER_INVALID_PARAM;
	}

	if ((card_id == g_actual_audio_in_card_id) && device_id == (g_audio_in_cards[g_actual_audio_in_card_id].device_id)) {
		meddbg("Same card & device id card_id : %d device_id : %d\n", card_id, device_id);
		return AUDIO_MANAGER_DEVICE_ALREADY_IN_USE;
	}
	return change_stream_device(card_id, device_id, INPUT);
}

audio_manager_result_t change_stream_out_device(int card_id, int device_id)
{
	if (card_id >= CONFIG_AUDIO_MAX_OUTPUT_CARD_NUM) {
		meddbg("invalid card id : %d\n", card_id);
		return AUDIO_MANAGER_INVALID_PARAM;
	}

	if (device_id >= CONFIG_AUDIO_MAX_DEVICE_NUM) {
		meddbg("invalid device id : %d\n", device_id);
		return AUDIO_MANAGER_INVALID_PARAM;
	}

	if ((card_id == g_actual_audio_out_card_id) && (device_id == g_audio_out_cards[g_actual_audio_out_card_id].device_id)) {
		meddbg("Same card & device id card_id : %d device_id : %d\n", card_id, device_id);
		return AUDIO_MANAGER_DEVICE_ALREADY_IN_USE;
	}

	return change_stream_device(card_id, device_id, OUTPUT);
}

/* TODO policy should be merged logic of focus manager */
audio_manager_result_t set_stream_policy(audio_manager_stream_policy_t policy, audio_io_direction_t direct)
{
	audio_card_info_t *card;

	if (direct == INPUT) {
		card = &g_audio_in_cards[g_actual_audio_in_card_id];
	} else {
		card = &g_audio_out_cards[g_actual_audio_out_card_id];
	}

	/* TODO please consider that we should provides it before stream structure created?? or called in media's prepare.... */
	if (card->config[card->device_id].status == AUDIO_CARD_NONE) {
		meddbg("Failed to set stream policy, card is not available\n");
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}

	if (policy < card->policy) {
		meddbg("Failed to set stream policy : %d, previous is more high : %d\n", policy, card->policy);
		return AUDIO_MANAGER_SET_STREAM_POLICY_NOT_ALLOWED;
	}

	/* TODO Consider that reset stream & set values(channel, buffer size) based on policy */
	pthread_mutex_lock(&(card->card_mutex));
	card->policy = policy;
	pthread_mutex_unlock(&(card->card_mutex));
	medvdbg("stream policy set : %d\n", policy);
	return AUDIO_MANAGER_SUCCESS;
}

audio_manager_result_t get_stream_policy(audio_manager_stream_policy_t *policy, audio_io_direction_t direct)
{
	audio_card_info_t *card;

	if (direct == INPUT) {
		card = &g_audio_in_cards[g_actual_audio_in_card_id];
	} else {
		card = &g_audio_out_cards[g_actual_audio_out_card_id];
	}

	if (card->config[card->device_id].status == AUDIO_CARD_NONE) {
		meddbg("Failed to get stream policy, card is not available\n");
		return AUDIO_MANAGER_NO_AVAIL_CARD;
	}

	*policy = card->policy;

	return AUDIO_MANAGER_SUCCESS;
}

audio_manager_result_t set_stream_in_policy(audio_manager_stream_policy_t policy)
{
	return set_stream_policy(policy, INPUT);
}

audio_manager_result_t set_stream_out_policy(audio_manager_stream_policy_t policy)
{
	return set_stream_policy(policy, OUTPUT);
}

audio_manager_result_t get_stream_in_policy(audio_manager_stream_policy_t *policy)
{
	return get_stream_policy(policy, INPUT);
}

audio_manager_result_t get_stream_out_policy(audio_manager_stream_policy_t *policy)
{
	return get_stream_policy(policy, OUTPUT);
}

void print_audio_card_info(audio_io_direction_t direct)
{
#if defined(CONFIG_DEBUG) && defined(CONFIG_DEBUG_MEDIA)
	audio_card_info_t *card;
	audio_card_status_t status;
	int i, j, max_card, actual_card_id;
	char path[AUDIO_DEVICE_FULL_PATH_LENGTH];

	if (direct == INPUT) {
		max_card = CONFIG_AUDIO_MAX_OUTPUT_CARD_NUM;
		actual_card_id = g_actual_audio_in_card_id;
	} else {
		max_card = CONFIG_AUDIO_MAX_INPUT_CARD_NUM;
		actual_card_id = g_actual_audio_out_card_id;
	}

	for (j = 0; j < CONFIG_AUDIO_MAX_DEVICE_NUM; j++) {
		for (i = 0; i < max_card; i++) {
			if (direct == INPUT) {
				card = &g_audio_in_cards[i];
			} else {
				card = &g_audio_out_cards[i];
			}
			status = card->config[j].status;
			if (status != AUDIO_CARD_NONE) {
				get_card_path(path, i, j, direct);

				dbg_noarg("\nDevice Path : %s\n", path);
				dbg_noarg("Card Number : %d\t Device Number : %d\t", i, j);
				if ((card->card_id == actual_card_id) && (j == card->device_id)) {
					dbg_noarg("---> Actual Card\n");
				} else {
					dbg_noarg("---> Not using now\n");
				}
				dbg_noarg("Status : ");
				switch (status) {
				case AUDIO_CARD_NONE:
					dbg_noarg("%s(%d)\n", "AUDIO_CARD_NONE", AUDIO_CARD_NONE);
					break;
				case AUDIO_CARD_IDLE:
					dbg_noarg("%s(%d)\n", "AUDIO_CARD_IDLE", AUDIO_CARD_IDLE);
					break;
				case AUDIO_CARD_PAUSE:
					dbg_noarg("%s(%d)\n", "AUDIO_CARD_PAUSE", AUDIO_CARD_PAUSE);
					break;
				case AUDIO_CARD_READY:
					dbg_noarg("%s(%d)\n", "AUDIO_CARD_READY", AUDIO_CARD_READY);
					break;
				case AUDIO_CARD_RUNNING:
					dbg_noarg("%s(%d)\n", "AUDIO_CARD_RUNNING", AUDIO_CARD_RUNNING);
					break;
				default:
					dbg_noarg("%s(%d)\n", "ERROR!!", AUDIO_MANAGER_DEVICE_FAIL);
					break;
				}

				dbg_noarg("Volume : %d Max Volume : %d\n", card->config[j].volume, card->config[j].max_volume);
				dbg_noarg("Supports Process Type : ");
				if ((card->config[j].device_process_type & AUDIO_DEVICE_PROCESS_TYPE_STEREO_EXTENDER) != 0) {
					dbg_noarg("%s(%x)\t", "AUDIO_DEVICE_PROCESS_TYPE_STEREO_EXTENDER", AUDIO_DEVICE_PROCESS_TYPE_STEREO_EXTENDER);
				}

				if ((card->config[j].device_process_type & AUDIO_DEVICE_PROCESS_TYPE_SPEECH_DETECTOR) != 0) {
					dbg_noarg("%s(%x)\t", "AUDIO_DEVICE_PROCESS_TYPE_SPEECH_DETECTOR", AUDIO_DEVICE_PROCESS_TYPE_SPEECH_DETECTOR);
				}

				if ((card->config[j].device_process_type & AUDIO_DEVICE_PROCESS_TYPE_NONE) != 0) {
					dbg_noarg("%s(%x)\t", "AUDIO_DEVICE_PROCESS_TYPE_NONE", AUDIO_DEVICE_PROCESS_TYPE_NONE);
				}
				dbg_noarg("\n");

				if (status != AUDIO_CARD_IDLE) {
					dbg_noarg("Channel :%d\nSampleRate :%d\nPcmFormat : %d\nBuffer Frame Size : %d\n\n", pcm_get_channels(card->pcm), pcm_get_rate(card->pcm), pcm_get_format(card->pcm), pcm_get_buffer_size(card->pcm));
				}
			}
		}
	}
#endif
}

void dump_audio_card_info()
{
	print_audio_card_info(INPUT);
	print_audio_card_info(OUTPUT);
}
