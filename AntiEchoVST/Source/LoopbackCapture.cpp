#define INITGUID
#include "LoopbackCapture.h"
#include <cstring>

#if JUCE_WINDOWS
 #include <functiondiscoverykeys_devpkey.h>
 #include <propvarutil.h>
#endif

LoopbackCapture::LoopbackCapture()
{
    for (auto& ch : ring)
        ch.assign (ringCapacity, 0.0f);
}

LoopbackCapture::~LoopbackCapture()
{
    stop();
}

float LoopbackCapture::readSample (int channel, int64_t globalPos) const noexcept
{
    const int64_t wp = writePos.load (std::memory_order_acquire);

    if (globalPos < 0 || globalPos >= wp)
        return 0.0f;
    if (wp - globalPos > (int64_t) ringCapacity)
        return 0.0f;

    size_t idx = (size_t) (((globalPos % (int64_t) ringCapacity) + (int64_t) ringCapacity) % (int64_t) ringCapacity);
    return ring[(size_t) channel % numChannels][idx];
}

#if JUCE_WINDOWS

std::vector<RenderDeviceInfo> LoopbackCapture::getAvailableDevices()
{
    std::vector<RenderDeviceInfo> result;

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumr;
    HRESULT hr = CoCreateInstance (__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                   __uuidof(IMMDeviceEnumerator), (void**) enumr.GetAddressOf());
    if (FAILED (hr)) return result;

    Microsoft::WRL::ComPtr<IMMDeviceCollection> collection;
    hr = enumr->EnumAudioEndpoints (eRender, DEVICE_STATE_ACTIVE, collection.GetAddressOf());
    if (FAILED (hr)) return result;

    UINT count = 0;
    collection->GetCount (&count);

    for (UINT i = 0; i < count; ++i)
    {
        Microsoft::WRL::ComPtr<IMMDevice> dev;
        if (FAILED (collection->Item (i, dev.GetAddressOf())))
            continue;

        LPWSTR idStr = nullptr;
        if (FAILED (dev->GetId (&idStr)))
            continue;

        juce::String idJuce (idStr);
        CoTaskMemFree (idStr);

        juce::String nameJuce = idJuce; // フォールバック

        Microsoft::WRL::ComPtr<IPropertyStore> props;
        if (SUCCEEDED (dev->OpenPropertyStore (STGM_READ, props.GetAddressOf())))
        {
            PROPVARIANT varName;
            PropVariantInit (&varName);
            if (SUCCEEDED (props->GetValue (PKEY_Device_FriendlyName, &varName)) && varName.pwszVal != nullptr)
                nameJuce = juce::String (varName.pwszVal);
            PropVariantClear (&varName);
        }

        result.push_back ({ idJuce, nameJuce });
    }

    return result;
}

bool LoopbackCapture::start (const juce::String& deviceId, double& sampleRateOut)
{
    if (running.load())
        return true;

    HRESULT hr = CoCreateInstance (__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                    __uuidof(IMMDeviceEnumerator), (void**) enumerator.GetAddressOf());
    if (FAILED (hr)) return false;

    if (deviceId.isNotEmpty())
    {
        hr = enumerator->GetDevice (deviceId.toWideCharPointer(), device.GetAddressOf());
        if (FAILED (hr))
            hr = enumerator->GetDefaultAudioEndpoint (eRender, eConsole, device.GetAddressOf());
    }
    else
    {
        hr = enumerator->GetDefaultAudioEndpoint (eRender, eConsole, device.GetAddressOf());
    }
    if (FAILED (hr)) return false;

    currentDeviceName = "(unknown)";
    Microsoft::WRL::ComPtr<IPropertyStore> props;
    if (SUCCEEDED (device->OpenPropertyStore (STGM_READ, props.GetAddressOf())))
    {
        PROPVARIANT varName;
        PropVariantInit (&varName);
        if (SUCCEEDED (props->GetValue (PKEY_Device_FriendlyName, &varName)) && varName.pwszVal != nullptr)
            currentDeviceName = juce::String (varName.pwszVal);
        PropVariantClear (&varName);
    }

    hr = device->Activate (__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**) audioClient.GetAddressOf());
    if (FAILED (hr)) return false;

    hr = audioClient->GetMixFormat (&mixFormat);
    if (FAILED (hr)) return false;

    deviceSampleRate = (double) mixFormat->nSamplesPerSec;
    sampleRateOut = deviceSampleRate;

    REFERENCE_TIME bufferDuration = 200000; // 20ms相当(100ns単位)
    hr = audioClient->Initialize (AUDCLNT_SHAREMODE_SHARED,
                                  AUDCLNT_STREAMFLAGS_LOOPBACK,
                                  bufferDuration, 0, mixFormat, nullptr);
    if (FAILED (hr)) return false;

    hr = audioClient->GetService (__uuidof(IAudioCaptureClient), (void**) captureClient.GetAddressOf());
    if (FAILED (hr)) return false;

    hr = audioClient->Start();
    if (FAILED (hr)) return false;

    running.store (true);
    thread_ = std::thread (&LoopbackCapture::captureThreadFunc, this);
    return true;
}

void LoopbackCapture::stop()
{
    if (! running.load())
        return;

    running.store (false);
    if (thread_.joinable())
        thread_.join();

    if (audioClient)
        audioClient->Stop();

    captureClient.Reset();
    audioClient.Reset();
    device.Reset();
    enumerator.Reset();

    if (mixFormat != nullptr)
    {
        CoTaskMemFree (mixFormat);
        mixFormat = nullptr;
    }
}

void LoopbackCapture::captureThreadFunc()
{
    CoInitializeEx (nullptr, COINIT_MULTITHREADED);

    const bool isFloat = (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
        (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
         reinterpret_cast<WAVEFORMATEXTENSIBLE*> (mixFormat)->SubFormat.Data1 == 3);
    const int srcChannels = mixFormat->nChannels;
    const int bytesPerSample = mixFormat->wBitsPerSample / 8;

    while (running.load())
    {
        UINT32 packetLength = 0;
        HRESULT hr = captureClient->GetNextPacketSize (&packetLength);
        if (FAILED (hr)) break;

        while (packetLength != 0)
        {
            BYTE* data = nullptr;
            UINT32 numFrames = 0;
            DWORD flags = 0;

            hr = captureClient->GetBuffer (&data, &numFrames, &flags, nullptr, nullptr);
            if (FAILED (hr)) break;

            const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;

            for (UINT32 i = 0; i < numFrames; ++i)
            {
                float left = 0.0f, right = 0.0f;

                if (! silent && data != nullptr)
                {
                    if (isFloat)
                    {
                        const float* fdata = reinterpret_cast<const float*> (data + (size_t) i * srcChannels * bytesPerSample);
                        left  = fdata[0];
                        right = (srcChannels > 1) ? fdata[1] : fdata[0];
                    }
                    else
                    {
                        const int16_t* sdata = reinterpret_cast<const int16_t*> (data + (size_t) i * srcChannels * bytesPerSample);
                        left  = sdata[0] / 32768.0f;
                        right = (srcChannels > 1) ? sdata[1] / 32768.0f : left;
                    }
                }

                int64_t pos = writePos.load (std::memory_order_relaxed);
                size_t idx = (size_t) (pos % (int64_t) ringCapacity);
                ring[0][idx] = left;
                ring[1][idx] = right;
                writePos.store (pos + 1, std::memory_order_release);
            }

            hr = captureClient->ReleaseBuffer (numFrames);
            if (FAILED (hr)) break;

            hr = captureClient->GetNextPacketSize (&packetLength);
            if (FAILED (hr)) break;
        }

        std::this_thread::sleep_for (std::chrono::milliseconds (5));
    }

    CoUninitialize();
}

#else // 非Windows環境(ビルドチェック用ダミー実装)

std::vector<RenderDeviceInfo> LoopbackCapture::getAvailableDevices() { return {}; }
bool LoopbackCapture::start (const juce::String&, double& sampleRateOut)
{
    sampleRateOut = 48000.0;
    running.store (false);
    return false;
}
void LoopbackCapture::stop() {}
void LoopbackCapture::captureThreadFunc() {}

#endif
