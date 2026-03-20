#ifndef SOLOUD_FAKE_MP3_H
#define SOLOUD_FAKE_MP3_H

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#include "audiosource/wav/dr_wav.h"

#define DRMP3_API static inline
#define DR_MP3_NO_STDIO
#include "audiosource/wav/dr_mp3.h"

#define DRFLAC_API static inline
#define DR_FLAC_NO_STDIO
#define DR_FLAC_NO_CRC
#include "audiosource/wav/dr_flac.h"

DRMP3_API void
drmp3_uninit(drmp3* pMP3) {
}

DRMP3_API drmp3_bool32
drmp3_init(drmp3* pMP3, drmp3_read_proc onRead, drmp3_seek_proc onSeek, void* pUserData, const drmp3_allocation_callbacks* pAllocationCallbacks) {
	return 0;
}

DRMP3_API drmp3_bool32
drmp3_init_memory(drmp3* pMP3, const void* pData, size_t dataSize, const drmp3_allocation_callbacks* pAllocationCallbacks) {
	return 0;
}

DRMP3_API drmp3_uint64
drmp3_get_pcm_frame_count(drmp3* pMP3) {
	return 0;
}

DRMP3_API drmp3_bool32
drmp3_seek_to_pcm_frame(drmp3* pMP3, drmp3_uint64 frameIndex) {
	return 0;
}

DRMP3_API drmp3_uint64
drmp3_read_pcm_frames_f32(drmp3* pMP3, drmp3_uint64 framesToRead, float* pBufferOut) {
	return 0;
}

DRFLAC_API void
drflac_close(drflac* pFlac) {
}

DRFLAC_API drflac*
drflac_open_memory(const void* pData, size_t dataSize, const drflac_allocation_callbacks* pAllocationCallbacks) {
	return NULL;
}

DRFLAC_API drflac*
drflac_open(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData, const drflac_allocation_callbacks* pAllocationCallbacks) {
	return NULL;
}

DRFLAC_API drflac_bool32
drflac_seek_to_pcm_frame(drflac* pFlac, drflac_uint64 pcmFrameIndex) {
	return 0;
}

DRFLAC_API drflac_uint64
drflac_read_pcm_frames_f32(drflac* pFlac, drflac_uint64 framesToRead, float* pBufferOut) {
	return 0;
}

#endif
