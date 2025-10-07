#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#define MAX_PATH_LEN 4096
#define MAX_PLAYLIST_SIZE 10000
#define COMMAND_FILE "player_command.txt"
#define STATUS_FILE "player_status.txt"
#define POLL_INTERVAL_MS 100 // How often to check command file

// Global player state
typedef enum {
    STOPPED,
    PLAYING,
    PAUSED
} PlayerState;

typedef enum {
    SHUFFLE_OFF,
    SHUFFLE_ON
} ShuffleState;

// Define the PlayerContext struct
typedef struct {
    AVFormatContext *fmt_ctx;
    AVCodecContext *audio_codec_ctx;
    struct SwrContext *swr_ctx;
    pa_simple *pa_stream;
    int audio_stream_idx;
    PlayerState state;
    char current_filepath[MAX_PATH_LEN];
    long long current_playback_pts; // Current playback position in presentation time stamps
    long long duration_pts; // Total duration in presentation time stamps
    int current_song_index;
    int seek_requested; // Flag to indicate a seek operation is requested
    long long seek_target_pts; // Target PTS for seeking
    int eof_reached; // Flag to indicate end of file reached
    ShuffleState shuffle_state; // Shuffle state

    // Thread synchronization
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_t audio_thread_id;
    int audio_thread_running; // Flag to track if thread is active
} PlayerContext;

PlayerContext player_ctx;
char playlist[MAX_PLAYLIST_SIZE][MAX_PATH_LEN];
int playlist_size = 0;

// --- Utility Functions ---
void write_status(void) {
    FILE *f = fopen(STATUS_FILE, "w");
    if (f) {
        fprintf(f, "STATE: %s\n", 
                player_ctx.state == PLAYING ? "PLAYING" :
                player_ctx.state == PAUSED ? "PAUSED" : "STOPPED");
        fprintf(f, "SHUFFLE: %s\n",
                player_ctx.shuffle_state == SHUFFLE_ON ? "ON" : "OFF");
        fprintf(f, "CURRENT_SONG_INDEX: %d\n", player_ctx.current_song_index);
        fprintf(f, "CURRENT_SONG_PATH: %s\n", player_ctx.current_filepath);
        
        if (player_ctx.fmt_ctx && player_ctx.audio_stream_idx != -1 && 
            player_ctx.audio_stream_idx < player_ctx.fmt_ctx->nb_streams) {
            double current_sec = (double)player_ctx.current_playback_pts * av_q2d(player_ctx.fmt_ctx->streams[player_ctx.audio_stream_idx]->time_base);
            double total_sec = (double)player_ctx.duration_pts * av_q2d(player_ctx.fmt_ctx->streams[player_ctx.audio_stream_idx]->time_base);
            fprintf(f, "PLAYBACK_TIME: %.2f/%.2f\n", current_sec, total_sec);
        } else {
            fprintf(f, "PLAYBACK_TIME: N/A\n");
        }
        fclose(f);
    }
}

void read_playlist(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("Error opening playlist.txt");
        exit(1);
    }
    playlist_size = 0;
    char buffer[MAX_PATH_LEN];
    while (fgets(buffer, MAX_PATH_LEN, f) && playlist_size < MAX_PLAYLIST_SIZE) {
        size_t len = strcspn(buffer, "\n");
        buffer[len] = 0;
        strncpy(playlist[playlist_size], buffer, MAX_PATH_LEN - 1);
        playlist[playlist_size][MAX_PATH_LEN - 1] = '\0';
        playlist_size++;
    }
    fclose(f);
    printf("Loaded %d songs from playlist.txt\n", playlist_size);
}

// --- FFmpeg and PulseAudio Functions ---
int init_ffmpeg_and_pulseaudio(const char *filepath) {
    int error;

    // Open audio file
    if (avformat_open_input(&player_ctx.fmt_ctx, filepath, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open file %s\n", filepath);
        return -1;
    }
    if (avformat_find_stream_info(player_ctx.fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream info for %s\n", filepath);
        return -1;
    }

    // Find audio stream
    player_ctx.audio_stream_idx = -1;
    for (int i = 0; i < player_ctx.fmt_ctx->nb_streams; i++) {
        if (player_ctx.fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            player_ctx.audio_stream_idx = i;
            break;
        }
    }
    if (player_ctx.audio_stream_idx == -1) {
        fprintf(stderr, "No audio stream found in %s\n", filepath);
        return -1;
    }

    // Initialize codec context
    AVCodec *audio_codec = avcodec_find_decoder(player_ctx.fmt_ctx->streams[player_ctx.audio_stream_idx]->codecpar->codec_id);
    player_ctx.audio_codec_ctx = avcodec_alloc_context3(audio_codec);
    avcodec_parameters_to_context(player_ctx.audio_codec_ctx, player_ctx.fmt_ctx->streams[player_ctx.audio_stream_idx]->codecpar);
    if (avcodec_open2(player_ctx.audio_codec_ctx, audio_codec, NULL) < 0) {
        fprintf(stderr, "Could not open audio codec for %s\n", filepath);
        return -1;
    }

    // Initialize swresample
    player_ctx.swr_ctx = swr_alloc_set_opts(NULL,
                                            av_get_default_channel_layout(player_ctx.audio_codec_ctx->channels), AV_SAMPLE_FMT_S16,
                                            player_ctx.audio_codec_ctx->sample_rate,
                                            player_ctx.audio_codec_ctx->channel_layout ? player_ctx.audio_codec_ctx->channel_layout : av_get_default_channel_layout(player_ctx.audio_codec_ctx->channels),
                                            player_ctx.audio_codec_ctx->sample_fmt, player_ctx.audio_codec_ctx->sample_rate,
                                            0, NULL);
    if (!player_ctx.swr_ctx || swr_init(player_ctx.swr_ctx) < 0) {
        fprintf(stderr, "Could not initialize swresample for %s\n", filepath);
        return -1;
    }

    // Initialize PulseAudio
    pa_sample_spec pa_spec = {
        .format = PA_SAMPLE_S16LE,
        .rate = player_ctx.audio_codec_ctx->sample_rate,
        .channels = player_ctx.audio_codec_ctx->channels
    };
    player_ctx.pa_stream = pa_simple_new(NULL, "mp3_player", PA_STREAM_PLAYBACK, NULL, "playback", &pa_spec, NULL, NULL, &error);
    if (!player_ctx.pa_stream) {
        fprintf(stderr, "pa_simple_new() failed: %s\n", pa_strerror(error));
        return -1;
    }

    player_ctx.duration_pts = player_ctx.fmt_ctx->streams[player_ctx.audio_stream_idx]->duration;
    player_ctx.current_playback_pts = 0;
    strcpy(player_ctx.current_filepath, filepath);
    return 0;
}

void cleanup_player_resources(void) {
    if (player_ctx.fmt_ctx) {
        avformat_close_input(&player_ctx.fmt_ctx);
        player_ctx.fmt_ctx = NULL;
    }
    if (player_ctx.audio_codec_ctx) {
        avcodec_free_context(&player_ctx.audio_codec_ctx);
        player_ctx.audio_codec_ctx = NULL;
    }
    if (player_ctx.swr_ctx) {
        swr_free(&player_ctx.swr_ctx);
        player_ctx.swr_ctx = NULL;
    }
    if (player_ctx.pa_stream) {
        pa_simple_free(player_ctx.pa_stream);
        player_ctx.pa_stream = NULL;
    }
}

// --- Audio Playback Thread ---
void *audio_playback_thread(void *arg) {
    AVPacket pkt;
    AVFrame *audio_frame = av_frame_alloc();
    uint8_t *audio_buf = NULL;
    int bytes_per_sample = pa_sample_size_of_format(PA_SAMPLE_S16LE) * player_ctx.audio_codec_ctx->channels;
    int buf_size;
    int eof = 0;

    while (1) {
        pthread_mutex_lock(&player_ctx.mutex);

        while (player_ctx.state == PAUSED) {
            pthread_cond_wait(&player_ctx.cond, &player_ctx.mutex);
        }

        if (player_ctx.state == STOPPED) {
            pthread_mutex_unlock(&player_ctx.mutex);
            break;
        }

        // Handle seek request
        if (player_ctx.seek_requested) {
            printf("Seeking to %lld PTS\n", player_ctx.seek_target_pts);
            int ret = av_seek_frame(player_ctx.fmt_ctx, player_ctx.audio_stream_idx, player_ctx.seek_target_pts, AVSEEK_FLAG_ANY);
            if (ret < 0) {
                fprintf(stderr, "Error seeking: %s\n", av_err2str(ret));
            } else {
                avcodec_flush_buffers(player_ctx.audio_codec_ctx);
                player_ctx.current_playback_pts = player_ctx.seek_target_pts;
            }
            player_ctx.seek_requested = 0;
        }

        pthread_mutex_unlock(&player_ctx.mutex);

        // Read frame, decode, and play
        if (!eof && player_ctx.state == PLAYING) {
            int ret = av_read_frame(player_ctx.fmt_ctx, &pkt);
            if (ret < 0) {
                if (ret == AVERROR_EOF) {
                    printf("End of file reached.\n");
                    eof = 1;
                } else {
                    fprintf(stderr, "Error reading frame: %s\n", av_err2str(ret));
                }
                av_packet_unref(&pkt);
                if (eof) {
                    pthread_mutex_lock(&player_ctx.mutex);
                    player_ctx.state = STOPPED;
                    player_ctx.eof_reached = 1;
                    pthread_cond_signal(&player_ctx.cond);
                    pthread_mutex_unlock(&player_ctx.mutex);
                }
                continue;
            }

            if (pkt.stream_index == player_ctx.audio_stream_idx) {
                ret = avcodec_send_packet(player_ctx.audio_codec_ctx, &pkt);
                if (ret < 0) {
                    fprintf(stderr, "Error sending packet to decoder: %s\n", av_err2str(ret));
                    av_packet_unref(&pkt);
                    continue;
                }

                while (ret >= 0) {
                    ret = avcodec_receive_frame(player_ctx.audio_codec_ctx, audio_frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    } else if (ret < 0) {
                        fprintf(stderr, "Error receiving frame from decoder: %s\n", av_err2str(ret));
                        break;
                    }

                    // Convert audio format
                    buf_size = swr_get_out_samples(player_ctx.swr_ctx, audio_frame->nb_samples) * bytes_per_sample;
                    audio_buf = av_malloc(buf_size);
                    if (!audio_buf) {
                        fprintf(stderr, "Failed to allocate audio buffer\n");
                        break;
                    }

                    int out_samples = swr_convert(player_ctx.swr_ctx, &audio_buf, buf_size / bytes_per_sample,
                                                  (const uint8_t **)audio_frame->data, audio_frame->nb_samples);
                    if (out_samples < 0) {
                        fprintf(stderr, "Error converting audio: %s\n", av_err2str(out_samples));
                        av_free(audio_buf);
                        break;
                    }

                    // Write to PulseAudio
                    int pa_error;
                    if (pa_simple_write(player_ctx.pa_stream, audio_buf, out_samples * bytes_per_sample, &pa_error) < 0) {
                        fprintf(stderr, "pa_simple_write() failed: %s\n", pa_strerror(pa_error));
                        av_free(audio_buf);
                        pthread_mutex_lock(&player_ctx.mutex);
                        player_ctx.state = STOPPED;
                        pthread_cond_signal(&player_ctx.cond);
                        pthread_mutex_unlock(&player_ctx.mutex);
                        break;
                    }
                    av_free(audio_buf);

                    // Update playback position
                    pthread_mutex_lock(&player_ctx.mutex);
                    player_ctx.current_playback_pts = audio_frame->pts;
                    pthread_mutex_unlock(&player_ctx.mutex);
                }
            }
            av_packet_unref(&pkt);
        }
    }

    av_frame_free(&audio_frame);
    printf("Audio playback thread exiting.\n");
    return NULL;
}

// --- Stop Audio Thread Safely ---
void stop_audio_thread(void) {
    if (player_ctx.audio_thread_running) {
        pthread_mutex_lock(&player_ctx.mutex);
        player_ctx.state = STOPPED;
        pthread_cond_signal(&player_ctx.cond);
        pthread_mutex_unlock(&player_ctx.mutex);
        pthread_join(player_ctx.audio_thread_id, NULL);
        player_ctx.audio_thread_running = 0;
    }
}

// --- Main Command Loop ---
void process_command(const char *command) {
    printf("Processing command: %s\n", command);

    if (strncmp(command, "PLAY ", 5) == 0) {
        int index = atoi(command + 5);
        if (index < 0 || index >= playlist_size) {
            printf("Invalid song index: %d\n", index);
            return;
        }

        if (index == player_ctx.current_song_index && player_ctx.state == PAUSED) {
            // Resume without restarting
            pthread_mutex_lock(&player_ctx.mutex);
            player_ctx.state = PLAYING;
            pthread_cond_signal(&player_ctx.cond);
            pthread_mutex_unlock(&player_ctx.mutex);
        } else {
            // Stop current playback if active
            stop_audio_thread();
            cleanup_player_resources();

            // Set new index and init
            player_ctx.current_song_index = index;
            if (init_ffmpeg_and_pulseaudio(playlist[index]) == 0) {
                pthread_create(&player_ctx.audio_thread_id, NULL, audio_playback_thread, NULL);
                player_ctx.audio_thread_running = 1;
                pthread_mutex_lock(&player_ctx.mutex);
                player_ctx.state = PLAYING;
                player_ctx.eof_reached = 0;
                player_ctx.current_playback_pts = 0;
                player_ctx.seek_requested = 0;
                pthread_cond_signal(&player_ctx.cond);
                pthread_mutex_unlock(&player_ctx.mutex);
            } else {
                player_ctx.state = STOPPED;
            }
        }
    } else if (strcmp(command, "PAUSE") == 0) {
        if (player_ctx.state == PLAYING) {
            pthread_mutex_lock(&player_ctx.mutex);
            player_ctx.state = PAUSED;
            pthread_cond_signal(&player_ctx.cond); // Not strictly needed but safe
            pthread_mutex_unlock(&player_ctx.mutex);
        }
    } else if (strcmp(command, "STOP") == 0) {
        stop_audio_thread();
        cleanup_player_resources();
        player_ctx.state = STOPPED;
    } else if (strcmp(command, "NEXT") == 0) {
        int new_index;
        if (player_ctx.shuffle_state == SHUFFLE_ON) {
            // Generate a random index, but make sure it's not the current song
            do {
                new_index = rand() % playlist_size;
            } while (new_index == player_ctx.current_song_index && playlist_size > 1);
        } else {
            new_index = (player_ctx.current_song_index + 1) % playlist_size;
        }
        
        stop_audio_thread();
        cleanup_player_resources();
        player_ctx.current_song_index = new_index;
        if (init_ffmpeg_and_pulseaudio(playlist[new_index]) == 0) {
            pthread_create(&player_ctx.audio_thread_id, NULL, audio_playback_thread, NULL);
            player_ctx.audio_thread_running = 1;
            pthread_mutex_lock(&player_ctx.mutex);
            player_ctx.state = PLAYING;
            player_ctx.eof_reached = 0;
            player_ctx.current_playback_pts = 0;
            player_ctx.seek_requested = 0;
            pthread_cond_signal(&player_ctx.cond);
            pthread_mutex_unlock(&player_ctx.mutex);
        } else {
            player_ctx.state = STOPPED;
        }
    } else if (strcmp(command, "PREV") == 0) {
        int new_index = (player_ctx.current_song_index - 1 + playlist_size) % playlist_size;
        stop_audio_thread();
        cleanup_player_resources();
        player_ctx.current_song_index = new_index;
        if (init_ffmpeg_and_pulseaudio(playlist[new_index]) == 0) {
            pthread_create(&player_ctx.audio_thread_id, NULL, audio_playback_thread, NULL);
            player_ctx.audio_thread_running = 1;
            pthread_mutex_lock(&player_ctx.mutex);
            player_ctx.state = PLAYING;
            player_ctx.eof_reached = 0;
            player_ctx.current_playback_pts = 0;
            player_ctx.seek_requested = 0;
            pthread_cond_signal(&player_ctx.cond);
            pthread_mutex_unlock(&player_ctx.mutex);
        } else {
            player_ctx.state = STOPPED;
        }
    } else if (strncmp(command, "SEEK ", 5) == 0) {
        double target_sec = atof(command + 5);
        if (player_ctx.fmt_ctx && player_ctx.audio_stream_idx != -1 && player_ctx.state != STOPPED) {
            pthread_mutex_lock(&player_ctx.mutex);
            player_ctx.seek_target_pts = (long long)(target_sec / av_q2d(player_ctx.fmt_ctx->streams[player_ctx.audio_stream_idx]->time_base));
            player_ctx.seek_requested = 1;
            pthread_cond_signal(&player_ctx.cond);
            pthread_mutex_unlock(&player_ctx.mutex);
        }
    } else if (strcmp(command, "QUIT") == 0) {
        stop_audio_thread();
        cleanup_player_resources();
        player_ctx.state = STOPPED;
    } else if (strcmp(command, "SHUFFLE_ON") == 0) {
        player_ctx.shuffle_state = SHUFFLE_ON;
        printf("Shuffle mode enabled\n");
    } else if (strcmp(command, "SHUFFLE_OFF") == 0) {
        player_ctx.shuffle_state = SHUFFLE_OFF;
        printf("Shuffle mode disabled\n");
    } else {
        printf("Unknown command: %s\n", command);
    }
    write_status();
}

// --- Main Function ---
int main(int argc, char *argv[]) {
    printf("DEBUG: Entering main function.\n");
    srand(time(NULL)); // Seed the random number generator
    const char *playlist_file = (argc < 2) ? "playlist.txt" : argv[1];

    read_playlist(playlist_file);
    printf("DEBUG: Playlist loaded. Size: %d\n", playlist_size);
    if (playlist_size == 0) {
        fprintf(stderr, "Playlist is empty or could not be read.\n");
        return 1;
    }

    // Initialize player context
memset(&player_ctx, 0, sizeof(PlayerContext));
player_ctx.state = STOPPED;
player_ctx.shuffle_state = SHUFFLE_OFF;
player_ctx.current_song_index = 0;
player_ctx.audio_thread_running = 0;
pthread_mutex_init(&player_ctx.mutex, NULL);
pthread_cond_init(&player_ctx.cond, NULL);

    printf("MP3 Player CLI started. Monitoring %s for commands.\n", COMMAND_FILE);
    write_status(); // Initial status write

    struct stat last_stat;
    char command_buffer[MAX_PATH_LEN];
    command_buffer[0] = '\0';

    if (stat(COMMAND_FILE, &last_stat) == -1) {
        FILE *f = fopen(COMMAND_FILE, "w");
        if (f) fclose(f);
        stat(COMMAND_FILE, &last_stat);
    }

    while (1) {
        struct stat current_stat;
        if (stat(COMMAND_FILE, &current_stat) == 0 && current_stat.st_mtime > last_stat.st_mtime) {
            FILE *f = fopen(COMMAND_FILE, "r");
            if (f) {
                if (fgets(command_buffer, MAX_PATH_LEN, f)) {
                    command_buffer[strcspn(command_buffer, "\n")] = 0;
                    process_command(command_buffer);
                }
                fclose(f);
            }
            last_stat = current_stat;
        }

        // Check for EOF to auto-play next
        pthread_mutex_lock(&player_ctx.mutex);
        if (player_ctx.state == STOPPED && player_ctx.eof_reached) {
            player_ctx.eof_reached = 0;
            pthread_mutex_unlock(&player_ctx.mutex);

            int new_index;
            if (player_ctx.shuffle_state == SHUFFLE_ON) {
                // Generate a random index
                do {
                    new_index = rand() % playlist_size;
                } while (new_index == player_ctx.current_song_index && playlist_size > 1);
            } else {
                // Play next song in order
                if (player_ctx.current_song_index < playlist_size - 1) {
                    new_index = player_ctx.current_song_index + 1;
                } else {
                    // End of playlist in sequential mode
                    player_ctx.state = STOPPED;
                    write_status();
                    continue;
                }
            }

            player_ctx.current_song_index = new_index;
            if (init_ffmpeg_and_pulseaudio(playlist[player_ctx.current_song_index]) == 0) {
                pthread_create(&player_ctx.audio_thread_id, NULL, audio_playback_thread, NULL);
                player_ctx.audio_thread_running = 1;
                pthread_mutex_lock(&player_ctx.mutex);
                player_ctx.state = PLAYING;
                player_ctx.current_playback_pts = 0;
                player_ctx.seek_requested = 0;
                pthread_cond_signal(&player_ctx.cond);
                pthread_mutex_unlock(&player_ctx.mutex);
                write_status();
            } else {
                player_ctx.state = STOPPED;
                write_status();
            }
        } else {
            pthread_mutex_unlock(&player_ctx.mutex);
        }

        if (strcmp(command_buffer, "QUIT") == 0) {
            break;
        }

        usleep(POLL_INTERVAL_MS * 1000);
    }

    stop_audio_thread();
    cleanup_player_resources();
    pthread_mutex_destroy(&player_ctx.mutex);
    pthread_cond_destroy(&player_ctx.cond);
    printf("MP3 Player CLI gracefully exited.\n");
    return 0;
}
