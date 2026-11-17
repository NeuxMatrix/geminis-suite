#include "SystemVolume.h"

#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <mmdeviceapi.h>
 #include <endpointvolume.h>
 #include <cmath>
 #if defined (_MSC_VER)
  #pragma comment (lib, "ole32.lib")
 #endif
#endif

namespace geminis::app
{
SystemVolume readSystemVolume()
{
    SystemVolume v;
   #if defined (_WIN32)
    // COM en el hilo que llama (el de la interfaz ya lo suele tener; si es otro modo, sigue valiendo)
    thread_local const bool comReady = []
    {
        const HRESULT hr = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);
        return SUCCEEDED (hr) || hr == RPC_E_CHANGED_MODE;
    }();
    if (! comReady) return v;

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioEndpointVolume* volume = nullptr;

    if (SUCCEEDED (CoCreateInstance (__uuidof (MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                     __uuidof (IMMDeviceEnumerator), reinterpret_cast<void**> (&enumerator)))
        && SUCCEEDED (enumerator->GetDefaultAudioEndpoint (eRender, eConsole, &device))
        && SUCCEEDED (device->Activate (__uuidof (IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                                        reinterpret_cast<void**> (&volume))))
    {
        float scalar = 1.0f, db = 0.0f;
        BOOL mute = FALSE;
        if (SUCCEEDED (volume->GetMasterVolumeLevelScalar (&scalar))
            && SUCCEEDED (volume->GetMasterVolumeLevel (&db))
            && SUCCEEDED (volume->GetMute (&mute)))
        {
            // Se usa el nivel en dB, que es lo que Windows aplica al resto de programas
            v.available = true;
            v.muted = mute != FALSE;
            v.gain = scalar <= 0.001f ? 0.0f : std::pow (10.0f, std::fmin (db, 0.0f) / 20.0f);
        }
    }

    if (volume != nullptr)     volume->Release();
    if (device != nullptr)     device->Release();
    if (enumerator != nullptr) enumerator->Release();
   #endif
    return v;
}
} // namespace geminis::app
