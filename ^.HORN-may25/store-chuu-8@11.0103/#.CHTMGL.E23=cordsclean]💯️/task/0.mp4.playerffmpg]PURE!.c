#include <stdio.h>
#include <stdlib.h>
#include <GL/glut.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <math.h>

// Global variables
AVFormatContext *fmt_ctx = NULL;
AVCodecContext *video_codec_ctx = NULL;
AVCodecContext *audio_codec_ctx = NULL;
AVFrame *frame = NULL;
AVPacket pkt;
struct SwsContext *sws_ctx = NULL;
struct SwrContext *swr_ctx = NULL;
int video_stream_idx = -1;
int audio_stream_idx = -1;
uint8_t *rgb_buffer = NULL;
GLuint texture_id;
int width, height;
int eof_reached = 0;

// PulseAudio variables
pa_simple *pa_stream = NULL;
pa_sample_spec pa_spec;
uint64_t total_samples_written = 0;
int bytes_per_sample;

// Video frame queue
#define MAX_FRAMES 10
AVFrame *video_frames[MAX_FRAMES];
double video_pts[MAX_FRAMES];
int video_frame_count = 0;

// Get current audio playback time
double get_audio_time() {
    return (double)total_samples_written / pa_spec.rate;
}

// Initialize FFmpeg
void init_ffmpeg(const char *filename) {
    printf("Initializing FFmpeg for %s\n", filename);
    if (avformat_open_input(&fmt_ctx, filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open video file\n");
        exit(1);
    }
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream info\n");
        exit(1);
    }

    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_idx == -1) {
            video_stream_idx = i;
        } else if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_idx == -1) {
            audio_stream_idx = i;
        }
    }
    if (video_stream_idx == -1) {
        fprintf(stderr, "No video stream found\n");
        exit(1);
    }
    if (audio_stream_idx == -1) {
        fprintf(stderr, "No audio stream found\n");
        exit(1);
    }

    AVCodec *video_codec = avcodec_find_decoder(fmt_ctx->streams[video_stream_idx]->codecpar->codec_id);
    video_codec_ctx = avcodec_alloc_context3(video_codec);
    avcodec_parameters_to_context(video_codec_ctx, fmt_ctx->streams[video_stream_idx]->codecpar);
    if (avcodec_open2(video_codec_ctx, video_codec, NULL) < 0) {
        fprintf(stderr, "Could not open video codec\n");
        exit(1);
    }

    AVCodec *audio_codec = avcodec_find_decoder(fmt_ctx->streams[audio_stream_idx]->codecpar->codec_id);
    audio_codec_ctx = avcodec_alloc_context3(audio_codec);
    avcodec_parameters_to_context(audio_codec_ctx, fmt_ctx->streams[audio_stream_idx]->codecpar);
    if (avcodec_open2(audio_codec_ctx, audio_codec, NULL) < 0) {
        fprintf(stderr, "Could not open audio codec\n");
        exit(1);
    }

    frame = av_frame_alloc();
    width = video_codec_ctx->width;
    height = video_codec_ctx->height;
    printf("Video resolution: %dx%d\n", width, height);

    sws_ctx = sws_getContext(width, height, video_codec_ctx->pix_fmt,
                             width, height, AV_PIX_FMT_RGB24,
                             SWS_BILINEAR, NULL, NULL, NULL);
    rgb_buffer = malloc(width * height * 3);

    // Initialize swresample
    swr_ctx = swr_alloc_set_opts(NULL,
                                 av_get_default_channel_layout(audio_codec_ctx->channels), AV_SAMPLE_FMT_S16, audio_codec_ctx->sample_rate,
                                 audio_codec_ctx->channel_layout ? audio_codec_ctx->channel_layout : av_get_default_channel_layout(audio_codec_ctx->channels),
                                 audio_codec_ctx->sample_fmt, audio_codec_ctx->sample_rate,
                                 0, NULL);
    if (!swr_ctx || swr_init(swr_ctx) < 0) {
        fprintf(stderr, "Could not initialize swresample\n");
        exit(1);
    }
}

// Initialize PulseAudio
void init_pulseaudio() {
    pa_spec.format = PA_SAMPLE_S16LE;
    pa_spec.rate = audio_codec_ctx->sample_rate;
    pa_spec.channels = audio_codec_ctx->channels;
    printf("PulseAudio: rate=%d, channels=%d\n", pa_spec.rate, pa_spec.channels);

    int error;
    pa_stream = pa_simple_new(NULL, "mp4_player", PA_STREAM_PLAYBACK, NULL, "playback", &pa_spec, NULL, NULL, &error);
    if (!pa_stream) {
        fprintf(stderr, "pa_simple_new() failed: %s\n", pa_strerror(error));
        exit(1);
    }

    bytes_per_sample = pa_spec.channels * 2; // 16-bit audio
}

// Initialize OpenGL
void init_opengl() {
    printf("Initializing OpenGL\n");
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

// Render timer text
void render_text() {
    double total_time = (double)fmt_ctx->duration / AV_TIME_BASE;
    double current_time = get_audio_time();
    char text[64];
    snprintf(text, sizeof(text), "%.2f / %.2f", current_time, total_time);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, width, 0, height);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(1, 1, 1);
    glRasterPos2f(10, 10);
    for (char *c = text; *c; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// Display function
void display() {
    printf("Display: frame_count=%d, audio_time=%.2f, eof=%d\n", video_frame_count, get_audio_time(), eof_reached);

    // Decode packets until queue is full or end of file
    if (!eof_reached && video_frame_count < MAX_FRAMES) {
        int ret = av_read_frame(fmt_ctx, &pkt);
        if (ret == AVERROR_EOF) {
            printf("EOF reached\n");
            eof_reached = 1;
            av_packet_unref(&pkt);
        } else if (ret >= 0) {
            if (pkt.stream_index == video_stream_idx) {
                if (avcodec_send_packet(video_codec_ctx, &pkt) >= 0) {
                    AVFrame *frame = av_frame_alloc();
                    if (avcodec_receive_frame(video_codec_ctx, frame) >= 0) {
                        if (video_frame_count < MAX_FRAMES) {
                            video_frames[video_frame_count] = frame;
                            video_pts[video_frame_count] = av_q2d(fmt_ctx->streams[video_stream_idx]->time_base) * frame->pts;
                            printf("Queued video frame %d, PTS=%.2f\n", video_frame_count, video_pts[video_frame_count]);
                            video_frame_count++;
                        } else {
                            av_frame_free(&frame);
                        }
                    } else {
                        av_frame_free(&frame);
                    }
                }
            } else if (pkt.stream_index == audio_stream_idx) {
                if (avcodec_send_packet(audio_codec_ctx, &pkt) >= 0) {
                    AVFrame *audio_frame = av_frame_alloc();
                    if (avcodec_receive_frame(audio_codec_ctx, audio_frame) >= 0) {
                        // Allocate output buffer for converted audio
                        int out_samples = swr_get_out_samples(swr_ctx, audio_frame->nb_samples);
                        uint8_t *out_buffer = av_malloc(out_samples * bytes_per_sample);
                        out_samples = swr_convert(swr_ctx, &out_buffer, out_samples,
                                                 (const uint8_t **)audio_frame->data, audio_frame->nb_samples);
                        if (out_samples > 0) {
                            int data_size = out_samples * bytes_per_sample;
                            int error;
                            if (pa_simple_write(pa_stream, out_buffer, data_size, &error) < 0) {
                                fprintf(stderr, "pa_simple_write failed: %s\n", pa_strerror(error));
                            } else {
                                total_samples_written += out_samples;
                                printf("Wrote %d audio samples, total=%" PRIu64 "\n", out_samples, total_samples_written);
                            }
                        }
                        av_free(out_buffer);
                        av_frame_free(&audio_frame);
                    }
                }
            }
            av_packet_unref(&pkt);
        }
    }

    // Get current time from audio
    double current_time = get_audio_time();

    // Find frame to render
    int frame_idx = -1;
    for (int i = 0; i < video_frame_count; i++) {
        if (video_pts[i] <= current_time) {
            frame_idx = i;
        } else {
            break;
        }
    }

    // Render frame if available
    if (frame_idx >= 0) {
        printf("Rendering frame %d, PTS=%.2f\n", frame_idx, video_pts[frame_idx]);
        AVFrame *frame = video_frames[frame_idx];
        if (frame) {
            uint8_t *dest[4] = {rgb_buffer, NULL, NULL, NULL};
            int dest_linesize[4] = {width * 3, 0, 0, 0};
            sws_scale(sws_ctx, (const uint8_t * const *)frame->data, frame->linesize,
                      0, height, dest, dest_linesize);

            glClear(GL_COLOR_BUFFER_BIT);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texture_id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                         GL_RGB, GL_UNSIGNED_BYTE, rgb_buffer);

            glBegin(GL_QUADS);
            glTexCoord2f(0, 1); glVertex2f(-1, -1);
            glTexCoord2f(1, 1); glVertex2f(1, -1);
            glTexCoord2f(1, 0); glVertex2f(1, 1);
            glTexCoord2f(0, 0); glVertex2f(-1, 1);
            glEnd();

            // Clean up only the rendered frame
            av_frame_free(&video_frames[frame_idx]);
            video_frames[frame_idx] = NULL;
            memmove(video_frames + frame_idx, video_frames + frame_idx + 1, (video_frame_count - frame_idx - 1) * sizeof(AVFrame*));
            memmove(video_pts + frame_idx, video_pts + frame_idx + 1, (video_frame_count - frame_idx - 1) * sizeof(double));
            video_frame_count--;
        }
    }

    // Render timer
    render_text();
    glutSwapBuffers();

    // Check for end of playback
    if (eof_reached && video_frame_count == 0 && get_audio_time() >= (double)fmt_ctx->duration / AV_TIME_BASE - 0.1) {
        printf("Playback complete, exiting\n");
        int error;
        if (pa_simple_drain(pa_stream, &error) < 0) {
            fprintf(stderr, "pa_simple_drain failed: %s\n", pa_strerror(error));
        }
        exit(0);
    }

    glutTimerFunc(10, display, 0);
}

// Cleanup
void cleanup() {
    printf("Cleaning up resources\n");
    if (frame) {
        av_frame_free(&frame);
        frame = NULL;
    }
    if (video_codec_ctx) {
        avcodec_free_context(&video_codec_ctx);
        video_codec_ctx = NULL;
    }
    if (audio_codec_ctx) {
        avcodec_free_context(&audio_codec_ctx);
        audio_codec_ctx = NULL;
    }
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
        fmt_ctx = NULL;
    }
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = NULL;
    }
    if (swr_ctx) {
        swr_free(&swr_ctx);
        swr_ctx = NULL;
    }
    if (rgb_buffer) {
        free(rgb_buffer);
        rgb_buffer = NULL;
    }
    for (int i = 0; i < MAX_FRAMES; i++) {
        if (video_frames[i]) {
            av_frame_free(&video_frames[i]);
            video_frames[i] = NULL;
        }
    }
    video_frame_count = 0;
    if (pa_stream) {
        int error;
        pa_simple_drain(pa_stream, &error);
        pa_simple_free(pa_stream);
        pa_stream = NULL;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <video_file>\n", argv[0]);
        return 1;
    }

    printf("Starting MP4 player\n");
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(640, 480);
    glutCreateWindow("MP4 Player");

    init_ffmpeg(argv[1]);
    init_pulseaudio();
    init_opengl();

    glutDisplayFunc(display);
    glutTimerFunc(0, display, 0);

    atexit(cleanup);
    glutMainLoop();
    return 0;
}
