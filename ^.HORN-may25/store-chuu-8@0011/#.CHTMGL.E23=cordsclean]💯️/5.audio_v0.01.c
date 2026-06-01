#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/version.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pthread.h>
#include <unistd.h>
#include <inttypes.h>

// Function forward declarations
int stop_audio(const char* filepath);

// Audio state structure
typedef struct {
    char filepath[512];
    int is_playing;
    int is_paused;
    pthread_t playback_thread;
    int thread_running;
    pa_simple *pa_stream;
    AVFormatContext *fmt_ctx;
    AVCodecContext *audio_codec_ctx;
    int audio_stream_idx;
    struct SwrContext *swr_ctx;
    uint64_t total_samples_written;
    int bytes_per_sample;
    double duration;
    double current_time;
    pthread_mutex_t mutex;
} AudioState;

#define MAX_AUDIO_FILES 10
AudioState audio_files[MAX_AUDIO_FILES];

// Initialize audio system
void init_audio_system() {
    // Initialize FFmpeg - av_register_all() is deprecated, initialization is automatic in newer versions
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
#endif
    avformat_network_init();
    
    // Initialize our audio file array
    for (int i = 0; i < MAX_AUDIO_FILES; i++) {
        audio_files[i].filepath[0] = '\0';
        audio_files[i].is_playing = 0;
        audio_files[i].is_paused = 0;
        audio_files[i].thread_running = 0;
        audio_files[i].pa_stream = NULL;
        audio_files[i].fmt_ctx = NULL;
        audio_files[i].audio_codec_ctx = NULL;
        audio_files[i].audio_stream_idx = -1;
        audio_files[i].swr_ctx = NULL;
        pthread_mutex_init(&audio_files[i].mutex, NULL);
    }
    
    printf("Audio system initialized\n");
}

// Find an available audio slot
int find_audio_slot(const char* filepath) {
    for (int i = 0; i < MAX_AUDIO_FILES; i++) {
        if (audio_files[i].filepath[0] == '\0' || 
            strcmp(audio_files[i].filepath, filepath) == 0) {
            // If empty or same file, use this slot
            strncpy(audio_files[i].filepath, filepath, sizeof(audio_files[i].filepath) - 1);
            audio_files[i].filepath[sizeof(audio_files[i].filepath) - 1] = '\0';
            return i;
        }
    }
    return -1; // No available slot
}

// Audio playback function (runs in a separate thread)
void* audio_playback_thread(void* arg) {
    int slot_idx = *(int*)arg;
    AudioState* audio_state = &audio_files[slot_idx];
    
    pthread_mutex_lock(&audio_state->mutex);
    audio_state->thread_running = 1;
    int sample_rate = audio_state->audio_codec_ctx ? audio_state->audio_codec_ctx->sample_rate : 0;
    pthread_mutex_unlock(&audio_state->mutex);
    
    while (1) {
        // Check if thread should continue with proper synchronization
        pthread_mutex_lock(&audio_state->mutex);
        int should_continue = audio_state->thread_running && audio_state->is_playing && !audio_state->is_paused;
        
        // Double-check that required resources are still valid
        int fmt_ctx_valid = (audio_state->fmt_ctx != NULL);
        int codec_ctx_valid = (audio_state->audio_codec_ctx != NULL);
        int pa_stream_valid = (audio_state->pa_stream != NULL);
        int swr_ctx_valid = (audio_state->swr_ctx != NULL);
        
        if (!should_continue || !fmt_ctx_valid || !codec_ctx_valid) {
            pthread_mutex_unlock(&audio_state->mutex);
            break;
        }
        
        // Make local copies of pointers to avoid race conditions during processing
        AVFormatContext *fmt_ctx = audio_state->fmt_ctx;
        AVCodecContext *codec_ctx = audio_state->audio_codec_ctx;
        int audio_stream_idx = audio_state->audio_stream_idx;
        pa_simple *pa_stream = audio_state->pa_stream;
        struct SwrContext *swr_ctx = audio_state->swr_ctx;
        int bytes_per_sample = audio_state->bytes_per_sample;
        
        pthread_mutex_unlock(&audio_state->mutex);
        
        AVPacket pkt;
        int ret = av_read_frame(fmt_ctx, &pkt);
        
        if (ret == AVERROR_EOF) {
            printf("End of audio file reached\n");
            // Restart from beginning if loop is enabled (in a real implementation)
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error reading audio frame: %d\n", ret);
            break;
        }
        
        if (pkt.stream_index == audio_stream_idx) {
            ret = avcodec_send_packet(codec_ctx, &pkt);
            if (ret < 0) {
                fprintf(stderr, "Error sending audio packet: %d\n", ret);
                av_packet_unref(&pkt);
                break;
            }
            
            while (ret >= 0) {
                AVFrame* frame = av_frame_alloc();
                ret = avcodec_receive_frame(codec_ctx, frame);
                
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    av_frame_free(&frame);
                    break;
                } else if (ret < 0) {
                    fprintf(stderr, "Error receiving audio frame: %d\n", ret);
                    av_frame_free(&frame);
                    break;
                }
                
                // Resample and convert the audio
                int out_samples = swr_get_out_samples(swr_ctx, frame->nb_samples);
                if (out_samples <= 0) {
                    av_frame_free(&frame);
                    break;
                }
                
                uint8_t *out_buffer = av_malloc(out_samples * bytes_per_sample);
                if (!out_buffer) {
                    fprintf(stderr, "Failed to allocate output buffer\n");
                    av_frame_free(&frame);
                    break;
                }
                
                out_samples = swr_convert(swr_ctx, &out_buffer, out_samples,
                                         (const uint8_t **)frame->data, frame->nb_samples);
                
                if (out_samples > 0) {
                    int data_size = out_samples * bytes_per_sample;
                    int error;
                    
                    if (!pa_stream_valid || pa_simple_write(pa_stream, out_buffer, data_size, &error) < 0) {
                        if (pa_stream_valid) {
                            fprintf(stderr, "PulseAudio write error: %s\n", pa_strerror(error));
                        }
                        av_free(out_buffer);
                        av_frame_free(&frame);
                        break;
                    } else {
                        pthread_mutex_lock(&audio_state->mutex);
                        audio_state->total_samples_written += out_samples;
                        audio_state->current_time = (double)audio_state->total_samples_written / sample_rate;
                        pthread_mutex_unlock(&audio_state->mutex);
                    }
                }
                
                av_free(out_buffer);
                av_frame_free(&frame);
            }
        }
        
        av_packet_unref(&pkt);
        
        // Small sleep to prevent excessive CPU usage
        usleep(1000);
    }
    
    // Final cleanup check
    pthread_mutex_lock(&audio_state->mutex);
    audio_state->is_playing = 0;
    audio_state->thread_running = 0;
    pthread_mutex_unlock(&audio_state->mutex);
    
    // Drain the audio stream to ensure all audio is played
    pthread_mutex_lock(&audio_state->mutex);
    if (audio_state->pa_stream) {
        int error;
        pa_simple_drain(audio_state->pa_stream, &error);
    }
    pthread_mutex_unlock(&audio_state->mutex);
    
    printf("Audio playback thread finished\n");
    return NULL;
}

// Load and prepare audio file for playback
int load_audio_file(const char* filepath, int slot_idx) {
    AudioState* audio_state = &audio_files[slot_idx];
    
    // Open audio file
    if (avformat_open_input(&audio_state->fmt_ctx, filepath, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open audio file: %s\n", filepath);
        return -1;
    }
    
    if (avformat_find_stream_info(audio_state->fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream info\n");
        return -1;
    }
    
    // Find audio stream
    audio_state->audio_stream_idx = -1;
    for (int i = 0; i < audio_state->fmt_ctx->nb_streams; i++) {
        if (audio_state->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_state->audio_stream_idx = i;
            break;
        }
    }
    
    if (audio_state->audio_stream_idx == -1) {
        fprintf(stderr, "No audio stream found in file: %s\n", filepath);
        return -1;
    }
    
    // Get audio codec
    AVCodec *audio_codec = avcodec_find_decoder(
        audio_state->fmt_ctx->streams[audio_state->audio_stream_idx]->codecpar->codec_id);
    if (!audio_codec) {
        fprintf(stderr, "Unsupported audio codec\n");
        return -1;
    }
    
    audio_state->audio_codec_ctx = avcodec_alloc_context3(audio_codec);
    if (!audio_state->audio_codec_ctx) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        return -1;
    }
    
    avcodec_parameters_to_context(audio_state->audio_codec_ctx,
        audio_state->fmt_ctx->streams[audio_state->audio_stream_idx]->codecpar);
    
    if (avcodec_open2(audio_state->audio_codec_ctx, audio_codec, NULL) < 0) {
        fprintf(stderr, "Could not open audio codec\n");
        return -1;
    }
    
    // Calculate duration
    audio_state->duration = (double)audio_state->fmt_ctx->duration / AV_TIME_BASE;
    printf("Loaded audio file: %s, duration: %.2f seconds\n", filepath, audio_state->duration);
    
    return 0;
}

// Initialize PulseAudio for the audio state
int init_pulseaudio(AudioState* audio_state) {
    pa_sample_spec pa_spec;
    pa_spec.format = PA_SAMPLE_S16LE;
    pa_spec.rate = audio_state->audio_codec_ctx->sample_rate;
    pa_spec.channels = audio_state->audio_codec_ctx->channels;
    
    printf("PulseAudio: rate=%d, channels=%d\n", pa_spec.rate, pa_spec.channels);
    
    int error;
    audio_state->pa_stream = pa_simple_new(NULL, "chtml_audio", PA_STREAM_PLAYBACK, NULL, 
                                          "playback", &pa_spec, NULL, NULL, &error);
    if (!audio_state->pa_stream) {
        fprintf(stderr, "pa_simple_new() failed: %s\n", pa_strerror(error));
        return -1;
    }
    
    audio_state->bytes_per_sample = pa_spec.channels * 2; // 16-bit = 2 bytes
    
    // Initialize resampler
    audio_state->swr_ctx = swr_alloc_set_opts(NULL,
        av_get_default_channel_layout(audio_state->audio_codec_ctx->channels), 
        AV_SAMPLE_FMT_S16, audio_state->audio_codec_ctx->sample_rate,
        audio_state->audio_codec_ctx->channel_layout ? 
            audio_state->audio_codec_ctx->channel_layout : 
            av_get_default_channel_layout(audio_state->audio_codec_ctx->channels),
        audio_state->audio_codec_ctx->sample_fmt, 
        audio_state->audio_codec_ctx->sample_rate,
        0, NULL);
    
    if (!audio_state->swr_ctx || swr_init(audio_state->swr_ctx) < 0) {
        fprintf(stderr, "Could not initialize swresample\n");
        return -1;
    }
    
    return 0;
}

// Play audio file
int play_audio(const char* filepath) {
    int slot_idx = find_audio_slot(filepath);
    if (slot_idx < 0) {
        fprintf(stderr, "No available audio slot\n");
        return -1;
    }
    
    AudioState* audio_state = &audio_files[slot_idx];
    
    pthread_mutex_lock(&audio_state->mutex);
    
    // If already playing, stop it first
    if (audio_state->is_playing) {
        // Temporarily unlock while stopping to avoid deadlock
        pthread_mutex_unlock(&audio_state->mutex);
        stop_audio(filepath);
        usleep(100000); // Wait 100ms for thread to finish
        pthread_mutex_lock(&audio_state->mutex);
    }
    
    // Load the audio file if not already loaded
    if (audio_state->fmt_ctx == NULL) {
        pthread_mutex_unlock(&audio_state->mutex);
        int result = load_audio_file(filepath, slot_idx);
        pthread_mutex_lock(&audio_state->mutex);
        
        if (result < 0) {
            pthread_mutex_unlock(&audio_state->mutex);
            return -1;
        }
    }
    
    // Initialize PulseAudio if needed
    if (audio_state->pa_stream == NULL) {
        int result = init_pulseaudio(audio_state);
        if (result < 0) {
            pthread_mutex_unlock(&audio_state->mutex);
            return -1;
        }
    }
    
    // Reset to beginning and clear any pending data
    if (audio_state->fmt_ctx != NULL) {
        av_seek_frame(audio_state->fmt_ctx, audio_state->audio_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
        if (audio_state->audio_codec_ctx != NULL) {
            avcodec_flush_buffers(audio_state->audio_codec_ctx);
        }
    }
    audio_state->total_samples_written = 0;
    audio_state->current_time = 0.0;
    
    // Start playback
    audio_state->is_playing = 1;
    audio_state->is_paused = 0;
    
    pthread_mutex_unlock(&audio_state->mutex);
    
    // Create playback thread
    if (pthread_create(&audio_state->playback_thread, NULL, audio_playback_thread, &slot_idx) != 0) {
        pthread_mutex_lock(&audio_state->mutex);
        audio_state->is_playing = 0;
        pthread_mutex_unlock(&audio_state->mutex);
        fprintf(stderr, "Could not create audio playback thread\n");
        return -1;
    }
    
    printf("Started playing: %s\n", filepath);
    return 0;
}

// Stop audio playback
int stop_audio(const char* filepath) {
    for (int i = 0; i < MAX_AUDIO_FILES; i++) {
        if (strcmp(audio_files[i].filepath, filepath) == 0) {
            AudioState* audio_state = &audio_files[i];
            
            pthread_mutex_lock(&audio_state->mutex);
            
            // Check if it's actually playing before stopping
            if (!audio_state->is_playing) {
                pthread_mutex_unlock(&audio_state->mutex);
                return 0; // Not playing, nothing to stop
            }
            
            audio_state->is_playing = 0;
            audio_state->is_paused = 0;
            audio_state->thread_running = 0;
            
            // Store the stream temporarily to avoid accessing after mutex release
            pa_simple* pa_stream = audio_state->pa_stream;
            
            // Release the mutex before joining the thread to avoid potential deadlocks
            pthread_mutex_unlock(&audio_state->mutex);
            
            // Wait for thread to finish
            void* thread_result;
            int join_result = pthread_join(audio_files[i].playback_thread, &thread_result);
            if (join_result != 0) {
                fprintf(stderr, "Warning: Could not join audio thread, error: %d\n", join_result);
            }
            
            // Drain the audio stream to ensure all audio is played (if it existed)
            if (pa_stream) {
                int error;
                pa_simple_drain(pa_stream, &error);
            }
            
            printf("Stopped playing: %s\n", filepath);
            return 0;
        }
    }
    
    return -1; // File not found
}

// Pause audio playback
int pause_audio(const char* filepath) {
    for (int i = 0; i < MAX_AUDIO_FILES; i++) {
        if (strcmp(audio_files[i].filepath, filepath) == 0 && audio_files[i].is_playing) {
            AudioState* audio_state = &audio_files[i];
            
            pthread_mutex_lock(&audio_state->mutex);
            audio_state->is_paused = 1;
            pthread_mutex_unlock(&audio_state->mutex);
            
            printf("Paused: %s\n", filepath);
            return 0;
        }
    }
    
    return -1; // File not found
}

// Resume audio playback
int resume_audio(const char* filepath) {
    for (int i = 0; i < MAX_AUDIO_FILES; i++) {
        if (strcmp(audio_files[i].filepath, filepath) == 0 && audio_files[i].is_playing) {
            AudioState* audio_state = &audio_files[i];
            
            pthread_mutex_lock(&audio_state->mutex);
            audio_state->is_paused = 0;
            pthread_mutex_unlock(&audio_state->mutex);
            
            printf("Resumed: %s\n", filepath);
            return 0;
        }
    }
    
    return -1; // File not found
}

// Check if audio is playing
int is_audio_playing(const char* filepath) {
    for (int i = 0; i < MAX_AUDIO_FILES; i++) {
        if (strcmp(audio_files[i].filepath, filepath) == 0) {
            return audio_files[i].is_playing && !audio_files[i].is_paused;
        }
    }
    
    return 0; // Not playing
}

// Get current playback time
double get_audio_time(const char* filepath) {
    for (int i = 0; i < MAX_AUDIO_FILES; i++) {
        if (strcmp(audio_files[i].filepath, filepath) == 0) {
            pthread_mutex_lock(&audio_files[i].mutex);
            double time = audio_files[i].current_time;
            pthread_mutex_unlock(&audio_files[i].mutex);
            return time;
        }
    }
    
    return 0.0; // Default to 0 if not found
}

// Cleanup audio system
void cleanup_audio() {
    for (int i = 0; i < MAX_AUDIO_FILES; i++) {
        if (audio_files[i].is_playing) {
            stop_audio(audio_files[i].filepath);
        }
        
        // Clean up FFmpeg resources
        if (audio_files[i].audio_codec_ctx) {
            avcodec_free_context(&audio_files[i].audio_codec_ctx);
            audio_files[i].audio_codec_ctx = NULL;
        }
        
        if (audio_files[i].fmt_ctx) {
            avformat_close_input(&audio_files[i].fmt_ctx);
            audio_files[i].fmt_ctx = NULL;
        }
        
        if (audio_files[i].swr_ctx) {
            swr_free(&audio_files[i].swr_ctx);
            audio_files[i].swr_ctx = NULL;
        }
        
        // Clean up PulseAudio resources
        if (audio_files[i].pa_stream) {
            pa_simple_free(audio_files[i].pa_stream);
            audio_files[i].pa_stream = NULL;
        }
        
        pthread_mutex_destroy(&audio_files[i].mutex);
    }
    
    avformat_network_deinit();
    printf("Audio system cleaned up\n");
}