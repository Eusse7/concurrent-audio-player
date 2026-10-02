#include "audio_engine.h"
#include <iostream>
#include <chrono>
#include <cstring>

void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

AudioEngine::AudioEngine(Playlist* pl) : playlist(pl), head(0), tail(0), count(0), current_frame_offset(0),
    current_amplitude(0.0f), visualizer_active(false),
    flush_requested(false), state(STOPPED), pending_command(CMD_NONE), exit_requested(false) {
}

AudioEngine::~AudioEngine() {
}

void AudioEngine::start() {
    producer_thread = std::thread(AudioEngine::producerLoop, this);
    consumer_thread = std::thread(AudioEngine::consumerLoop, this);
    visualizer_thread = std::thread(AudioEngine::visualizerLoop, this);
}

void AudioEngine::stopEngine() {
    {
        std::unique_lock<std::mutex> lock(state_mutex);
        exit_requested = true;
        state_cond.notify_all();
    }
    
    {
        std::unique_lock<std::mutex> lock(buffer_mutex);
        flush_requested = true;
        not_full.notify_all();
        not_empty.notify_all();
    }

    if (producer_thread.joinable()) producer_thread.join();
    if (consumer_thread.joinable()) consumer_thread.join();
    if (visualizer_thread.joinable()) visualizer_thread.join();
}

void AudioEngine::toggleVisualizer() {
    bool current = visualizer_active.load();
    visualizer_active.store(!current);
}

void AudioEngine::play() {
    std::unique_lock<std::mutex> lock(state_mutex);
    pending_command = CMD_PLAY;
    state_cond.notify_all();
}

void AudioEngine::pause() {
    std::unique_lock<std::mutex> lock(state_mutex);
    pending_command = CMD_PAUSE;
    state_cond.notify_all();
}

void AudioEngine::stop() {
    std::unique_lock<std::mutex> lock(state_mutex);
    pending_command = CMD_STOP;
    state_cond.notify_all();
}

void AudioEngine::next() {
    std::unique_lock<std::mutex> lock(state_mutex);
    pending_command = CMD_NEXT;
    state_cond.notify_all();
}

void AudioEngine::prev() {
    std::unique_lock<std::mutex> lock(state_mutex);
    pending_command = CMD_PREV;
    state_cond.notify_all();
}

void AudioEngine::producerLoop(AudioEngine* engine) {
    Song current_song;
    ma_decoder decoder;
    bool is_decoder_initialized = false;
    std::string current_playing_title = "";

    auto cleanup_decoder = [&]() {
        if (is_decoder_initialized) {
            ma_decoder_uninit(&decoder);
            is_decoder_initialized = false;
        }
    };

    while (true) {
        std::unique_lock<std::mutex> state_lock(engine->state_mutex);

        while (engine->state != PLAYING && !engine->exit_requested && engine->pending_command == CMD_NONE) {
            engine->state_cond.wait(state_lock);
        }

        if (engine->exit_requested) {
            break;
        }

        if (engine->pending_command != CMD_NONE) {
            Command cmd = engine->pending_command;
            engine->pending_command = CMD_NONE;
            
            if (cmd == CMD_PLAY) {
                if (engine->state == STOPPED || engine->state == PAUSED) {
                    engine->state = PLAYING;
                    engine->state_cond.notify_all();
                }
            } else if (cmd == CMD_PAUSE) {
                if (engine->state == PLAYING) {
                    engine->state = PAUSED;
                }
            } else if (cmd == CMD_STOP) {
                engine->state = STOPPED;
                std::unique_lock<std::mutex> buf_lock(engine->buffer_mutex);
                engine->head = engine->tail = engine->count = engine->current_frame_offset = 0;
                engine->flush_requested = true;
                engine->not_full.notify_all();
                cleanup_decoder();
            } else if (cmd == CMD_NEXT) {
                engine->playlist->next();
                engine->state = PLAYING;
                std::unique_lock<std::mutex> buf_lock(engine->buffer_mutex);
                engine->head = engine->tail = engine->count = engine->current_frame_offset = 0;
                engine->flush_requested = true;
                engine->not_full.notify_all();
                cleanup_decoder();
            } else if (cmd == CMD_PREV) {
                engine->playlist->prev();
                engine->state = PLAYING;
                std::unique_lock<std::mutex> buf_lock(engine->buffer_mutex);
                engine->head = engine->tail = engine->count = engine->current_frame_offset = 0;
                engine->flush_requested = true;
                engine->not_full.notify_all();
                cleanup_decoder();
            }
            continue;
        }

        state_lock.unlock();

        if (!engine->playlist->getCurrentSong(current_song)) {
            cleanup_decoder();
            sleep_ms(500); 
            continue;
        }

        if (!is_decoder_initialized || current_playing_title != current_song.title) {
            cleanup_decoder();
            
            ma_decoder_config config = ma_decoder_config_init(ma_format_f32, CHANNELS, 44100);
            if (ma_decoder_init_file(current_song.title.c_str(), &config, &decoder) != MA_SUCCESS) {
                std::cout << "\n[Reproductor] Error: No se pudo abrir el archivo de audio '" << current_song.title << "'\n> " << std::flush;
                sleep_ms(2000); // Esperar un momento antes de saltar para no saturar si no hay canciones validas
                
                std::unique_lock<std::mutex> lock(engine->state_mutex);
                engine->pending_command = CMD_NEXT;
                continue;
            }
            
            is_decoder_initialized = true;
            current_playing_title = current_song.title;
            std::cout << "\n[Reproductor] 🎵 Reproduciendo: " << current_song.title << "\n> " << std::flush;
        }

        AudioFrame new_frame;
        ma_uint64 framesRead = 0;
        
        ma_decoder_read_pcm_frames(&decoder, new_frame.samples, SAMPLES_PER_CHUNK, &framesRead);
        new_frame.valid_frames = (int)framesRead;

        if (framesRead == 0) {
            // EOF reached, wait for buffer to drain
            bool command_received = false;
            while (true) {
                {
                    std::unique_lock<std::mutex> slock(engine->state_mutex);
                    if (engine->exit_requested || engine->pending_command != CMD_NONE) {
                        command_received = true;
                        break;
                    }
                }
                {
                    std::unique_lock<std::mutex> block(engine->buffer_mutex);
                    if (engine->count == 0 || engine->flush_requested) {
                        break;
                    }
                }
                sleep_ms(50);
            }
            
            if (!command_received && !engine->exit_requested) {
                std::unique_lock<std::mutex> lock(engine->state_mutex);
                engine->pending_command = CMD_NEXT;
            }
            continue;
        }

        std::unique_lock<std::mutex> buf_lock(engine->buffer_mutex);
        
        if (engine->flush_requested) {
            engine->flush_requested = false;
            buf_lock.unlock();
            continue;
        }

        while (engine->count == BUFFER_CAPACITY && !engine->flush_requested && !engine->exit_requested) {
            engine->not_full.wait(buf_lock);
        }

        if (engine->exit_requested || engine->flush_requested) {
            if (engine->flush_requested) engine->flush_requested = false;
            buf_lock.unlock();
            continue;
        }

        engine->buffer[engine->tail] = new_frame;
        engine->tail = (engine->tail + 1) % BUFFER_CAPACITY;
        engine->count++;
        
        engine->not_empty.notify_all();
        buf_lock.unlock();
    }
    
    cleanup_decoder();
}

void AudioEngine::consumerLoop(AudioEngine* engine) {
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_f32;
    deviceConfig.playback.channels = CHANNELS;
    deviceConfig.sampleRate        = 44100;
    deviceConfig.dataCallback      = AudioEngine::data_callback;
    deviceConfig.pUserData         = engine;
    
    // Configuración para solucionar glitches en WSL (aumentar buffer)
    deviceConfig.periodSizeInFrames = 4410; // 100ms
    deviceConfig.periods = 3; // 300ms total buffer

    ma_device device;
    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        std::cout << "\n[Reproductor] Error al inicializar el dispositivo de audio.\n> " << std::flush;
        return;
    }
    
    ma_device_start(&device);

    while (true) {
        std::unique_lock<std::mutex> state_lock(engine->state_mutex);
        if (engine->exit_requested) {
            break;
        }
        state_lock.unlock();
        sleep_ms(100);
    }

    ma_device_uninit(&device);
}

void AudioEngine::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pInput;
    AudioEngine* engine = (AudioEngine*)pDevice->pUserData;
    float* pOut = (float*)pOutput;
    
    bool is_playing = false;
    {
        std::unique_lock<std::mutex> state_lock(engine->state_mutex);
        is_playing = (engine->state == PLAYING);
    }

    if (!is_playing) {
        std::memset(pOut, 0, frameCount * CHANNELS * sizeof(float));
        return;
    }

    ma_uint32 framesToRead = frameCount;
    ma_uint32 outOffset = 0;
    
    std::unique_lock<std::mutex> buf_lock(engine->buffer_mutex);
    
    while (framesToRead > 0 && engine->count > 0) {
        AudioFrame& frame = engine->buffer[engine->head];
        
        int available = frame.valid_frames - engine->current_frame_offset;
        int toCopy = (available < (int)framesToRead) ? available : framesToRead;
        
        if (toCopy > 0) {
            std::memcpy(pOut + (outOffset * CHANNELS), 
                        frame.samples + (engine->current_frame_offset * CHANNELS), 
                        toCopy * CHANNELS * sizeof(float));
            
            engine->current_frame_offset += toCopy;
            outOffset += toCopy;
            framesToRead -= toCopy;
        }
        
        if (engine->current_frame_offset >= frame.valid_frames) {
            engine->head = (engine->head + 1) % BUFFER_CAPACITY;
            engine->count--;
            engine->current_frame_offset = 0;
            engine->not_full.notify_all();
        }
    }
    
    buf_lock.unlock();
    
    if (framesToRead > 0) {
        std::memset(pOut + (outOffset * CHANNELS), 0, framesToRead * CHANNELS * sizeof(float));
    }
    
    // Calcular amplitud (Peak)
    float peak = 0.0f;
    for (ma_uint32 i = 0; i < frameCount * CHANNELS; ++i) {
        float val = pOut[i];
        if (val < 0) val = -val;
        if (val > peak) peak = val;
    }
    engine->current_amplitude.store(peak);
}

#include <random>

void AudioEngine::visualizerLoop(AudioEngine* engine) {
    const int NUM_BARS = 30;
    const int MAX_HEIGHT = 8;
    std::mt19937 gen(1337);
    
    while (true) {
        bool should_exit = false;
        {
            std::unique_lock<std::mutex> state_lock(engine->state_mutex);
            should_exit = engine->exit_requested;
        }
        if (should_exit) break;
        
        if (engine->visualizer_active.load() && engine->state == PLAYING) {
            float peak = engine->current_amplitude.load() * 3.0f;
            if (peak > 1.0f) peak = 1.0f;
            
            int heights[NUM_BARS];
            for (int i = 0; i < NUM_BARS; i++) {
                float distance = std::abs(i - (NUM_BARS / 2.0f)) / (NUM_BARS / 2.0f);
                float bell = 1.0f - (distance * distance); // Parabola (campana invertida)
                if (bell < 0) bell = 0;
                
                float noise = (gen() % 100) / 100.0f;
                heights[i] = (int)(peak * bell * MAX_HEIGHT * (0.3f + noise * 0.7f));
            }
            
            std::string output = "\n";
            for (int h = MAX_HEIGHT; h >= 1; h--) {
                output += "    ";
                for (int i = 0; i < NUM_BARS; i++) {
                    if (heights[i] >= h) {
                        if (h >= 6) output += "\033[35m"; // Magenta
                        else if (h >= 3) output += "\033[34m"; // Azul
                        else output += "\033[36m"; // Cyan
                        output += "██ \033[0m";
                    } else {
                        output += "   ";
                    }
                }
                output += "\n";
            }
            output += "\n    \033[33m(Escribe 'v' y presiona ENTER para regresar)\033[0m\n";
            std::cout << output;
            std::cout << "\033[" << (MAX_HEIGHT + 3) << "A" << std::flush;
        }
        sleep_ms(50);
    }
}
