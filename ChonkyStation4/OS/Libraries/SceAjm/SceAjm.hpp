#pragma once

#include <Common.hpp>


class Module;

namespace PS4::OS::Libs::SceAjm {

void init(Module& module);

enum SceAjmDecMp3OflType {
    SCE_AJM_DEC_MP3_OFL_TYPE_NONE           = 0,
    SCE_AJM_DEC_MP3_OFL_TYPE_LAME           = 1,
    SCE_AJM_DEC_MP3_OFL_TYPE_VBRI           = 2,
    SCE_AJM_DEC_MP3_OFL_TYPE_FGH            = 3,
    SCE_AJM_DEC_MP3_OFL_TYPE_VBRI_AND_FGH   = 4
};

struct SceAjmDecMp3ParseFrame {
    size_t frame_size;
    unsigned int n_channels;
    unsigned int samples_per_channel;
    unsigned int bitrate;
    unsigned int sample_rate;
    unsigned int ofl_encoder_delay;
    unsigned int ofl_num_frames;
    unsigned int ofl_total_samples;
    SceAjmDecMp3OflType ofl_type;
};

s32 PS4_FUNC sceAjmDecMp3ParseFrame(const void* buf, size_t size, int parse_ofl, SceAjmDecMp3ParseFrame* frame_info);

}   // End namespace PS4::OS::Libs::SceAjm