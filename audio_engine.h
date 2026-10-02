#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "playlist.h"
#include "miniaudio.h"

enum PlaybackState {
    STOPPED,
    PLAYING,
    PAUSED
};

enum Command {
    CMD_NONE,
    CMD_PLAY,
    CMD_PAUSE,
    CMD_STOP,
    CMD_NEXT,
    CMD_PREV
};

#define BUFFER_CAPACITY 100
#define SAMPLES_PER_CHUNK 4096
#define CHANNELS 2

struct AudioFrame {
    float samples[SAMPLES_PER_CHUNK * CHANNELS];
    int valid_frames;
};

class AudioEngine {
private:
    Playlist* playlist;
    
    // Threads
    std::thread producer_thread;
    std::thread consumer_thread;
    
    // --- FX SYSTEM (Second Producer) ---
    AudioFrame fx_buffer[BUFFER_CAPACITY];
    int fx_head;
    int fx_tail;
    int fx_count;
    int fx_current_frame_offset;
    bool fx_flush_requested;
    
    std::mutex fx_buffer_mutex;
    std::condition_variable fx_not_full;
    std::condition_variable fx_not_empty;
    
    std::string fx_file_to_play;
    bool fx_play_requested;
    std::mutex fx_state_mutex;
    std::condition_variable fx_state_cond;
    std::thread fx_producer_thread;
    static void fxProducerLoop(AudioEngine* engine);
    // -----------------------------------
    
    // Circular buffer for audio frames
    AudioFrame buffer[BUFFER_CAPACITY];
    int head;
    int tail;
    int count;
    int current_frame_offset;
    
    // Visualizer
    std::atomic<float> current_amplitude;
    std::atomic<bool> visualizer_active;
    std::thread visualizer_thread;
    static void visualizerLoop(AudioEngine* engine);
    
    // Synchronization for buffer
    std::mutex buffer_mutex;
    std::condition_variable not_full;
    std::condition_variable not_empty;
    bool flush_requested;

    // Synchronization for state
    std::mutex state_mutex;
    std::condition_variable state_cond;
    PlaybackState state;
    Command pending_command;
    bool exit_requested;

    // Internal methods
    static void producerLoop(AudioEngine* engine);
    static void consumerLoop(AudioEngine* engine);
    static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    
public:
    AudioEngine(Playlist* pl);
    ~AudioEngine();

    void start();
    void stopEngine();

    // Async commands from CLI
    void play();
    void pause();
    void stop();
    void next();
    void prev();
    
    void playEffect(const std::string& filename);
    
    void toggleVisualizer();
    bool isVisualizerActive() const { return visualizer_active.load(); }
};

#endif
