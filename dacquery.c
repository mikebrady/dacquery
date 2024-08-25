/*
 * Copyright (c) Mike Brady 2024
 * All rights reserved.
 * Mike Brady <4265913+mikebrady@users.noreply.github.com>

 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "dacquery.h"
#include <alsa/asoundlib.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h> /* Definition of AT_* constants */
#include <getopt.h>
#include <grp.h>
#include <math.h>
#include <poll.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int display_extended_information = 0;
// int include_mixers_with_capture = 0;

// this dummy function is used to keep alsa subsystem error messages quiet
static void
snd_error_quiet(__attribute__((unused)) const char *file, __attribute__((unused)) int line,
                __attribute__((unused)) const char *func, __attribute__((unused)) int err,
                __attribute__((unused)) const char *fmt, __attribute__((unused)) va_list arg) {
  // return NULL;
}

#define MIXER_BUNDLE_SIZE 32

typedef struct {
  char name[64];
  unsigned int index;
  long minv, maxv, mindecibels, maxdecibels; // the min values are for non-muting
  int has_a_decibel_range;
  int lowest_value_is_mute;
} mixer_info_t;

typedef struct {
  mixer_info_t mixer[MIXER_BUNDLE_SIZE];
  size_t size;
  size_t first_free;
} mixer_bundle_t;

static int process_mixers(char *device_name, mixer_bundle_t *mixer_bundle) {
  int result = 0;
  snd_mixer_t *handle;
  snd_mixer_selem_id_t *sid;
  snd_mixer_elem_t *elem;
  snd_mixer_selem_id_alloca(&sid);
  // long min_db, max_db;
  debug(3, "process_mixers on device \"%s\".", device_name);
  if ((result = snd_mixer_open(&handle, 0)) == 0) {
    if ((result = snd_mixer_attach(handle, device_name)) == 0) {
      if ((result = snd_mixer_selem_register(handle, NULL, NULL)) == 0) {
        if ((result = snd_mixer_load(handle)) == 0) {
          for (elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem)) {
            if (snd_mixer_selem_is_active(elem)) {

              /*
                            int has_capture_elements = snd_mixer_selem_has_common_volume(elem) ||
                                                       snd_mixer_selem_has_capture_volume(elem) ||
                                                       snd_mixer_selem_has_common_switch(elem) ||
                                                       snd_mixer_selem_has_capture_switch(elem);

                            if (((include_mixers_with_capture == 1) && has_capture_elements) ||
                                 ((include_mixers_with_capture == 0) && (!has_capture_elements))) {
              */
              // if (1) {
              // if (snd_mixer_selem_has_playback_volume(elem) ||
              // snd_mixer_selem_has_common_volume(elem)) {
              if (snd_mixer_selem_has_playback_volume(elem) &&
                  (snd_mixer_selem_is_enumerated(elem) == 0)) {

                strncpy(mixer_bundle->mixer[mixer_bundle->first_free].name,
                        snd_mixer_selem_get_name(elem),
                        sizeof(mixer_bundle->mixer[mixer_bundle->first_free].name) - 1);
                mixer_bundle->mixer[mixer_bundle->first_free].index =
                    snd_mixer_selem_get_index(elem);
                if (snd_mixer_selem_get_playback_volume_range(
                        elem, &mixer_bundle->mixer[mixer_bundle->first_free].minv,
                        &mixer_bundle->mixer[mixer_bundle->first_free].maxv) < 0)
                  debug(1, "Can't read mixer's [linear] min and max volumes.");

                if (snd_mixer_selem_get_playback_dB_range(
                        elem, &mixer_bundle->mixer[mixer_bundle->first_free].mindecibels,
                        &mixer_bundle->mixer[mixer_bundle->first_free].maxdecibels) == 0) {
                  mixer_bundle->mixer[mixer_bundle->first_free].has_a_decibel_range = 1;
                  if (mixer_bundle->mixer[mixer_bundle->first_free].mindecibels ==
                      SND_CTL_TLV_DB_GAIN_MUTE) {
                    // For instance, the Raspberry Pi does this
                    debug(1, "Lowest dB value is a mute");
                    mixer_bundle->mixer[mixer_bundle->first_free].lowest_value_is_mute = 1;
                    // mixer_bundle->mixer[mixer_bundle->first_free].minv++;
                    if (snd_mixer_selem_ask_playback_vol_dB(
                            elem, mixer_bundle->mixer[mixer_bundle->first_free].minv + 1,
                            &mixer_bundle->mixer[mixer_bundle->first_free].mindecibels) != 0)
                      debug(1, "Can't get dB value corresponding to a minimum volume "
                               "+ 1.");
                  } else {
                    mixer_bundle->mixer[mixer_bundle->first_free].lowest_value_is_mute = 0;
                  }
                  // inform("Mixer name: \"%s\",%d%*sRange: %6.2f dB, max: %6.2f dB, min: %6.2f dB",
                  //        snd_mixer_selem_get_name(elem), snd_mixer_selem_get_index(elem),
                  //        // 14 - strlen(snd_mixer_selem_get_name(elem)),
                  //        1, " ", (max_db - min_db) * 0.01, max_db * 0.01, min_db * 0.01);
                } else {
                  mixer_bundle->mixer[mixer_bundle->first_free].has_a_decibel_range = 0;
                  mixer_bundle->mixer[mixer_bundle->first_free].lowest_value_is_mute = 0;
                }
                mixer_bundle->first_free++;
              }
            }
          }

        } else {
          debug(1, "mixer load error -- error %d (\"%s\") on device \"%s\".", result,
                snd_strerror(result), device_name);
        }
      } else {
        debug(1, "mixer register error -- error %d (\"%s\") on device \"%s\".", result,
              snd_strerror(result), device_name);
      }
    } else {
      debug(1, "can't attach mixer -- error %d (\"%s\") on device \"%s\".", result,
            snd_strerror(result), device_name);
    }
    snd_mixer_close(handle);
  } else {
    debug(1, "can't open mixer -- error %d (\"%s\") on device \"%s\".", result,
          snd_strerror(result), device_name);
  }
  return result;
}

snd_pcm_t *alsa_handle = NULL;
snd_pcm_hw_params_t *alsa_params = NULL;
snd_pcm_sw_params_t *alsa_swparams = NULL;
int frame_size; // in bytes for interleaved stereo

void get_channel_map(snd_pcm_t *alsa_handle, char *channel_map_store) {
  if (channel_map_store != NULL) {
    channel_map_store[0] = '\0'; // default
    if (alsa_handle != NULL) {
      snd_pcm_chmap_t *channel_map = snd_pcm_get_chmap(alsa_handle);
      if (channel_map) {
        unsigned int i;
        for (i = 0; i < channel_map->channels; i++) {
          debug(3, "channel %d is %d, name: \"%s\", long name: \"%s\".", i, channel_map->pos[i],
                snd_pcm_chmap_name(channel_map->pos[i]),
                snd_pcm_chmap_long_name(channel_map->pos[i]));
        }
        if (snd_pcm_chmap_print(channel_map, sizeof(char[128]), channel_map_store) < 0)
          channel_map_store[0] = '\0'; // if there's any problem
        debug(3,
              "channel count: %d, channel name list: "
              "\"%s\".",
              channel_map->channels, channel_map_store);

        free(channel_map);
      } else {
        debug(3, "no channel map.");
      }
    }
  } else {
    debug(1, "no memory allocated for a channel map store");
  }
}

// can have up to 32 rates
static unsigned int rates_to_check[] = {5512,  8000,  11025, 16000,  22050,  32000,  44100, 48000,
                                        64000, 88200, 96000, 176400, 192000, 352800, 384000};

static snd_pcm_format_t formats_to_check[] = {SND_PCM_FORMAT_S8,
                                              SND_PCM_FORMAT_U8,
                                              SND_PCM_FORMAT_S16_LE,
                                              SND_PCM_FORMAT_S16_BE,
                                              SND_PCM_FORMAT_U16_LE,
                                              SND_PCM_FORMAT_U16_BE,
                                              SND_PCM_FORMAT_S24_LE,
                                              SND_PCM_FORMAT_S24_BE,
                                              SND_PCM_FORMAT_U24_LE,
                                              SND_PCM_FORMAT_U24_BE,
                                              SND_PCM_FORMAT_S32_LE,
                                              SND_PCM_FORMAT_S32_BE,
                                              SND_PCM_FORMAT_U32_LE,
                                              SND_PCM_FORMAT_U32_BE,
                                              SND_PCM_FORMAT_FLOAT_LE,
                                              SND_PCM_FORMAT_FLOAT_BE,
                                              SND_PCM_FORMAT_FLOAT64_LE,
                                              SND_PCM_FORMAT_FLOAT64_BE,
                                              SND_PCM_FORMAT_IEC958_SUBFRAME_LE,
                                              SND_PCM_FORMAT_IEC958_SUBFRAME_BE,
                                              SND_PCM_FORMAT_MU_LAW,
                                              SND_PCM_FORMAT_A_LAW,
                                              SND_PCM_FORMAT_IMA_ADPCM,
                                              SND_PCM_FORMAT_MPEG,
                                              SND_PCM_FORMAT_GSM,
                                              SND_PCM_FORMAT_SPECIAL,
                                              SND_PCM_FORMAT_S24_3LE,
                                              SND_PCM_FORMAT_S24_3BE,
                                              SND_PCM_FORMAT_U24_3LE,
                                              SND_PCM_FORMAT_U24_3BE,
                                              SND_PCM_FORMAT_S20_3LE,
                                              SND_PCM_FORMAT_S20_3BE,
                                              SND_PCM_FORMAT_U20_3LE,
                                              SND_PCM_FORMAT_U20_3BE,
                                              SND_PCM_FORMAT_S18_3LE,
                                              SND_PCM_FORMAT_S18_3BE,
                                              SND_PCM_FORMAT_U18_3LE,
                                              SND_PCM_FORMAT_U18_3BE,
                                              SND_PCM_FORMAT_G723_24,
                                              SND_PCM_FORMAT_G723_24_1B,
                                              SND_PCM_FORMAT_G723_40,
                                              SND_PCM_FORMAT_G723_40_1B,
                                              SND_PCM_FORMAT_DSD_U8,
                                              SND_PCM_FORMAT_DSD_U16_LE,
                                              SND_PCM_FORMAT_DSD_U32_LE,
                                              SND_PCM_FORMAT_DSD_U16_BE,
                                              SND_PCM_FORMAT_DSD_U32_BE};

typedef struct {
  uint32_t rate_set, channel_set;
  uint64_t format_set;
  char channel_mappings[32][128];
} configuration_set;

typedef struct {
  configuration_set *configuration_sets; // this will be a malloced array of type configuration_set
  size_t configuration_sets_count; // the size of the array. Not all the elements will be valid!
  int error_status;
  int already_handled;
  char interface_name[64];
  char device_name[64];
  char subdevice_name[64];
  unsigned int card_number;
  unsigned int device_number;
  unsigned int subdevice_number;
} configuration_bundle;

// if the new configuration can be added to an existing configuration set
// i.e. same format set and same channel set but a new rate, then add it in

// otherwise add a new configuration set

void add_to_configuration_sets(unsigned int channel_count, uint32_t rate_index, uint32_t format_set,
                               char *channel_map, configuration_bundle *configuration) {
  if (configuration->configuration_sets_count == 0) {
    configuration->configuration_sets = malloc(sizeof(configuration_set));
    configuration->configuration_sets[0].rate_set = rate_index;
    configuration->configuration_sets[0].format_set = format_set;
    configuration->configuration_sets[0].channel_set = (1 << channel_count);
    memset(configuration->configuration_sets[0].channel_mappings, 0, sizeof(char[128]) * 32);
    if (channel_map != NULL)
      strncpy(configuration->configuration_sets[0].channel_mappings[channel_count], channel_map,
              sizeof(char[128]));
    configuration->configuration_sets_count = 1;
  } else {
    // check each configuration set in turn to see if they can be merged
    unsigned int i = 0;
    int can_be_merged = 0;
    while ((i < configuration->configuration_sets_count) && (can_be_merged == 0)) {
      if ((configuration->configuration_sets[i].channel_set == (uint32_t)(1 << channel_count)) &&
          (configuration->configuration_sets[i].format_set == format_set)) {
        // now see if the channel maps are compatible
        if ((configuration->configuration_sets[i].channel_mappings[channel_count][0] == '\0') &&
            (channel_map == NULL)) {
          can_be_merged = 1;
        } else if ((channel_map != NULL) &&
                   (strcmp(configuration->configuration_sets[i].channel_mappings[channel_count],
                           channel_map) == 0)) {
          can_be_merged = 1;
        }
      }
      if (can_be_merged != 0) {
        configuration->configuration_sets[i].rate_set |= rate_index;
      } else {
        i++;
      }
    }
    if (can_be_merged == 0) {
      // debug(1,"added");
      configuration->configuration_sets =
          realloc(configuration->configuration_sets,
                  sizeof(configuration_set) * (configuration->configuration_sets_count + 1));
      configuration->configuration_sets[configuration->configuration_sets_count].rate_set =
          rate_index;
      configuration->configuration_sets[configuration->configuration_sets_count].format_set =
          format_set;
      configuration->configuration_sets[configuration->configuration_sets_count].channel_set =
          (1 << channel_count);
      memset(configuration->configuration_sets[configuration->configuration_sets_count]
                 .channel_mappings,
             0, sizeof(char[128]) * 32);
      strncpy(configuration->configuration_sets[configuration->configuration_sets_count]
                  .channel_mappings[channel_count],
              channel_map, sizeof(char[128]));
      configuration->configuration_sets_count++;
    }
  }
}

static configuration_bundle *get_permissible_configuration_settings(char *interface_name,
                                                                    snd_pcm_info_t *pcminfo) {
  debug(1, "get_permissible_configuration_settings for \"%s\".", interface_name);
  // can have up to 31 channels
  uint32_t possible_channel_mask = 0;
  unsigned int possible_channel_count = 0;

  uint32_t possible_rate_mask = 0;
  unsigned int possible_rate_count = 0;

  uint64_t possible_format_mask = 0;
  unsigned int possible_format_count = 0;
  int ret = 0;
  configuration_bundle *configuration = malloc(sizeof(configuration_bundle));
  if (configuration != NULL) {
    memset(configuration, 0, sizeof(configuration_bundle));
    strncpy(configuration->interface_name, interface_name,
            sizeof(configuration->interface_name) - 1);
    strncpy(configuration->device_name, snd_pcm_info_get_name(pcminfo),
            sizeof(configuration->device_name) - 1);
    strncpy(configuration->subdevice_name, snd_pcm_info_get_subdevice_name(pcminfo),
            sizeof(configuration->subdevice_name) - 1);

    snd_pcm_hw_params_t *local_alsa_params = NULL;
    snd_pcm_hw_params_alloca(&local_alsa_params);
    ret = snd_pcm_open(&alsa_handle, interface_name, SND_PCM_STREAM_PLAYBACK, 0);
    if (ret == 0) {
      // check what numbers of channels the device can provide...
      unsigned int i;
      for (i = 1; i <= 8; i++) {
        snd_pcm_hw_free(alsa_handle); // remove any previous configurations
        int local_response = snd_pcm_hw_params_any(alsa_handle, local_alsa_params);
        if (local_response == 0) {
          local_response = snd_pcm_hw_params_test_channels(alsa_handle, local_alsa_params, i);
          if (local_response == 0) {
            possible_channel_mask |= (1 << i);
            possible_channel_count++;
            debug(3, "\"%s\" can handle %u channels.", interface_name, i);
          } else {
            debug(3, "\"%s\" can not handle %u channels.", interface_name, i);
          }
        }
      }

      if (ret == 0) {
        // check what rates the device can handle
        for (i = 0; i < sizeof(rates_to_check) / sizeof(unsigned int); i++) {
          snd_pcm_hw_free(alsa_handle); // remove any previous configurations
          int local_response = snd_pcm_hw_params_any(alsa_handle, local_alsa_params);
          if (local_response == 0) {
            // We don't't use snd_pcm_hw_params_test_rate() here because it its too strict, it
            // seems. It excludes situations where the rate is nominally the requested rate but
            // might be a bit higher or lower, as indicated by the dir value returned when you use
            // snd_pcm_hw_params_set_rate_near().
            unsigned int actual_sample_rate = rates_to_check[i];
            int dir = 0;

            local_response = snd_pcm_hw_params_set_rate_near(alsa_handle, local_alsa_params,
                                                             &actual_sample_rate, &dir);
            // a returned dir value of 0 would mean exact rate only,
            // -1 means the rate chosen will be less, and +1 greater.
            // however, we also check that the nominal actual returned rate is the same.
            if ((local_response == 0) && (actual_sample_rate == rates_to_check[i])) {
              possible_rate_mask |= (1 << i);
              possible_rate_count++;
              debug(3, "\"%s\" can handle %u fps, dir: %d.", interface_name, rates_to_check[i],
                    dir);
            } else {
              debug(3, "\"%s\" can not handle %u fps.", interface_name, rates_to_check[i]);
            }
          }
        }

        if (ret == 0) {
          // check what formats the device can handle
          for (i = 0; i < sizeof(formats_to_check) / sizeof(snd_pcm_format_t); i++) {
            snd_pcm_hw_free(alsa_handle); // remove any previous configurations
            int local_response = snd_pcm_hw_params_any(alsa_handle, local_alsa_params);
            if (local_response == 0) {
              local_response = snd_pcm_hw_params_test_format(alsa_handle, local_alsa_params,
                                                             formats_to_check[i]);
              if (local_response == 0) {
                possible_format_mask |= (1 << i);
                possible_format_count++;
                debug(3, "\"%s\" can accept the %s format.", interface_name,
                      snd_pcm_format_name(formats_to_check[i]));
              } else {
                debug(3, "\"%s\" can not accept the %s format.", interface_name,
                      snd_pcm_format_name(formats_to_check[i]));
              }
            }
          }
        }
      }

      // now we know the maximum possible number of configurations
      // so let's check them out
      // first, let's initialise the results store

      if (ret == 0) {
        int local_response = -EINVAL;
        char local_channel_map_store[128];
        char channel_map_store[128] = "";
        unsigned int ci; // channel index
        uint32_t format_set = 0;
        for (ci = 1; ci <= 8; ci++) {
          if ((possible_channel_mask & (1 << ci)) !=
              0) { // if this channel count is among the channel counts that could be used...
            unsigned int ri; // rate index
            for (ri = 0; ri < sizeof(rates_to_check) / sizeof(unsigned int); ri++) {
              if ((possible_rate_mask & (1 << ri)) !=
                  0) { // if this rate is among the rates that could be used...
                format_set = 0;
                unsigned int fi; // format index
                for (fi = 0; fi < sizeof(formats_to_check) / sizeof(snd_pcm_format_t); fi++) {

                  // now check if this combination of formats is allowed
                  if ((possible_format_mask & (1 << fi)) !=
                      0) { // if this format is among the formats that could be used...
                    // now check if this specific combination of channel/rate/format is allowed
                    memset(local_alsa_params, 0, snd_pcm_hw_params_sizeof());
                    snd_pcm_hw_free(alsa_handle); // remove any previous configurations
                    snd_pcm_hw_params_any(alsa_handle, local_alsa_params);

                    local_response = snd_pcm_hw_params_any(alsa_handle, local_alsa_params);
                    if (local_response == 0) {
                      if ((snd_pcm_hw_params_set_access(alsa_handle, local_alsa_params,
                                                        SND_PCM_ACCESS_RW_INTERLEAVED) == 0) ||
                          (snd_pcm_hw_params_set_access(alsa_handle, local_alsa_params,
                                                        SND_PCM_ACCESS_MMAP_INTERLEAVED) == 0)) {
                        local_response = snd_pcm_hw_params_set_channels(
                            alsa_handle, local_alsa_params,
                            ci); // the channel index is the channel count too
                        if (local_response == 0) {
                          local_response = snd_pcm_hw_params_set_format(
                              alsa_handle, local_alsa_params, formats_to_check[fi]);
                          if (local_response == 0) {
                            unsigned int actual_sample_rate = rates_to_check[ri];
                            int dir = 0;
                            local_response = snd_pcm_hw_params_set_rate_near(
                                alsa_handle, local_alsa_params, &actual_sample_rate, &dir);
                            if (local_response == 0) {
                              if (actual_sample_rate != rates_to_check[ri]) {
                                local_response = -EINVAL;
                                debug(3,
                                      "Sample rate set, %u, is different to sample rate requested, "
                                      "%u.",
                                      actual_sample_rate, rates_to_check[ri]);
                              } else {
                                // success -- this combination of channel ci, rate ri and format fi
                                // works

                                debug(3, "\"%s\": snd_pcm_hw_params for  %u/%s/%u.", interface_name,
                                      rates_to_check[ri], snd_pcm_format_name(formats_to_check[fi]),
                                      ci);
                                local_response = snd_pcm_hw_params(alsa_handle, local_alsa_params);
                                if (local_response == 0) {
                                  get_channel_map(alsa_handle, (char *)&local_channel_map_store);
                                  if (local_channel_map_store[0] == '\0') {
                                    debug(3, "\"%s\": %u/%s/%u/", interface_name,
                                          rates_to_check[ri],
                                          snd_pcm_format_name(formats_to_check[fi]), ci);
                                  } else {
                                    debug(3, "\"%s\": %u/%s/%u/<%s>", interface_name,
                                          rates_to_check[ri],
                                          snd_pcm_format_name(formats_to_check[fi]), ci,
                                          local_channel_map_store);
                                  }

                                  // here, we know that this new format works with the given rate
                                  // and channel count if the format set is empty, then we should
                                  // store the channel map, if any if the format set is non-empty,
                                  // then we should check that the channel maps are the same and if
                                  // they are different, we should add the current configuration set
                                  // and start a new one

                                  if (format_set == 0) {
                                    format_set |= (1 << fi);
                                    strncpy(channel_map_store, local_channel_map_store,
                                            sizeof(channel_map_store));
                                  } else {
                                    int different = 0;
                                    if (strcmp(local_channel_map_store, channel_map_store) != 0) {
                                      different = 1;
                                    }
                                    if (different != 0) {
                                      debug(1, "found to be different");
                                      add_to_configuration_sets(ci, (1 << ri), format_set,
                                                                channel_map_store, configuration);
                                      format_set = (1 << fi);
                                      strncpy(channel_map_store, local_channel_map_store,
                                              sizeof(channel_map_store));
                                    } else {
                                      format_set |= (1 << fi);
                                    }
                                  }
                                } else {
                                  debug(3,
                                        "Unable to set hw parameters for device \"%s\": %d: "
                                        "\"%s\".%s",
                                        interface_name, local_response,
                                        snd_strerror(local_response),
                                        local_response == -ENOSPC
                                            ? "  This seems to be a USB error and may be caused by "
                                              "an "
                                              "incompatibility between the system and the device."
                                            : "");
                                }
                              }
                            } else {
                              debug(
                                  3,
                                  "could not set output rate %u for device \"%s\", error  \"%s\".",
                                  rates_to_check[ri], interface_name, snd_strerror(local_response));
                            }
                          } else {
                            debug(3, "could not set output format \"%s\" for device: \"%s\".",
                                  snd_pcm_format_name(formats_to_check[fi]),
                                  snd_strerror(local_response));
                          }
                        } else {
                          debug(3, "%u channel output is not available for device: \"%s\"", ci,
                                snd_strerror(local_response));
                        }
                      } else {
                        debug(1, "interleaved access not available for device: \"%s\".",
                              snd_strerror(local_response));
                      }
                    } else {
                      debug(
                          1,
                          "broken configuration for device \"%s\": no configurations available -- "
                          "error %d: \"%s\".",
                          interface_name, local_response, snd_strerror(local_response));
                    }

                    if (local_response == 0) {
                      // format_set |= (1 << fi); // this format is supported, but we need to check
                      // the channel maps
                    }
                  }
                }
                // debug(1, "finished checking formats for %u fps and %u channels",
                // rates_to_check[ri], ci);
                if (format_set != 0) {
                  add_to_configuration_sets(ci, (1 << ri), format_set, channel_map_store,
                                            configuration);
                }
              }
            }
          }
        }
        // now merge sets that have the same rates and formats but different sets of channels
        unsigned int i;
        for (i = 0; i < configuration->configuration_sets_count; i++) {
          unsigned int j;
          for (j = i + 1; j < configuration->configuration_sets_count; j++) {
            if ((configuration->configuration_sets[i].channel_set != 0) &&
                (configuration->configuration_sets[i].rate_set ==
                 configuration->configuration_sets[j].rate_set) &&
                (configuration->configuration_sets[i].format_set ==
                 configuration->configuration_sets[j].format_set)) {
              // copy in the channel maps
              int can_merge = 1;
              int ci;
              for (ci = 1; ci < 32; ci++) {
                // check that the channel maps for channels in both configurations are identical
                if (((configuration->configuration_sets[i].channel_set & (1 << ci)) != 0) &&
                    ((configuration->configuration_sets[j].channel_set & (1 << ci)) != 0)) {
                  if (strcmp(configuration->configuration_sets[i].channel_mappings[ci],
                             configuration->configuration_sets[j].channel_mappings[ci]) != 0)
                    can_merge = 0;
                }
              }
              if (can_merge != 0) {
                // debug(1, "channel merge -- the later one is merged into the earlier one");
                int ci;
                for (ci = 1; ci < 32; ci++) {
                  // copy in any new channel maps
                  if (((configuration->configuration_sets[i].channel_set & (1 << ci)) == 0) &&
                      ((configuration->configuration_sets[j].channel_set & (1 << ci)) != 0)) {
                    strncpy(configuration->configuration_sets[i].channel_mappings[ci],
                            configuration->configuration_sets[j].channel_mappings[ci],
                            sizeof(char[128]));
                  }
                }
                configuration->configuration_sets[i].channel_set |=
                    configuration->configuration_sets[j].channel_set;

                configuration->configuration_sets[j].channel_set = 0; // flag it as empty
              }
            }
          }
        }
      }
      snd_pcm_close(alsa_handle);
    }
    configuration->error_status = ret;
    if (ret != 0)
      debug(1, "get_permissible_configuration_settings: error %d (\"%s\") on device \"%s\".", ret,
            snd_strerror(ret), interface_name);
  } else {
    debug(1, "could not malloc an initial configuration bundle");
  }
  return configuration;
}

// return 0 if they are equal
int configurations_equal(configuration_bundle *a, configuration_bundle *b) {
  int response = 1; // assume they are different
  if ((a == NULL) || (b == NULL)) {
    debug(1, "configurations_equal: null configuration bundle");
  } else {
    if (a->error_status != 0)
      debug(3, "Error a %d (\"%s\") on %s.", a->error_status, snd_strerror(a->error_status),
            a->device_name);
    if (b->error_status != 0)
      debug(3, "Error b %d (\"%s\") on %s.", b->error_status, snd_strerror(b->error_status),
            b->device_name);
    if ((a->error_status == 0) && (b->error_status == 0)) {
      if (a->device_number == b->device_number) {
        if (a->configuration_sets_count == b->configuration_sets_count) {
          unsigned int si;
          for (si = 0; si < a->configuration_sets_count; si++) {
            response = 0; // assume they are equal and look for differences
            configuration_set *ca = &a->configuration_sets[si];
            configuration_set *cb = &b->configuration_sets[si];
            if ((ca->rate_set == cb->rate_set) && (ca->channel_set == cb->channel_set) &&
                (ca->format_set == cb->format_set)) {
              unsigned int cmi = 0;
              while ((cmi < 32) && (response == 0)) {
                if (strcasecmp(ca->channel_mappings[cmi], cb->channel_mappings[cmi]) != 0) {
                  response = 1;
                } else {
                  cmi++;
                }
              }
            } else {
              response = 1;
            }
          }
        }
      }
    }
  }
  return response;
}

void print_configuration(configuration_bundle *configuration,
                         unsigned int similar_interface_count) {
  if (configuration != NULL) {
    unsigned int i;
    unsigned int valid_configuration_sets = 0;
    for (i = 0; i < configuration->configuration_sets_count; i++) {
      if (configuration->configuration_sets[i].channel_set != 0) {
        valid_configuration_sets++;
      }
    }
    unsigned int printed_configuration_sets = 0;
    for (i = 0; i < configuration->configuration_sets_count; i++) {

      if (configuration->configuration_sets[i].channel_set != 0) {
        if (printed_configuration_sets == 0) {
          if (similar_interface_count == 1)
            printf("                  This interface supports ");
          else
            printf("                  These interfaces support ");
        } else {
          if (similar_interface_count == 1)
            printf("                    It also supports ");
          else
            printf("                    They also support ");
        }
        printf("any rate, format and channel combination from the "
               "following "
               "table:\n");
        printf(
            "                       "
            "-------------------------------------------------------------------------------------"
            "------------------------\n");
        printf("                      |    Rate |              Format |  Channels | Channel Map    "
               "          "
               "                                       |\n");
        printf(
            "                       "
            "-------------------------------------------------------------------------------------"
            "------------------------\n");
        configuration_set tcs = configuration->configuration_sets[i];
        unsigned int tri, tfi, tci;
        tri = tfi = tci = 0;
        while ((tcs.rate_set != 0) || (tcs.format_set != 0) || (tcs.channel_set != 0)) {
          // while ((tcs.rate_set != 0) && (tcs.format_set != 0) && (tcs.channel_set !=
          // 0)) { next rate
          if (tcs.rate_set != 0) {
            while ((tcs.rate_set & (1 << tri)) == 0)
              tri++;
            tcs.rate_set &= ~(1 << tri);
            printf("                      |%8d ", rates_to_check[tri]);
          } else {
            printf("                      |         ");
          }
          // next format
          if (tcs.format_set != 0) {
            while ((tcs.format_set & (1 << tfi)) == 0)
              tfi++;
            tcs.format_set &= ~(1 << tfi);
            printf("|%20s ", snd_pcm_format_name(formats_to_check[tfi]));
          } else {
            printf("|                     ");
          }
          // next channel count
          if (tcs.channel_set != 0) {
            while ((tcs.channel_set & (1 << tci)) == 0)
              tci++;
            tcs.channel_set &= ~(1 << tci);
            printf("|%10d | %-63s |\n", tci, tcs.channel_mappings[tci]);
          } else {
            printf("|%10s | %-63s |\n", "", "");
          }
        }
        printf(
            "                       "
            "-------------------------------------------------------------------------------------"
            "------------------------\n");
        printed_configuration_sets++;
      }
    }
  } else {
    debug(1, "no configuration to print!");
  }
}

static int process_cards() {
  // get total number of cards
  int card_count = 0;
  {
    void **hints;
    if (snd_device_name_hint(-1, "ctl", &hints) == 0) {
      void **control_interface_hints = hints;
      while (*control_interface_hints != NULL) {
        char *control_interface_name = snd_device_name_get_hint(*control_interface_hints, "NAME");
        if (strstr(control_interface_name, "hw:CARD=") == control_interface_name) {
          debug(1, "%s", control_interface_name);
          card_count++;
        }
        free(control_interface_name);
        control_interface_hints++;
      }
      snd_device_name_free_hint(hints);
    }
  }
  printf("  --- Alsa Version: %s.\n", SND_LIB_VERSION_STR);
  printf("  --- Sound Cards: %u.\n", card_count);

  char *prefixes[] = {"hw", "hdmi", "iec958"};
  void **hints;
  if (snd_device_name_hint(-1, "ctl", &hints) == 0) {
    void **control_interface_hints = hints;
    while (*control_interface_hints != NULL) {
      char *control_interface_name = snd_device_name_get_hint(*control_interface_hints, "NAME");
      // only accept names that have a "hw:" in them...
      debug(1, "control interface name: \"%s\"", control_interface_name);
      if (strstr(control_interface_name, "hw:CARD=") == control_interface_name) {
        char *card_name = control_interface_name + strlen("hw:CARD=");
        snd_ctl_t *handle;
        int err = snd_ctl_open(&handle, control_interface_name, 0);
        if (err == 0) {
          snd_ctl_card_info_t *info;
          snd_ctl_card_info_alloca(&info);
          err = snd_ctl_card_info(handle, info);
          if (err == 0) {
            const size_t maximum_configurations = 256;
            configuration_bundle *configurations[maximum_configurations];
            size_t current_configuration = 0;

            // if ((err == 0) && (snd_ctl_card_info_get_card(info) != 0)) {
            int card_number = snd_ctl_card_info_get_card(info);
            printf("  --- Card %u:\n", card_number);
            if (display_extended_information != 0) printf("        --- CTL name: \"%s\".\n", control_interface_name);
            printf("        --- Name: \"%s\".\n", snd_ctl_card_info_get_name(info));
            if (display_extended_information != 0) printf("        --- Long name: \"%s\".\n", snd_ctl_card_info_get_longname(info));

            if (display_extended_information != 0)  {
              // get device count
              unsigned int l_device_count = 0;
              {
                int l_dev = -1;
                while ((snd_ctl_pcm_next_device(handle, &l_dev) == 0) && (l_dev != -1))
                  l_device_count++;
              }
              printf("        --- Devices: %u.\n", l_device_count);
            }
            debug(2,
                  "ctl: name \"%s\", index %d, components \"%s\", name \"%s\", longname \"%s\", "
                  "mixer \"%s\", driver \"%s\".",
                  control_interface_name, snd_ctl_card_info_get_card(info),
                  snd_ctl_card_info_get_components(info), snd_ctl_card_info_get_name(info),
                  snd_ctl_card_info_get_longname(info), snd_ctl_card_info_get_mixername(info),
                  snd_ctl_card_info_get_driver(info));

            // get the all the names of the PCM interfaces on the card
            char *interface_names[32];
            unsigned int interface_names_count = 0;

            void **name_hints;
            if (snd_device_name_hint(snd_ctl_card_info_get_card(info), "pcm", &name_hints) == 0) {
              void **device_on_card_hints = name_hints;
              // for each virtual interface of interest on the card...
              while (*device_on_card_hints != NULL) {
                interface_names[interface_names_count++] =
                    snd_device_name_get_hint(*device_on_card_hints, "NAME");
                device_on_card_hints++;
              }
              snd_device_name_free_hint(name_hints);
            }

            {
              unsigned int i;
              for (i = 0; i < interface_names_count; i++) {
                debug(1, "interface name %d is \"%s\".", i, interface_names[i]);
              }
            }

            snd_pcm_info_t *pcminfo;
            snd_pcm_info_alloca(&pcminfo);
            unsigned int pn = 0;
            /*
                        for (pn = 0; pn < sizeof(prefixes) / sizeof(char *); pn++) {
                          // check that the prefix is either "hw" (index 0) or prefixes one  of the
               interface
                          // names
                          int prefix_is_valid = (pn == 0);
                          if (prefix_is_valid == 0) {
                            unsigned int ni = 0;
                            while ((prefix_is_valid == 0) && (ni < interface_names_count)) {
                              if (strstr(interface_names[ni], prefixes[pn]) == interface_names[ni])
               { debug(3, "prefix \"%s\" is valid.", prefixes[pn]); prefix_is_valid = 1; } else {
                                debug(3, "prefix \"%s\" is not valid against \"%s\".", prefixes[pn],
                                      interface_names[ni]);
                                ni++;
                              }
                            }
                          }
                          if (prefix_is_valid != 0) {
            */
            if ((err = snd_ctl_pcm_info(handle, pcminfo)) == 0) {
              int dev = -1;
              while ((snd_ctl_pcm_next_device(handle, &dev) == 0) && (dev != -1)) {
                debug(1, "device: %u", dev);
                snd_pcm_info_set_device(pcminfo, dev);
                if (display_extended_information != 0) {
                  printf("              --- Device %u:\n", dev);
                  printf("                    --- Name: \"%s\".\n", snd_pcm_info_get_name(pcminfo));
                  printf("                    --- ID: \"%s\".\n", snd_pcm_info_get_id(pcminfo));
                }
                snd_pcm_info_set_subdevice(pcminfo, 0);
                if ((err = snd_ctl_pcm_info(handle, pcminfo)) == 0) {
                  int sub_device_count = snd_pcm_info_get_subdevices_avail(pcminfo);
                  if (sub_device_count == 0) {
                    // printf("Note: no subdevices can be found on card %u, device %u!\n", card_number,
                    //        dev);
                    // this zero can happen if the device is busy, so pretend there is at least one.
                    sub_device_count = 1; // pretend there is one subdevice
                    if (display_extended_information != 0) printf("                    --- Subdevices: Count not available. Is the device busy?\n");
                  } else if (display_extended_information != 0) {
                    printf("                    --- Subdevices: %d.\n", sub_device_count);
                  }

                  int sub_device;
                  for (sub_device = 0; sub_device < sub_device_count; sub_device++) {
                    debug(3, "subdevice: %u", sub_device);
                    snd_pcm_info_set_subdevice(pcminfo, sub_device);
                    snd_pcm_info_set_stream(pcminfo, SND_PCM_STREAM_PLAYBACK);
                    if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
                      if (err != -ENOENT)
                        debug(1, "snd_ctl_pcm_info error for card %i, subdevice %i: %s",
                              card_number, sub_device, snd_strerror(err));
                    }
                    if (display_extended_information != 0) {
                    printf("                          --- Subdevice: %d:\n", sub_device);
                    printf("                                --- Name: \"%s\".\n",
                           snd_pcm_info_get_subdevice_name(pcminfo));
                    }
                    int at_least_on_interface_found = 0;
                    for (pn = 0; pn < sizeof(prefixes) / sizeof(char *); pn++) {
                      char interface_name[128];

                      if (sub_device == 0) {
                        if (dev == 0) {
                          sprintf(interface_name, "%s:%s", prefixes[pn], card_name);
                        } else {
                          sprintf(interface_name, "%s:CARD=%s,DEV=%i", prefixes[pn], card_name,
                                  dev);
                        }
                      } else {
                        sprintf(interface_name, "%s:CARD=%s,DEV=%i,SUBDEV=%i", prefixes[pn],
                                card_name, dev, sub_device);
                      }

                      configurations[current_configuration] =
                          get_permissible_configuration_settings(interface_name, pcminfo);

                      if (configurations[current_configuration] != NULL) {
                        if  (configurations[current_configuration]->error_status != -ENOENT) {
                          current_configuration++;
                          if (display_extended_information != 0) {
                          if (at_least_on_interface_found == 0) {
                            printf("                                --- Interfaces:\n");
                            at_least_on_interface_found = 1;
                          }
                          printf("                                      >>> \"%s\"\n",
                                 interface_name);
                          }
                        } else {
                          debug(1, "error %d looking for configurations for \"%s\"", configurations[current_configuration]->error_status, interface_name);
                        }
                      } else {
                        debug(1, "no configuration bundle for interface \"%s\".", interface_name);
                      }
                    }
                  }

                } else {
                  debug(1, "card %i, device %i: error %d, %s", card_number, dev, err,
                        snd_strerror(err));
                }
              }
            } else {
              debug(1, "card %i, error %d, %s", snd_ctl_card_info_get_card(info), err,
                    snd_strerror(err));
            }
            // }
            // }

            // free all those interface names
            unsigned int ini;
            for (ini = 0; ini < interface_names_count; ini++)
              free(interface_names[ini]);

            mixer_bundle_t mixer_info;
            mixer_info.size = MIXER_BUNDLE_SIZE;
            mixer_info.first_free = 0;
            err = process_mixers(control_interface_name, &mixer_info);
            if (err == 0) {
              debug(2, "%u mixers found.", mixer_info.first_free);
              if (mixer_info.first_free == 0)
                printf("        --- No mixers found.\n");
              else if (mixer_info.first_free > 0) {
                if (mixer_info.first_free == 1)
                  printf("        --- Mixer:\n");
                else
                  printf("        --- Mixers:\n");                
                printf("               "
                       "---------------------------------------------------------------------------"
                       "-----------------------------\n");
                printf("              |  %-32s  |  %5s  |  %6s  |  %6s  |  %7s  |  %7s"
                       "  |  %7s  |\n",
                       "Name", "Index", "Min", "Max", "Mute dB", "Min dB", "Max dB");
                printf("               "
                       "---------------------------------------------------------------------------"
                       "-----------------------------\n");
                unsigned int i;
                for (i = 0; i < mixer_info.first_free; i++) {
                  if (mixer_info.mixer[i].has_a_decibel_range != 0) {
                    // if (i % 2 == 0) {
                    printf("              |  %-32s  |  %5u  |  %6ld  |  %6ld  |  %7s  |  %7.2f  | "
                           " %7.2f  |\n",
                           mixer_info.mixer[i].name, mixer_info.mixer[i].index,
                           mixer_info.mixer[i].minv, mixer_info.mixer[i].maxv,
                           mixer_info.mixer[i].lowest_value_is_mute ? "Yes" : "No",
                           mixer_info.mixer[i].mindecibels * 0.01,
                           mixer_info.mixer[i].maxdecibels * 0.01);
                  } else {
                    printf("              |  %-32s  |  %5u  |  %6ld  |  %6ld  |  %7s  |  %7s  "
                           "|  %7s  |\n",
                           mixer_info.mixer[i].name, mixer_info.mixer[i].index,
                           mixer_info.mixer[i].minv, mixer_info.mixer[i].maxv, " ", " ", " ");
                  }
                }
                printf("               "
                       "---------------------------------------------------------------------------"
                       "-----------------------------\n");
              }
            } else {
              debug(1, "Error %d (\"%s\") getting mixer information for card \"%s\".", err,
                    snd_strerror(err), control_interface_name);
            }
            size_t ci;
            int configurations_printed = 0;
            for (ci = 0; ci < current_configuration; ci++) {
              // process the configuration
              if ((configurations[ci] != NULL) && (configurations[ci] != NULL) &&
                  (configurations[ci]->error_status != -ENOENT) &&
                  (configurations[ci]->error_status != -EINVAL) &&
                  (configurations[ci]->already_handled == 0)) {
                if (configurations_printed == 0) {
                  configurations_printed = 1;
                  printf("        --- Interfaces and Supported Formats:\n");
                }
                printf("              >>> Interface \"%s\":\n", configurations[ci]->interface_name);
                char indent[] = "                  ";
                if (configurations[ci]->error_status == -EBUSY) {
                  printf("%sThis interface is busy and can not be "
                         "checked.\n",
                         indent);
                  printf("%sTo check it, take it out of use and try again.\n", indent);
                } else if (configurations[ci]->error_status == -524) {
                  printf("%sThis interface appears to be for a disconnected or uninitialized HDMI port. To test it:\n",
                         indent);
                  printf("%s   (1) connect it to the HDMI device,\n", indent);
                  printf("%s   (2) turn the HDMI device on and select this device as "
                         "source,\n",
                         indent);
                  printf("%s   (3) reboot and try again.\n", indent);
                } else if (configurations[ci]->error_status != 0) {
                  printf("%sError %d (\"%s\").\n", indent, configurations[ci]->error_status,
                         snd_strerror(configurations[ci]->error_status));
                } else {
                  unsigned int similar_interface_count = 1;
                  size_t cj;
                  for (cj = ci + 1; cj < current_configuration; cj++) {
                    if ((configurations[cj]->already_handled == 0) &&
                        (configurations_equal(configurations[ci], configurations[cj]) == 0)) {
                      // if (0) {
                      printf("              >>> Interface \"%s\":\n",
                             configurations[cj]->interface_name);
                      configurations[cj]->already_handled = 1;
                      similar_interface_count++;
                    }
                  }

                  print_configuration(configurations[ci], similar_interface_count);
                }
              }
            }

            debug(1, "Pass 2");
            for (ci = 0; ci < current_configuration; ci++) {
              if (configurations[ci] != NULL) {
                // delete configuration[ci]
                if (configurations[ci]->configuration_sets != NULL)
                  free(configurations[ci]->configuration_sets);
                free(configurations[ci]);
              }
            }
          }
        }
      }
      free(control_interface_name);
      control_interface_hints++;
    }
    snd_device_name_free_hint(hints);
  } else {
    debug(1, "could not get list of control interfaces");
  }
  return 0;
}

void check_device_access() {
  gid_t required_gid = 0;   // store the owner gid of the first inaccessible device
  int required_gid_set = 0; // flag to prevent overwrite of first device
  // get to /dev/snd
  char sound_dir[] = "/dev/snd";
  DIR *dp = opendir(sound_dir);
  if (dp != NULL) {
    int stat_error = 0;
    int dfd = dirfd(dp); // Very, very unlikely to fail
    struct dirent *dirp;
    int sound_devices_found = 0;
    int sound_devices_accessible = 0;
    while ((dirp = readdir(dp)) != NULL) {
      struct stat sb;
      if (fstatat(dfd, dirp->d_name, &sb, 0) == -1) {
        // debug(1, "fstatat(\"%s/%s\") failed (%d: %s)", sound_dir, dirp->d_name, errno,
        //       strerror(errno));
        stat_error = 1;
      } else {
        // debug(1, "fstatat(\"%s/%s\")", sound_dir, dirp->d_name);
        if (((sb.st_mode & S_IFMT) == S_IFCHR) || ((sb.st_mode & S_IFMT) == S_IFBLK)) {
          sound_devices_found++;
          char device_pathname[4096];
          snprintf(device_pathname, sizeof(device_pathname) - 1, "%s/%s", sound_dir, dirp->d_name);
          // debug(1,"Checking access to \"%s\"", device_pathname);
          if (access(device_pathname, R_OK | W_OK) == 0) {
            // debug(1,"Can access \"%s\".", dirp->d_name);
            sound_devices_accessible++;
          } else {
            // debug(1, "Unable to access \"%s\".", dirp->d_name);
            if (required_gid_set == 0) {
              required_gid = sb.st_gid;
              required_gid_set = 1;
            }
          }
        }
      }
    }
    if ((sound_devices_found == 0) && (stat_error == 0)) {
      inform("No sound devices were found."); // not necessarily an error
    } else {
      debug(2, "Devices found in the sound devices directory \"%s\": %d, devices accessible: %d.",
            sound_dir, sound_devices_found, sound_devices_accessible);
      if ((sound_devices_found != sound_devices_accessible) || (stat_error != 0)) {
        // get user name
        struct passwd *result;
        result = getpwuid(getuid());
        if (sound_devices_accessible == 0) {
          // get the name of the group of the first inaccessible device. In most cases, it will be
          // "audio" and will bethe same for all inaccessible devices.

          struct group *gr = getgrgid(required_gid);

          if (stat_error == 0) {
            inform(
                "This check can not be performed because the current user, \"%s\", does not have "
                "permission to access sound devices.",
                result->pw_name);
            inform("Adding \"%s\" to the \"%s\" group may fix this.", result->pw_name, gr->gr_name);
            inform("Alternatively, try running this tool as the \"root\" user.");
          } else {
            inform(
                "This check can not be performed because the current user, \"%s\", does not have "
                "permission to examine the contents of the sound devices directory \"%s\".",
                result->pw_name, sound_dir);
            inform("Try running this tool as the \"root\" user.");
          }

        } else {
          inform("This check can not be performed because the current user, \"%s\", does not have "
                 "permission to access all sound devices.",
                 result->pw_name);
          inform("To fix this, check the permissions of items in the standard sound device "
                 "directory \"%s\".",
                 sound_dir);
          inform("Alternatively, try running this tool as the \"root\" user.");
        }
      }
    }
    closedir(dp);
  } else {
    if (errno == ENOENT) {
      inform("The standard sound device directory \"%s\" was not found.", sound_dir);
    } else {
      inform("The standard sound device directory \"%s\" could not be accessed. (Error %d: %s).",
             sound_dir, errno, strerror(errno));
    }
  }
}

int main(int argc, char *argv[]) {
  snd_lib_error_set_handler(
      (snd_lib_error_handler_t)snd_error_quiet); // quieten alsa diagnostic messages
  // int result = 0;
  int debug_level = 0;
  int i;
  for (i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      if (strcmp(argv[i] + 1, "V") == 0) {
#ifdef CONFIG_USE_GIT_VERSION_STRING
#include "gitversion.h"
        if (git_version_string[0] != '\0')
          fprintf(stdout, "Version: %s\n", git_version_string);
        else
#endif
          fprintf(stdout, "Version: %s.\n", VERSION);
        exit(EXIT_SUCCESS);
      } else if (strcmp(argv[i] + 1, "vvv") == 0) {
        debug_level = 3;
        snd_lib_error_set_handler(NULL); // allow alsa diagnostic messages
      } else if (strcmp(argv[i] + 1, "vv") == 0) {
        debug_level = 2;
        snd_lib_error_set_handler(NULL); // allow alsa diagnostic messages
      } else if (strcmp(argv[i] + 1, "v") == 0) {
        debug_level = 1;
      } else if (strcmp(argv[i] + 1, "h") == 0) {
        fprintf(
            stdout,
            "Dacquery prints information about ALSA DACs -- (Digital to Analog Converters).\n"
            "It tries to open each DAC for interleaved operation at standard rates\n"
            "and formats, with one to eight output channels.\n"
            "Dacquery also prints information about output mixers it finds.\n\n"
            "Notes:\n"
            "1. This tool must be run by a user with access to audio devices,\n"
            "   usually as a member of the \"audio\" unix group, or as the root user.\n"
            "2. Make sure any HDMI devices you wish to check are plugged in, turned on\n"
            "   and enabled when the machine boots up. Reboot if necessary.\n"
            "3. If a device is in use, it can't be checked by this tool.\n"
            "   Take the device out of use and run this tool again.\n"
            "4. If a device can not be accessed, it may mean that it needs to be configured or\n"
            "   connected to an active external device.\n"
            "5. Try 'man dacquery' for more.\n\n"

            "Command line arguments:\n"
            "    -e     display extended information, including a \"map\" of cards, devices, subdevices and interfaces,\n"
            "    -V     display the version,\n"
            "    -v     turn on debugging messages -- not for general use,\n"
            "    -h     display this help text.\n");

        exit(EXIT_SUCCESS);
      } else if (strcmp(argv[i] + 1, "e") == 0) {
        display_extended_information = 1;
      } else {
        fprintf(stdout, "%s -- unknown option. Program terminated.\n", argv[0]);
        exit(EXIT_FAILURE);
      }
    }
  }
  debug_init(debug_level, 0, 1, 1);
  check_device_access();
  return process_cards() ? 1 : 0;
  // result = check_device_access();
}
