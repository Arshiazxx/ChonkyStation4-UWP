#include "ScePlayGo.hpp"
#include <Logger.hpp>
#include <Loaders/Module.hpp>
#include <OS/Libraries/SceSystemService/SceSystemService.hpp>
#include <OS/Filesystem.hpp>


namespace PS4::OS::Libs::ScePlayGo {

MAKE_LOG_FUNCTION(log, lib_scePlayGo);

void init(Module& module) {
    module.addSymbolExport("ts6GlZOKRrE", "scePlayGoInitialize", "libScePlayGo", "libScePlayGo", (void*)&scePlayGoInitialize);
    module.addSymbolExport("M1Gma1ocrGE", "scePlayGoOpen", "libScePlayGo", "libScePlayGo", (void*)&scePlayGoOpen);
    module.addSymbolExport("73fF1MFU8hA", "scePlayGoGetChunkId", "libScePlayGo", "libScePlayGo", (void*)&scePlayGoGetChunkId);
    module.addSymbolExport("uWIYLFkkwqk", "scePlayGoGetLocus", "libScePlayGo", "libScePlayGo", (void*)&scePlayGoGetLocus);
    module.addSymbolExport("-RJWNMK3fC8", "scePlayGoGetProgress", "libScePlayGo", "libScePlayGo", (void*)&scePlayGoGetProgress);
    module.addSymbolExport("Nn7zKwnA5q0", "scePlayGoGetToDoList", "libScePlayGo", "libScePlayGo", (void*)&scePlayGoGetToDoList);
    module.addSymbolExport("gUPGiOQ1tmQ", "scePlayGoSetToDoList", "libScePlayGo", "libScePlayGo", (void*)&scePlayGoSetToDoList);
    module.addSymbolExport("v6EZ-YWRdMs", "scePlayGoGetEta", "libScePlayGo", "libScePlayGo", (void*)&scePlayGoGetEta);
    module.addSymbolExport("3OMbYZBaa50", "scePlayGoGetLanguageMask", "libScePlayGo", "libScePlayGo", (void*)&scePlayGoGetLanguageMask);
    module.addSymbolExport("LosLlHOpNqQ", "scePlayGoSetLanguageMask", "libScePlayGo", "libScePlayGo", (void*)&scePlayGoSetLanguageMask);
    
    module.addSymbolStub("-Q1-u1a7p0g", "scePlayGoPrefetch", "libScePlayGo", "libScePlayGo");
    module.addSymbolStub("4AAcTU9R3XM", "scePlayGoSetInstallSpeed", "libScePlayGo", "libScePlayGo");
    module.addSymbolStub("rvBSfTimejE", "scePlayGoGetInstallSpeed", "libScePlayGo", "libScePlayGo");
    module.addSymbolStub("Uco1I0dlDi8", "scePlayGoClose", "libScePlayGo", "libScePlayGo");
    module.addSymbolStub("MPe0EeBGM-E", "scePlayGoTerminate", "libScePlayGo", "libScePlayGo");
}

ScePlayGoInitParams param;
ScePlayGoLanguageMask lang_mask = 0;

// Struct definitions from shadPS4
struct chunk_t {
    u32 offset;
    u32 length;
} __attribute__((packed));

struct PlayGoHeader {
    u32 magic;

    u16 version_major;
    u16 version_minor;
    u16 image_count;    // [0;1]
    u16 chunk_count;    // [0;1000]
    u16 mchunk_count;   // [0;8000]
    u16 scenario_count; // [0;32]

    u32 file_size;
    u16 default_scenario_id;
    u16 attrib;
    u32 sdk_version;
    u16 disc_count; // [0;2] (if equals to 0 then disc count = 1)
    u16 layer_bmp;

    u8 reserved[32];
    char content_id[128];

    chunk_t chunk_attrs; // [0;32000]
    chunk_t chunk_mchunks;
    chunk_t chunk_labels;   // [0;16000]
    chunk_t mchunk_attrs;   // [0;12800]
    chunk_t scenario_attrs; // [0;1024]
    chunk_t scenario_chunks;
    chunk_t scenario_labels;
    chunk_t inner_mchunk_attrs; // [0;12800]
} __attribute__((packed));

struct PlayGoChunk {
    u64 req_locus;
    u64 language_mask;
    u64 total_size;
    std::string label_name;
};

struct image_disc_layer_no_t {
    u8 layer_no : 2;
    u8 disc_no : 2;
    u8 image_no : 4;
} __attribute__((packed));

struct playgo_chunk_attr_entry_t {
    u8 flag;
    image_disc_layer_no_t image_disc_layer_no;
    u8 req_locus;
    u8 unk[11];
    u16 mchunk_count;
    u64 language_mask;
    u32 mchunks_offset; //<-chunk_mchunks
    u32 label_offset;   //<-chunk_labels
} __attribute__((packed));

struct playgo_chunk_loc_t {
    u64 offset : 48;
    u64 _align1 : 8;
    u64 image_no : 4;
    u64 _align2 : 4;
} __attribute__((packed));

struct playgo_chunk_size_t {
    u64 size : 48;
    u64 _align : 16;
} __attribute__((packed));

struct playgo_mchunk_attr_entry_t {
    playgo_chunk_loc_t loc;
    playgo_chunk_size_t size;
} __attribute__((packed));

std::vector<PlayGoChunk> chunks;

inline ScePlayGoLanguageMask PS4_FUNC scePlayGoConvertLanguage(s32 system_lang) {
    return (system_lang >= 0 && system_lang < 48) ? (1ull << (64 - system_lang - 1)) : 0;
}

s32 PS4_FUNC scePlayGoInitialize(const ScePlayGoInitParams* init_param) {
    log("scePlayGoInitialize(init_param=*%p)\n", init_param);
    param = *init_param;

    s32 lang = 0;
    SceSystemService::sceSystemServiceParamGetInt(SceSystemService::SCE_SYSTEM_SERVICE_PARAM_ID_LANG, &lang);
    lang_mask = scePlayGoConvertLanguage(lang);
    return SCE_OK;
}

s32 PS4_FUNC scePlayGoOpen(ScePlayGoHandle* out_handle, const void* param) {
    log("scePlayGoOpen(out_handle=*%p, param=%p)\n", out_handle, param);

    // Open file
    const auto path = FS::guestPathToHost("/app0/sce_sys/playgo-chunk.dat");
    if (!fs::exists(path))
        Helpers::panic("scePlayGoOpen called but playgo-chunk.dat is missing\n");

    auto file = Helpers::readBinary(path);
    PlayGoHeader* header = (PlayGoHeader*)file.data();
    playgo_chunk_attr_entry_t* chunk_attrs      = (playgo_chunk_attr_entry_t*)&file[header->chunk_attrs.offset];
    u16* chunk_mchunks                          = (u16*)&file[header->chunk_mchunks.offset];
    char* chunk_labels                          = (char*)&file[header->chunk_labels.offset];
    playgo_mchunk_attr_entry_t* mchunk_attrs    = (playgo_mchunk_attr_entry_t*)&file[header->mchunk_attrs.offset];

    // PlayGo chunk parser adapter from shadPS4
    chunks.resize(header->chunk_count);
    for (int i = 0; i < header->chunk_count; i++) {
        chunks[i].req_locus     = chunk_attrs[i].req_locus;
        chunks[i].language_mask = chunk_attrs[i].language_mask;
        chunks[i].label_name    = chunk_labels + chunk_attrs[i].label_offset;

        u64 total_size = 0;
        if (chunk_attrs[i].mchunk_count > 0) {
            u16* mchunks = (u16*)((u8*)chunk_mchunks + chunk_attrs[i].mchunks_offset);
            for (int j = 0; j < chunk_attrs[i].mchunk_count; j++) {
                total_size += mchunk_attrs[mchunks[j]].size.size;
            }
        }
        chunks[i].total_size = total_size;
    }

    log("Parsed playgo-chunk.dat\n");
    for (auto& chunk : chunks) {
        log("%s\t%.2f mb (%d bytes)\n", chunk.label_name.c_str(), (float)chunk.total_size / 1_MB, chunk.total_size);
    }

    *out_handle = PLAYGO_HANDLE;
    return SCE_OK;
}

s32 PS4_FUNC scePlayGoGetChunkId(ScePlayGoHandle handle, ScePlayGoChunkId* out_chunk_id_list, u32 n_entries, u32* out_n_entries) {
    log("scePlayGoGetChunkId(handle=0x%x, out_chunk_id_list=*%p, n_entries=%d, out_n_entries=%d)\n", handle, out_chunk_id_list, n_entries, out_n_entries);

    // If the actual IDs aren't requested, only return the number of chunks
    if (!out_chunk_id_list) {
        *out_n_entries = chunks.size();
        return SCE_OK;;
    }

    const auto count = std::min(chunks.size(), (size_t)n_entries);
    for (int i = 0; i < count; i++)
        out_chunk_id_list[i] = i;

    *out_n_entries = count;
    return SCE_OK;
}

s32 PS4_FUNC scePlayGoGetLocus(ScePlayGoHandle handle, const ScePlayGoChunkId* chunk_ids, u32 n_entries, ScePlayGoLocus* out_loci) {
    log("scePlayGoGetLocus(handle=0x%x, chunk_ids=*%p, n_entries=%d, out_loci=*%p)\n", handle, chunk_ids, n_entries, out_loci);

    for (int i = 0; i < n_entries; i++) {
        if (chunk_ids[i] >= chunks.size()) {
            out_loci[i] = (ScePlayGoLocus)ScePlayGoLocusValue::NotDownloaded;
            return SCE_PLAYGO_ERROR_BAD_CHUNK_ID;
        }
        out_loci[i] = (ScePlayGoLocus)ScePlayGoLocusValue::LocalFast;
    }
    return SCE_OK;
}

s32 PS4_FUNC scePlayGoGetProgress(ScePlayGoHandle handle, const ScePlayGoChunkId* chunk_ids, u32 n_entries, ScePlayGoProgress* out_progress) {
    log("scePlayGoGetProgress(handle=0x%x, chunk_ids=*%p, n_entries=%d, out_progress=*%p)\n", handle, chunk_ids, n_entries, out_progress);

    u64 total_size = 0;
    for (int i = 0; i < n_entries; i++) {
        if (chunk_ids[i] >= chunks.size()) {
            Helpers::panic("scePlayGoGetProgress: out of bounds chunk id %d (size was %d)\n", chunk_ids[i], chunks.size());
        }
        total_size += chunks[chunk_ids[i]].total_size;
    }

    out_progress->progress_size = total_size;
    out_progress->total_size = total_size;
    return SCE_OK;
}

s32 PS4_FUNC scePlayGoGetToDoList(ScePlayGoHandle handle, ScePlayGoToDo* out_todo_list, u32 n_entries, u32* n_out_entries) {
    log("scePlayGoGetToDoList(handle=0x%x, out_todo_list=*%p, n_entries=%d, n_out_entries=*%p)\n", handle, out_todo_list, n_entries, n_out_entries);

    *n_out_entries = 0;
    return SCE_OK;
}

s32 PS4_FUNC scePlayGoSetToDoList(ScePlayGoHandle handle, const ScePlayGoToDo* todo_list, u32 n_entries) {
    log("scePlayGoSetToDoList(handle=0x%x, todo_list=*%p, n_entries=%d)\n", handle, todo_list, n_entries);
    return SCE_OK;
}

s32 PS4_FUNC scePlayGoGetEta(ScePlayGoHandle handle, const ScePlayGoChunkId* chunk_ids, u32 n_entries, ScePlayGoEta* out_eta) {
    log("scePlayGoGetEta(handle=0x%x, chunk_ids=*%p, n_entries=%d, out_eta=*%p)\n", handle, chunk_ids, n_entries, out_eta);

    *out_eta = 0;
    return SCE_OK;
}

s32 PS4_FUNC scePlayGoGetLanguageMask(ScePlayGoHandle handle, ScePlayGoLanguageMask* out_mask) {
    log("scePlayGoGetLanguageMask(handle=0x%x, out_mask=*%p)\n", handle, out_mask);

    *out_mask = lang_mask;
    return SCE_OK;
}

s32 PS4_FUNC scePlayGoSetLanguageMask(ScePlayGoHandle handle, ScePlayGoLanguageMask mask) {
    log("scePlayGoSetLanguageMask(handle=0x%x, mask=*%p)\n", handle, mask);

    lang_mask = mask;
    return SCE_OK;
}

}   // End namespace PS4::OS::Libs::ScePlayGo