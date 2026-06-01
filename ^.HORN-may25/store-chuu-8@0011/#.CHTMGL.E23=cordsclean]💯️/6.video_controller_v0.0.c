#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/gl.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

// Forward declarations for functions in audio system
int init_pulseaudio_for_video(void* video_state);
int play_audio_from_video(void* video_state, uint8_t* audio_data, int data_size);

// Video state structure
typedef struct {
    char filepath[512];
    int is_playing;
    int is_paused;
    int is_stopped;
    int has_ended;
    int loop;
    int autoplay;
    
    // FFmpeg components
    AVFormatContext *fmt_ctx;
    AVCodecContext *video_codec_ctx;
    AVCodecContext *audio_codec_ctx;
    AVFrame *frame;
    AVPacket pkt;
    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
    
    int video_stream_idx;
    int audio_stream_idx;
    uint8_t *rgb_buffer;
    GLuint texture_id;
    
    int width, height;
    int eof_reached;
    
    // Video frame queue
    #define MAX_FRAMES 10
    AVFrame *video_frames[MAX_FRAMES];
    double video_pts[MAX_FRAMES];
    int video_frame_count;
    double audio_time_offset;
    
    // Audio with PulseAudio
    void *pa_stream;  // Using void* to avoid including pulse headers here
    int bytes_per_sample;
    uint64_t total_samples_written;
    
    // Synchronization
    struct timeval start_time;
    int initialized;
} VideoState;

#define MAX_VIDEO_FILES 5
VideoState video_files[MAX_VIDEO_FILES];

// Function prototypes
void* video_playback_thread(void* arg);
int init_ffmpeg_video(const char* filepath, int video_idx);
int load_video_file(const char* filepath, int video_idx);
void cleanup_video_state(int video_idx);
void init_video_system();
double get_video_audio_time(int video_idx);

// Initialize video system
void init_video_system() {
    // Initialize FFmpeg libraries
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
#endif
    avformat_network_init();
    
    // Initialize our video file array
    for (int i = 0; i < MAX_VIDEO_FILES; i++) {
        memset(&video_files[i], 0, sizeof(VideoState));
        video_files[i].filepath[0] = '\0';
        video_files[i].is_playing = 0;
        video_files[i].is_paused = 0;
        video_files[i].is_stopped = 1;
        video_files[i].has_ended = 0;
        video_files[i].loop = 0;
        video_files[i].autoplay = 0;
        video_files[i].video_stream_idx = -1;
        video_files[i].audio_stream_idx = -1;
        video_files[i].fmt_ctx = NULL;
        video_files[i].video_codec_ctx = NULL;
        video_files[i].audio_codec_ctx = NULL;
        video_files[i].frame = NULL;
        video_files[i].sws_ctx = NULL;
        video_files[i].swr_ctx = NULL;
        video_files[i].rgb_buffer = NULL;
        video_files[i].eof_reached = 0;
        video_files[i].video_frame_count = 0;
        video_files[i].audio_time_offset = 0.0;
        video_files[i].total_samples_written = 0;
        video_files[i].initialized = 0;
        
        // Initialize frame queue
        for (int j = 0; j < MAX_FRAMES; j++) {
            video_files[i].video_frames[j] = NULL;
        }
    }
    
    printf("Video system initialized\n");
}

// Find available video slot
int find_video_slot(const char* filepath) {
    for (int i = 0; i < MAX_VIDEO_FILES; i++) {
        if (video_files[i].filepath[0] == '\0' || 
            strcmp(video_files[i].filepath, filepath) == 0) {
            // If empty or same file, use this slot
            strncpy(video_files[i].filepath, filepath, sizeof(video_files[i].filepath) - 1);
            video_files[i].filepath[sizeof(video_files[i].filepath) - 1] = '\0';
            return i;
        }
    }
    return -1; // No available slot
}

// Initialize FFmpeg for video file
int init_ffmpeg_video(const char* filepath, int video_idx) {
    VideoState* video_state = &video_files[video_idx];
    
    printf("Initializing FFmpeg for video: %s\n", filepath);
    
    if (avformat_open_input(&video_state->fmt_ctx, filepath, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open video file: %s\n", filepath);
        return -1;
    }
    
    if (avformat_find_stream_info(video_state->fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream info\n");
        return -1;
    }

    // Find video and audio streams
    video_state->video_stream_idx = -1;
    video_state->audio_stream_idx = -1;
    
    for (int i = 0; i < video_state->fmt_ctx->nb_streams; i++) {
        if (video_state->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_state->video_stream_idx == -1) {
            video_state->video_stream_idx = i;
        } else if (video_state->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && video_state->audio_stream_idx == -1) {
            video_state->audio_stream_idx = i;
        }
    }
    
    if (video_state->video_stream_idx == -1) {
        fprintf(stderr, "No video stream found\n");
        return -1;
    }
    
    if (video_state->audio_stream_idx == -1) {
        printf("No audio stream found, continuing without audio\n");
    }

    // Initialize video codec
    AVCodec *video_codec = avcodec_find_decoder(video_state->fmt_ctx->streams[video_state->video_stream_idx]->codecpar->codec_id);
    if (!video_codec) {
        fprintf(stderr, "Unsupported video codec\n");
        return -1;
    }
    
    video_state->video_codec_ctx = avcodec_alloc_context3(video_codec);
    avcodec_parameters_to_context(video_state->video_codec_ctx, video_state->fmt_ctx->streams[video_state->video_stream_idx]->codecpar);
    if (avcodec_open2(video_state->video_codec_ctx, video_codec, NULL) < 0) {
        fprintf(stderr, "Could not open video codec\n");
        return -1;
    }

    // Initialize audio codec if present
    if (video_state->audio_stream_idx != -1) {
        AVCodec *audio_codec = avcodec_find_decoder(video_state->fmt_ctx->streams[video_state->audio_stream_idx]->codecpar->codec_id);
        if (audio_codec) {
            video_state->audio_codec_ctx = avcodec_alloc_context3(audio_codec);
            avcodec_parameters_to_context(video_state->audio_codec_ctx, video_state->fmt_ctx->streams[video_state->audio_stream_idx]->codecpar);
            if (avcodec_open2(video_state->audio_codec_ctx, audio_codec, NULL) < 0) {
                fprintf(stderr, "Could not open audio codec, continuing without audio\n");
                video_state->audio_stream_idx = -1; // Disable audio
            }
        } else {
            printf("Audio codec not found, continuing without audio\n");
            video_state->audio_stream_idx = -1;
        }
    }

    // Allocate frame
    video_state->frame = av_frame_alloc();
    video_state->width = video_state->video_codec_ctx->width;
    video_state->height = video_state->video_codec_ctx->height;
    printf("Video resolution: %dx%d\n", video_state->width, video_state->height);

    // Initialize sws context for RGB conversion
    video_state->sws_ctx = sws_getContext(video_state->width, video_state->height, video_state->video_codec_ctx->pix_fmt,
                             video_state->width, video_state->height, AV_PIX_FMT_RGB24,
                             SWS_BILINEAR, NULL, NULL, NULL);
    video_state->rgb_buffer = (uint8_t*)malloc(video_state->width * video_state->height * 3);

    // Initialize swresample for audio if present
    if (video_state->audio_stream_idx != -1) {
        video_state->swr_ctx = swr_alloc_set_opts(NULL,
                                     av_get_default_channel_layout(video_state->audio_codec_ctx->channels), AV_SAMPLE_FMT_S16, video_state->audio_codec_ctx->sample_rate,
                                     video_state->audio_codec_ctx->channel_layout ? video_state->audio_codec_ctx->channel_layout : av_get_default_channel_layout(video_state->audio_codec_ctx->channels),
                                     video_state->audio_codec_ctx->sample_fmt, video_state->audio_codec_ctx->sample_rate,
                                     0, NULL);
        if (!video_state->swr_ctx || swr_init(video_state->swr_ctx) < 0) {
            fprintf(stderr, "Could not initialize swresample, continuing without audio\n");
            video_state->audio_stream_idx = -1; // Disable audio
        }
        
        // Initialize PulseAudio for this video
        if (init_pulseaudio_for_video(video_state) < 0) {
            printf("Could not initialize PulseAudio, continuing without audio\n");
            video_state->audio_stream_idx = -1; // Disable audio
        }
    }

    video_state->initialized = 1;
    return 0;
}

// Load video file and prepare for playback
int load_video_file(const char* filepath, int video_idx) {
    VideoState* video_state = &video_files[video_idx];
    
    return init_ffmpeg_video(filepath, video_idx);
}

// Get current audio playback time for synchronization
double get_video_audio_time(int video_idx) {
    VideoState* video_state = &video_files[video_idx];
    
    if (video_state->audio_codec_ctx && video_state->audio_stream_idx != -1) {
        return (double)video_state->total_samples_written / video_state->audio_codec_ctx->sample_rate;
    }
    // Fallback: use wall clock time if audio is not available
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    double elapsed = (current_time.tv_sec - video_state->start_time.tv_sec) + 
                     (current_time.tv_usec - video_state->start_time.tv_usec) / 1000000.0;
    return elapsed + video_state->audio_time_offset;
}

// Video playback function (runs in a separate thread)
void* video_playback_thread(void* arg) {
    int video_idx = *(int*)arg;
    VideoState* video_state = &video_files[video_idx];
    
    printf("Starting video playback thread for: %s\n", video_state->filepath);
    
    // Record start time for synchronization
    gettimeofday(&video_state->start_time, NULL);
    video_state->total_samples_written = 0;
    video_state->audio_time_offset = 0.0;
    
    // Main playback loop
    while (video_state->is_playing && !video_state->is_paused) {
        // Decode packets until queue is full or end of file
        if (!video_state->eof_reached && video_state->video_frame_count < MAX_FRAMES) {
            int ret = av_read_frame(video_state->fmt_ctx, &video_state->pkt);
            if (ret == AVERROR_EOF) {
                printf("Video EOF reached\n");
                video_state->eof_reached = 1;
                av_packet_unref(&video_state->pkt);
            } else if (ret >= 0) {
                if (video_state->pkt.stream_index == video_state->video_stream_idx) {
                    if (avcodec_send_packet(video_state->video_codec_ctx, &video_state->pkt) >= 0) {
                        AVFrame *frame = av_frame_alloc();
                        if (avcodec_receive_frame(video_state->video_codec_ctx, frame) >= 0) {
                            if (video_state->video_frame_count < MAX_FRAMES) {
                                video_state->video_frames[video_state->video_frame_count] = frame;
                                video_state->video_pts[video_state->video_frame_count] = av_q2d(video_state->fmt_ctx->streams[video_state->video_stream_idx]->time_base) * frame->pts;
                                printf("Queued video frame %d, PTS=%.2f\n", video_state->video_frame_count, video_state->video_pts[video_state->video_frame_count]);
                                video_state->video_frame_count++;
                            } else {
                                av_frame_free(&frame);
                            }
                        } else {
                            av_frame_free(&frame);
                        }
                    }
                } else if (video_state->pkt.stream_index == video_state->audio_stream_idx) {
                    if (avcodec_send_packet(video_state->audio_codec_ctx, &video_state->pkt) >= 0) {
                        AVFrame *audio_frame = av_frame_alloc();
                        if (avcodec_receive_frame(video_state->audio_codec_ctx, audio_frame) >= 0) {
                            // Allocate output buffer for converted audio
                            int out_samples = swr_get_out_samples(video_state->swr_ctx, audio_frame->nb_samples);
                            uint8_t *out_buffer = (uint8_t*)av_malloc(out_samples * video_state->bytes_per_sample);
                            out_samples = swr_convert(video_state->swr_ctx, &out_buffer, out_samples,
                                             (const uint8_t **)audio_frame->data, audio_frame->nb_samples);
                            if (out_samples > 0) {
                                int data_size = out_samples * video_state->bytes_per_sample;
                                if (play_audio_from_video(video_state, out_buffer, data_size) < 0) {
                                    fprintf(stderr, "Failed to play audio from video\n");
                                } else {
                                    video_state->total_samples_written += out_samples;
                                    printf("Wrote %d audio samples, total=%" PRIu64 "\n", out_samples, video_state->total_samples_written);
                                }
                            }
                            av_free(out_buffer);
                            av_frame_free(&audio_frame);
                        }
                    }
                }
                av_packet_unref(&video_state->pkt);
            }
        }
        
        // Small sleep to prevent excessive CPU usage
        usleep(1000);
        
        // Check if playback should end
        if (video_state->eof_reached && video_state->video_frame_count == 0) {
            double current_time = get_video_audio_time(video_idx);
            double total_duration = (double)video_state->fmt_ctx->duration / AV_TIME_BASE;
            if (current_time >= total_duration - 0.1) {
                printf("Video playback completed\n");
                video_state->has_ended = 1;
                if (video_state->loop) {
                    // Seek back to beginning for loop
                    av_seek_frame(video_state->fmt_ctx, video_state->video_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
                    if (video_state->audio_codec_ctx) {
                        avcodec_flush_buffers(video_state->audio_codec_ctx);
                    }
                    if (video_state->video_codec_ctx) {
                        avcodec_flush_buffers(video_state->video_codec_ctx);
                    }
                    video_state->eof_reached = 0;
                    video_state->audio_time_offset = 0.0;
                    gettimeofday(&video_state->start_time, NULL);
                    continue;
                } else {
                    break; // Exit playback loop
                }
            }
        }
    }
    
    printf("Video playback thread finished for: %s\n", video_state->filepath);
    return NULL;
}

// Play video file
int play_video(const char* filepath, int autoplay, int loop) {
    int video_idx = find_video_slot(filepath);
    if (video_idx < 0) {
        fprintf(stderr, "No available video slot\n");
        return -1;
    }
    
    VideoState* video_state = &video_files[video_idx];
    
    // Just update the video properties without restarting if already playing
    if (video_state->is_playing) {
        // If already playing, just update loop setting and continue
        video_state->loop = loop;
        video_state->is_paused = 0;  // Resume if it was paused
        video_state->has_ended = 0;  // Reset end flag
        printf("Video already playing, resuming: %s\n", filepath);
        return 0;
    }
    
    // Load the video file if not already loaded
    if (!video_state->initialized) {
        int result = load_video_file(filepath, video_idx);
        if (result < 0) {
            return -1;
        }
    }
    
    // Set video properties
    video_state->autoplay = autoplay;
    video_state->loop = loop;
    video_state->is_playing = 1;
    video_state->is_paused = 0;
    video_state->is_stopped = 0;
    video_state->has_ended = 0;
    video_state->eof_reached = 0;
    video_state->video_frame_count = 0;
    
    // Reset frame queue
    for (int i = 0; i < MAX_FRAMES; i++) {
        if (video_state->video_frames[i]) {
            av_frame_free(&video_state->video_frames[i]);
            video_state->video_frames[i] = NULL;
        }
    }
    video_state->video_frame_count = 0;
    
    // Create playback thread
    pthread_t playback_thread;
    if (pthread_create(&playback_thread, NULL, video_playback_thread, &video_idx) != 0) {
        video_state->is_playing = 0;
        fprintf(stderr, "Could not create video playback thread\n");
        return -1;
    }
    
    // Store thread ID if needed for later joining
    // For now, we'll detach it since we're not keeping a reference
    pthread_detach(playback_thread);
    
    printf("Started playing video: %s\n", filepath);
    return 0;
}

// Stop video playback
int stop_video(const char* filepath) {
    for (int i = 0; i < MAX_VIDEO_FILES; i++) {
        if (strcmp(video_files[i].filepath, filepath) == 0) {
            VideoState* video_state = &video_files[i];
            
            // Safely stop playback by setting flags
            video_state->is_playing = 0;
            video_state->is_paused = 0;
            video_state->is_stopped = 1;
            video_state->has_ended = 1;  // Mark as ended to help threads finish
            
            // Wait a bit longer to ensure thread processes stop signal
            usleep(100000); // 100ms wait to allow thread to process stop signal
            
            // Set the mutex to protect during cleanup
            // Since we don't have mutex in this simplified version, we'll just set the state
            
            // It's safer to let the playback thread finish completely before cleaning up
            // Just reset state rather than freeing frames that might still be accessed by thread
            for (int j = 0; j < MAX_FRAMES; j++) {
                video_state->video_frames[j] = NULL;  // Don't free, just null out
            }
            video_state->video_frame_count = 0;
            
            // Seek to beginning for next play
            if (video_state->fmt_ctx && video_state->video_stream_idx >= 0) {
                av_seek_frame(video_state->fmt_ctx, video_state->video_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
                if (video_state->audio_codec_ctx) {
                    avcodec_flush_buffers(video_state->audio_codec_ctx);
                }
                if (video_state->video_codec_ctx) {
                    avcodec_flush_buffers(video_state->video_codec_ctx);
                }
            }
            
            video_state->audio_time_offset = 0.0;
            video_state->eof_reached = 0;
            
            printf("Stopped video: %s\n", filepath);
            return 0;
        }
    }
    
    return -1; // File not found
}

// Pause video playback
int pause_video(const char* filepath) {
    for (int i = 0; i < MAX_VIDEO_FILES; i++) {
        if (strcmp(video_files[i].filepath, filepath) == 0 && video_files[i].is_playing) {
            video_files[i].is_paused = 1;
            printf("Paused video: %s\n", filepath);
            return 0;
        }
    }
    
    return -1; // File not found
}

// Resume video playback
int resume_video(const char* filepath) {
    for (int i = 0; i < MAX_VIDEO_FILES; i++) {
        if (strcmp(video_files[i].filepath, filepath) == 0) {
            video_files[i].is_paused = 0;
            video_files[i].is_playing = 1;
            printf("Resumed video: %s\n", filepath);
            return 0;
        }
    }
    
    return -1; // File not found
}

// Check if video is playing
int is_video_playing(const char* filepath) {
    for (int i = 0; i < MAX_VIDEO_FILES; i++) {
        if (strcmp(video_files[i].filepath, filepath) == 0) {
            return video_files[i].is_playing && !video_files[i].is_paused && !video_files[i].has_ended;
        }
    }
    
    return 0; // Not playing
}

// Get video frame for rendering at current time
AVFrame* get_video_frame_for_time(const char* filepath, double current_time) {
    for (int i = 0; i < MAX_VIDEO_FILES; i++) {
        if (strcmp(video_files[i].filepath, filepath) == 0) {
            VideoState* video_state = &video_files[i];
            
            // Find frame to render based on current time
            int frame_idx = -1;
            for (int j = 0; j < video_state->video_frame_count; j++) {
                if (video_state->video_pts[j] <= current_time) {
                    frame_idx = j;
                } else {
                    break;
                }
            }
            
            if (frame_idx >= 0) {
                // Return the frame but don't remove it from the queue yet
                // The rendering function will handle cleanup
                return video_state->video_frames[frame_idx];
            }
        }
    }
    
    return NULL; // No frame found
}

// Function to update all video textures that are playing - called from main display loop
void update_all_video_textures() {
    for (int i = 0; i < MAX_VIDEO_FILES; i++) {
        if (video_files[i].is_playing && !video_files[i].is_paused && !video_files[i].has_ended && 
            strlen(video_files[i].filepath) > 0 && video_files[i].texture_id != 0) {
            // This would update the texture, but since we don't have direct access to texture_id here,
            // we'll just ensure the video is decoding frames properly
            // The actual texture update will happen in the view when drawing each element
        }
    }
}

// Get frame count in queue for synchronization
int get_video_frame_count(const char* filepath) {
    for (int i = 0; i < MAX_VIDEO_FILES; i++) {
        if (strcmp(video_files[i].filepath, filepath) == 0) {
            return video_files[i].video_frame_count;
        }
    }
    
    return 0;
}

// Convert video frame to RGB and update texture
int update_video_texture(const char* filepath, GLuint texture_id) {
    for (int i = 0; i < MAX_VIDEO_FILES; i++) {
        if (strcmp(video_files[i].filepath, filepath) == 0) {
            VideoState* video_state = &video_files[i];
            
            // Only update if video is actually playing
            if (!video_state->is_playing || video_state->is_paused || video_state->has_ended) {
                return 0; // Not playing, nothing to update
            }
            
            double current_time = get_video_audio_time(i);
            
            // Find frame to render
            int frame_idx = -1;
            for (int j = 0; j < video_state->video_frame_count; j++) {
                if (video_state->video_pts[j] <= current_time) {
                    frame_idx = j;
                } else {
                    break;
                }
            }

            // Render frame if available
            if (frame_idx >= 0) {
                printf("Rendering video frame %d, PTS=%.2f\n", frame_idx, video_state->video_pts[frame_idx]);
                AVFrame *frame = video_state->video_frames[frame_idx];
                if (frame) {
                    // Make sure the RGB buffer is properly allocated
                    if (video_state->rgb_buffer && video_state->sws_ctx) {
                        uint8_t *dest[4] = {video_state->rgb_buffer, NULL, NULL, NULL};
                        int dest_linesize[4] = {video_state->width * 3, 0, 0, 0};
                        
                        sws_scale(video_state->sws_ctx, (const uint8_t * const *)frame->data, frame->linesize,
                                  0, video_state->height, dest, dest_linesize);

                        // Bind and update the texture
                        glBindTexture(GL_TEXTURE_2D, texture_id);
                        
                        // Use proper format - RGB for the texture
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, video_state->width, video_state->height, 0,
                                     GL_RGB, GL_UNSIGNED_BYTE, video_state->rgb_buffer);

                        // Clean up only the rendered frame and shift queue
                        av_frame_free(&video_state->video_frames[frame_idx]);
                        video_state->video_frames[frame_idx] = NULL;
                        
                        // Shift remaining frames down
                        for (int k = frame_idx; k < video_state->video_frame_count - 1; k++) {
                            video_state->video_frames[k] = video_state->video_frames[k + 1];
                            video_state->video_pts[k] = video_state->video_pts[k + 1];
                        }
                        video_state->video_frame_count--;
                        
                        return 1; // Successfully updated texture
                    }
                }
            }
        }
    }
    
    return 0; // Failed to update texture
}

// Get video dimensions
int get_video_dimensions(const char* filepath, int* width, int* height) {
    for (int i = 0; i < MAX_VIDEO_FILES; i++) {
        if (strcmp(video_files[i].filepath, filepath) == 0) {
            *width = video_files[i].width;
            *height = video_files[i].height;
            return 0;
        }
    }
    
    return -1; // File not found
}

// Function to check if any videos are currently playing - for display loop optimization
int are_any_videos_playing() {
    for (int i = 0; i < MAX_VIDEO_FILES; i++) {
        if (video_files[i].is_playing && !video_files[i].is_paused && !video_files[i].has_ended) {
            return 1; // At least one video is playing
        }
    }
    return 0; // No videos are playing
}

// Cleanup video system
void cleanup_video() {
    for (int i = 0; i < MAX_VIDEO_FILES; i++) {
        if (video_files[i].is_playing) {
            stop_video(video_files[i].filepath);
        }
        
        // Clean up FFmpeg resources
        if (video_files[i].frame) {
            av_frame_free(&video_files[i].frame);
            video_files[i].frame = NULL;
        }
        if (video_files[i].video_codec_ctx) {
            avcodec_free_context(&video_files[i].video_codec_ctx);
            video_files[i].video_codec_ctx = NULL;
        }
        if (video_files[i].audio_codec_ctx) {
            avcodec_free_context(&video_files[i].audio_codec_ctx);
            video_files[i].audio_codec_ctx = NULL;
        }
        if (video_files[i].fmt_ctx) {
            avformat_close_input(&video_files[i].fmt_ctx);
            video_files[i].fmt_ctx = NULL;
        }
        if (video_files[i].sws_ctx) {
            sws_freeContext(video_files[i].sws_ctx);
            video_files[i].sws_ctx = NULL;
        }
        if (video_files[i].swr_ctx) {
            swr_free(&video_files[i].swr_ctx);
            video_files[i].swr_ctx = NULL;
        }
        if (video_files[i].rgb_buffer) {
            free(video_files[i].rgb_buffer);
            video_files[i].rgb_buffer = NULL;
        }
        
        // Clean up frame queue
        for (int j = 0; j < MAX_FRAMES; j++) {
            if (video_files[i].video_frames[j]) {
                av_frame_free(&video_files[i].video_frames[j]);
                video_files[i].video_frames[j] = NULL;
            }
        }
        video_files[i].video_frame_count = 0;
    }
    
    avformat_network_deinit();
    printf("Video system cleaned up\n");
}

// Function to initialize PulseAudio for video (needs to be implemented properly)
int init_pulseaudio_for_video(void* video_state_ptr) {
    VideoState* video_state = (VideoState*)video_state_ptr;
    
    if (!video_state || !video_state->audio_codec_ctx) {
        return -1;
    }
    
    pa_sample_spec pa_spec;
    pa_spec.format = PA_SAMPLE_S16LE;
    pa_spec.rate = video_state->audio_codec_ctx->sample_rate;
    pa_spec.channels = video_state->audio_codec_ctx->channels;
    printf("Video PulseAudio: rate=%d, channels=%d\n", pa_spec.rate, pa_spec.channels);

    int error;
    video_state->pa_stream = pa_simple_new(NULL, "chtml_video", PA_STREAM_PLAYBACK, NULL, "video_playback", &pa_spec, NULL, NULL, &error);
    if (!video_state->pa_stream) {
        fprintf(stderr, "pa_simple_new() for video failed: %s\n", pa_strerror(error));
        return -1;
    }

    video_state->bytes_per_sample = pa_spec.channels * 2; // 16-bit audio
    return 0;
}

// Function to play audio from video (needs to be implemented properly)
int play_audio_from_video(void* video_state_ptr, uint8_t* audio_data, int data_size) {
    VideoState* video_state = (VideoState*)video_state_ptr;
    
    if (!video_state || !video_state->pa_stream) {
        return -1;
    }

    int error;
    if (pa_simple_write(video_state->pa_stream, audio_data, data_size, &error) < 0) {
        fprintf(stderr, "pa_simple_write for video failed: %s\n", pa_strerror(error));
        return -1;
    }
    
    return 0;
}