#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <winbase.h>
static const char* graph_bins[] = {" ", ".", ":", "-", "=", "+", "*", "#"};
#else
#include <sys/resource.h>
#include <sys/utsname.h>
static const char* graph_bins[] = {" ", "⣀", "⣄", "⣤", "⣦", "⣶", "⣿", "⣿"};
#endif

#define MAX_SAMPLES 65536
#define NUM_BINS 30
#define LUT_SIZE 2048

typedef struct {
    atomic_size_t idx;
    double execTimes[MAX_SAMPLES];
    double jitterTimes[MAX_SAMPLES];
    float cpuSamples[1000];
    int cpuIdx;
    float phase;
    float phaseInc;
    ma_timer timer;
    double last_callback_time;
    atomic_int underrun_count;
    int recording;
    int duration_sec;
} BenchData;

static float sineLUT[LUT_SIZE];
static BenchData g_bench;

double get_process_cpu_time() {
#ifdef _WIN32
    FILETIME create, exit, kernel, user;
    GetProcessTimes(GetCurrentProcess(), &create, &exit, &kernel, &user);
    return (double)(*((unsigned long long*)&kernel) + *((unsigned long long*)&user)) / 10000000.0;
#else
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return (double)usage.ru_utime.tv_sec + (double)usage.ru_utime.tv_usec / 1000000.0 +
           (double)usage.ru_stime.tv_sec + (double)usage.ru_stime.tv_usec / 1000000.0;
#endif
}

void* cpu_monitor_thread(void* arg) {
    BenchData *b = (BenchData*)arg;
    double last = get_process_cpu_time();
    struct timespec ts = {0,100000000};

    while(b->recording) {
        nanosleep(&ts,NULL);
        double now = get_process_cpu_time();
        if(b->cpuIdx < 1000)
            b->cpuSamples[b->cpuIdx++] = (float)((now-last)*10.0);
        last = now;
    }
    return NULL;
}

void init_lut() {
    for(int i=0;i<LUT_SIZE;i++)
        sineLUT[i] = sinf((2.0f * 3.141592653589793f * i) / LUT_SIZE);
}

void print_system_info(int duration) {
#ifdef _WIN32
    printf("Miniaudio: %s | OS: Windows | Arch: x64 | Duration: %d sec\n",
           MA_VERSION_STRING,duration);
#else
    struct utsname buffer;
    uname(&buffer);
    printf("Miniaudio: %s | OS: %s | Arch: %s | Duration: %d sec\n",
           MA_VERSION_STRING,buffer.sysname,buffer.machine,duration);
#endif
    printf("--------------------------------------------------------------------------\n");
}

static void render_histogram(const char* label, double* samples, size_t count)
{
    if(count == 0) {
        printf("%-6s (no data)\n", label);
        return;
    }

    double min=1e9,max=-1e9;

    for(size_t i=0;i<count;i++){
        if(samples[i]<min) min=samples[i];
        if(samples[i]>max) max=samples[i];
    }

    double range=max-min;
    if(range<=1e-12) range=1e-12;

    double bins[NUM_BINS]={0};

    for(size_t i=0;i<count;i++){
        int bin=(int)(((samples[i]-min)/range)*NUM_BINS);
        if(bin<0) bin=0;
        if(bin>=NUM_BINS) bin=NUM_BINS-1;
        bins[bin]+=1.0;
    }

    printf("%-6s %8.4f [",label,min);

    for(int i=0;i<NUM_BINS;i++){
        double level=bins[i]/count;
        int idx=(int)(level*7.0);
        if(idx<0) idx=0;
        if(idx>7) idx=7;
        printf("%s",graph_bins[idx]);
    }

    printf("] %8.4f\n",max);
}

static void render_stats(BenchData *b, double period_ms)
{
    size_t count = (b->idx < MAX_SAMPLES)? b->idx : MAX_SAMPLES;

    double early[MAX_SAMPLES];
    double late[MAX_SAMPLES];
    size_t earlyCount = 0;
    size_t lateCount = 0;

    for(size_t i=0;i<count;i++) {
        double j = b->jitterTimes[i];
        if(j < 0)
            early[earlyCount++] = -j;
        else if(j > 0)
            late[lateCount++] = j;
    }

    double cpuArray[1000];
    int cpuCount=b->cpuIdx;

    for(int i=0;i<cpuCount;i++)
        cpuArray[i]=b->cpuSamples[i];

    render_histogram("exec",b->execTimes,count);
    render_histogram("jitter",b->jitterTimes,count);
    render_histogram("early",early,earlyCount);
    render_histogram("late",late,lateCount);
    render_histogram("cpu",cpuArray,cpuCount);

    printf("underruns: %d\n",atomic_load(&b->underrun_count));

    double max_exec = 0;
    for(size_t i=0;i<count;i++)
        if(b->execTimes[i] > max_exec)
            max_exec = b->execTimes[i];

    if(max_exec > period_ms*0.8)
        printf("callback budget status: FAIL\n");
    else
        printf("callback budget status: PASS\n");
}

static void audio_callback(ma_device* pDevice, void* pOutput,
                           const void* pInput, ma_uint32 frameCount)
{
    BenchData *b=(BenchData*)pDevice->pUserData;
    if(!b || !b->recording) return;

    double t0 = ma_timer_get_time_in_seconds(&b->timer)*1000.0;

    double expected_ms =
        (double)pDevice->playback.internalPeriodSizeInFrames /
        pDevice->sampleRate * 1000.0;

    double interval=0;

    if(b->last_callback_time>0)
        interval = t0 - b->last_callback_time;

    b->last_callback_time=t0;

    size_t idx = atomic_fetch_add(&b->idx,1);
    int record = (idx < MAX_SAMPLES);

    if(interval > expected_ms*1.5)
        atomic_fetch_add(&b->underrun_count,1);

    if(pDevice->playback.format==ma_format_f32){

        float *out=(float*)pOutput;

        for(ma_uint32 i=0;i<frameCount;i++){
            float s = sineLUT[(int)b->phase];
            *out++ = s;
            *out++ = s;

            b->phase += b->phaseInc;
            if(b->phase>=LUT_SIZE) b->phase-=LUT_SIZE;
        }

    } else {

        int16_t *out=(int16_t*)pOutput;

        for(ma_uint32 i=0;i<frameCount;i++){
            int16_t s=(int16_t)(sineLUT[(int)b->phase]*32767.0f);
            *out++ = s;
            *out++ = s;

            b->phase += b->phaseInc;
            if(b->phase>=LUT_SIZE) b->phase-=LUT_SIZE;
        }
    }

    double t1 = ma_timer_get_time_in_seconds(&b->timer)*1000.0;

    if(record){
        b->execTimes[idx] = t1 - t0;
        b->jitterTimes[idx] = (interval>0)? interval-expected_ms : 0.0;
    }
}

void run_test(ma_format format, ma_uint32 rate, ma_uint32 bufSize)
{
    g_bench.idx=0;
    g_bench.cpuIdx=0;
    g_bench.recording=0;
    g_bench.underrun_count=0;
    g_bench.last_callback_time=0;

    g_bench.phase=0;
    g_bench.phaseInc = (440.0f * LUT_SIZE)/(float)rate;

    ma_context ctx;
    ma_context_init(NULL,0,NULL,&ctx);

    ma_device_config cfg =
        ma_device_config_init(ma_device_type_playback);

    cfg.playback.format=format;
    cfg.sampleRate=rate;
    cfg.periodSizeInFrames=bufSize;
    cfg.dataCallback=audio_callback;
    cfg.pUserData=&g_bench;

    ma_device dev;

    if(ma_device_init(&ctx,&cfg,&dev)==MA_SUCCESS){

        ma_timer_init(&g_bench.timer);

        pthread_t tid;

        ma_device_start(&dev);

        g_bench.recording=1;

        pthread_create(&tid,NULL,cpu_monitor_thread,&g_bench);

        sleep(g_bench.duration_sec);

        g_bench.recording=0;

        pthread_join(tid,NULL);

        ma_device_stop(&dev);

        double period_ms = (double)bufSize*1000.0/rate;

        printf("Fmt: %s, Rate: %u, Buf: %u, CBs: %zu\n",
            (format==ma_format_f32?"f32":"s16"),
            rate,bufSize,g_bench.idx);

        render_stats(&g_bench,period_ms);

        printf("--------------------------------------------------------------------------\n");

        ma_device_uninit(&dev);
    }

    ma_context_uninit(&ctx);
}

int main(int argc,char** argv)
{
    int duration = (argc>1)? atoi(argv[1]) : 2;

    g_bench.duration_sec = duration;

    print_system_info(duration);

    init_lut();

    ma_format fmts[] = {ma_format_f32,ma_format_s16};
    ma_uint32 rates[] = {44100,48000};
    ma_uint32 bufs[]  = {128,256,512,1024};

    for(int f=0;f<2;f++)
        for(int r=0;r<2;r++)
            for(int b=0;b<4;b++)
                run_test(fmts[f],rates[r],bufs[b]);

    return 0;
}
