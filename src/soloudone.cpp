#include "core/soloud.cpp"
#include "core/soloud_audiosource.cpp"
#include "core/soloud_bus.cpp"
#include "core/soloud_core_3d.cpp"
#include "core/soloud_core_basicops.cpp"
#include "core/soloud_core_faderops.cpp"
#include "core/soloud_core_filterops.cpp"
#include "core/soloud_core_getters.cpp"
#include "core/soloud_core_setters.cpp"
#include "core/soloud_core_voicegroup.cpp"
#include "core/soloud_core_voiceops.cpp"
#include "core/soloud_fader.cpp"
#include "core/soloud_fft.cpp"
#include "core/soloud_file.cpp"
#include "core/soloud_misc.cpp"
#include "core/soloud_queue.cpp"
#include "core/soloud_thread.cpp"

#ifdef SOLOUD_FILTER

#include "core/soloud_filter.cpp"

#include "filter/soloud_bassboostfilter.cpp"
#include "filter/soloud_biquadresonantfilter.cpp"
#include "filter/soloud_dcremovalfilter.cpp"
#include "filter/soloud_duckfilter.cpp"
#include "filter/soloud_echofilter.cpp"
#define catmullrom catmullrom_
#include "filter/soloud_eqfilter.cpp"
#undef catmullrom_
#include "filter/soloud_fftfilter.cpp"
#include "filter/soloud_flangerfilter.cpp"
#include "filter/soloud_freeverbfilter.cpp"
#include "filter/soloud_lofifilter.cpp"
#include "filter/soloud_robotizefilter.cpp"
#include "filter/soloud_waveshaperfilter.cpp"

#endif

#include "backend/winmm/soloud_winmm.cpp"

extern "C" {
#include "audiosource/wav/stb_vorbis.c"
}

#ifdef SOLOUD_MP3_FLAC
#include "audiosource/wav/dr_impl.cpp"
#else
#include "soloudwavonly.h"
#endif

#include "audiosource/wav/soloud_wav.cpp"
#include "audiosource/wav/soloud_wavstream.cpp"

#ifdef SOLOUD_SPEECH
#include "audiosource/speech/darray.cpp"
#include "audiosource/speech/klatt.cpp"
#include "audiosource/speech/resonator.cpp"
#include "audiosource/speech/soloud_speech.cpp"
#include "audiosource/speech/tts.cpp"
#endif

#include "soloudcapi.h"