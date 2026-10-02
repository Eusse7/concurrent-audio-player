#include "miniaudio.h"
#include <iostream>

int main() {
    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, 44100);
    if (ma_decoder_init_file("Creep.mp3", &config, &decoder) != MA_SUCCESS) {
        std::cout << "Error initializing decoder.\n";
        return 1;
    }
    
    float frames[4096 * 2];
    ma_uint64 framesRead = 0;
    ma_result result = ma_decoder_read_pcm_frames(&decoder, frames, 4096, &framesRead);
    std::cout << "Result: " << result << ", Frames read: " << framesRead << "\n";
    ma_decoder_uninit(&decoder);
    return 0;
}
