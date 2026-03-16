/*
ma-bench.c - Miniaudio callback benchmark
- Works on Linux/macOS
- f32 and s16 output
- Precomputed sine table with fractional indexing
- Callback benchmarking (avg/min/max + jitter)
- DSP load %
- UTF-8 histograms
- OS + CPU info + Miniaudio version
- Command-line test duration
*/

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <sys/utsname.h>

#define HIST_BINS 32
#define HIST_WIDTH 42
#define SINE_TABLE_SIZE 2048
#define DEFAULT_TEST_SECONDS 5

typedef struct {
    double min, max, sum;
    uint64_t count;
} stat_t;

typedef struct {
    stat_t exec;
    stat_t jitter;
    double exec_hist[HIST_BINS];
    double jitter_hist[HIST_BINS];
    double period_ms;
} dsp_meter_t;

typedef struct {
    ma_device device;
    dsp_meter_t meter;
    float sine[SINE_TABLE_SIZE];
    double sine_pos;
    double last_start;
    uint64_t callbacks;
    double freq; // sine frequency
} bench_state;

/* --- Utility --- */
static double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec*1000.0 + ts.tv_nsec/1e6;
}

static void stat_init(stat_t *s) { s->min=1e9; s->max=0; s->sum=0; s->count=0; }
static void stat_push(stat_t *s,double v){ if(v<s->min)s->min=v; if(v>s->max)s->max=v; s->sum+=v; s->count++; }
static double stat_avg(stat_t *s){ return s->count ? s->sum/s->count : 0; }

static void meter_init(dsp_meter_t *m,double period_ms){
    stat_init(&m->exec);
    stat_init(&m->jitter);
    memset(m->exec_hist,0,sizeof(m->exec_hist));
    memset(m->jitter_hist,0,sizeof(m->jitter_hist));
    m->period_ms = period_ms;
}

static void hist_push(double *hist,double min,double max,double v){
    if(v<min)v=min;
    if(v>max)v=max;
    int bin=(int)((v-min)/(max-min)*(HIST_BINS-1));
    if(bin<0) bin=0;
    if(bin>=HIST_BINS) bin=HIST_BINS-1;
    hist[bin]++;
}

static void meter_record_exec(dsp_meter_t *m,double v){ stat_push(&m->exec,v); hist_push(m->exec_hist,0,m->period_ms,v);}
static void meter_record_jitter(dsp_meter_t *m,double v){ stat_push(&m->jitter,v); hist_push(m->jitter_hist,0,m->period_ms,v); }

static void render_hist(double *hist,double min,double max){
    double maxv=0; for(int i=0;i<HIST_BINS;i++) if(hist[i]>maxv) maxv=hist[i]; if(maxv==0) maxv=1;
    printf("├");
    for(int x=0;x<HIST_WIDTH;x++){
        int bin=(x*HIST_BINS)/HIST_WIDTH;
        double v=hist[bin]/maxv;
        const char* c=" ";
        if(v>0.85)c="⣿";
        else if(v>0.65)c="⣷";
        else if(v>0.45)c="⣯";
        else if(v>0.25)c="⣟";
        else if(v>0.10)c="⣮";
        else if(v>0.02)c="⣀";
        printf("%s",c);
    }
    printf("┤");
}

/* --- Audio callback --- */
static void audio_callback(ma_device* dev, void* out, const void* in, ma_uint32 frames){
    bench_state* s = (bench_state*)dev->pUserData;
    double start=now_ms();

    if(s->last_start!=0){
        double expected=s->last_start+s->meter.period_ms;
        meter_record_jitter(&s->meter,fabs(start-expected));
    }
    s->last_start=start;

    uint32_t channels=dev->playback.channels;
    double step = SINE_TABLE_SIZE * s->freq / dev->sampleRate;

    for(ma_uint32 i=0;i<frames;i++){
        int idx = ((int)s->sine_pos) & (SINE_TABLE_SIZE-1);
        float sample = s->sine[idx];
        s->sine_pos += step;

        for(uint32_t ch=0; ch<channels; ch++){
            if(dev->playback.format==ma_format_f32) ((float*)out)[i*channels+ch]=sample;
            else if(dev->playback.format==ma_format_s16) ((int16_t*)out)[i*channels+ch]=(int16_t)(sample*32767.0f);
        }
    }

    double end=now_ms();
    meter_record_exec(&s->meter,end-start);
    s->callbacks++;
    (void)in;
}

/* --- Precompute sine table --- */
static void precompute_sine(bench_state* s){
    for(int i=0;i<SINE_TABLE_SIZE;i++)
        s->sine[i]=sin((double)i/SINE_TABLE_SIZE*2.0*M_PI);
}

/* --- Print system info --- */
static void print_system_info(){
    struct utsname u;
    uname(&u);
    printf("Miniaudio version: %s\n",MA_VERSION_STRING);
    printf("OS: %s %s\n",u.sysname,u.release);
    printf("CPU: %s\n\n",u.machine);
}

/* --- Print benchmark results --- */
static void print_results(bench_state* s){
    double avg_exec=stat_avg(&s->meter.exec);
    double load=avg_exec/s->meter.period_ms*100.0;
    printf("exec   %7.4f ms ",s->meter.exec.min); render_hist(s->meter.exec_hist,0,s->meter.period_ms); printf(" %7.4f ms\n",s->meter.exec.max);
    printf("jitter %7.4f ms ",s->meter.jitter.min); render_hist(s->meter.jitter_hist,0,s->meter.period_ms); printf(" %7.4f ms\n",s->meter.jitter.max);
    printf("DSP load avg %.2f%%\n",load);
}

/* --- Run one test --- */
static void run_test(ma_format fmt,int rate,int frames,int seconds,double freq){
    bench_state s; memset(&s,0,sizeof(s));
    s.freq=freq;
    precompute_sine(&s);
    double period_ms=(double)frames/rate*1000.0;
    meter_init(&s.meter,period_ms);

    ma_device_config cfg=ma_device_config_init(ma_device_type_playback);
    cfg.playback.format=fmt;
    cfg.playback.channels=2;
    cfg.sampleRate=rate;
    cfg.periodSizeInFrames=frames;
    cfg.dataCallback=audio_callback;
    cfg.pUserData=&s;

    if(ma_device_init(NULL,&cfg,&s.device)!=MA_SUCCESS){ printf("Device init failed\n"); return; }
    if(ma_device_start(&s.device)!=MA_SUCCESS){ printf("Device start failed\n"); ma_device_uninit(&s.device); return; }

    ma_sleep(seconds*1000);
    ma_device_uninit(&s.device);

    double cps=s.callbacks/(double)seconds;
    printf("%5s | %6d Hz | %5d frames | %6.0f cb/s | %.1f Hz\n",
           fmt==ma_format_f32?"f32":"s16",rate,frames,cps,freq);
    print_results(&s);
    printf("\n");
}

/* --- Main --- */
int main(int argc,char** argv){
    int test_seconds=DEFAULT_TEST_SECONDS;
    if(argc>1) test_seconds=atoi(argv[1]);

    print_system_info();
    printf("Format | SampleRate | Period | Callbacks/sec | SineHz\n");
    printf("------------------------------------------------------\n\n");

    ma_format formats[]={ma_format_f32,ma_format_s16};
    int rates[]={44100,48000};
    int frames[]={128,256,512,1024};
    double freq = 440.0; // A4

    for(int f=0;f<2;f++)
        for(int r=0;r<2;r++)
            for(int p=0;p<4;p++)
                run_test(formats[f],rates[r],frames[p],test_seconds,freq);

    return 0;
}
