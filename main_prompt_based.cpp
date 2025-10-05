#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <alsa/asoundlib.h>
#include <fstream>



struct DeviceInfo {
    std::string name;
    std::string desc;
};



// Write raw PCM data to simple WAV file
void writeWav(const std::string &filename, const std::vector<short> &samples, int sampleRate, int channels)
{
    std::ofstream out(filename, std::ios::binary);

    int byteRate = sampleRate * channels * 2;
    int dataSize = samples.size() * 2;

    // RIFF header
    out.write("RIFF", 4);
    int chunkSize = 36 + dataSize;
    out.write(reinterpret_cast<const char *>(&chunkSize), 4);
    out.write("WAVE", 4);

    // fmt subchunk
    out.write("fmt ", 4);
    int subchunk1Size = 16;
    short audioFormat = 1; // PCM
    short numChannels = channels;
    int sampleRate_ = sampleRate;
    short bitsPerSample = 16;
    short blockAlign = numChannels * bitsPerSample / 8;

    out.write(reinterpret_cast<const char *>(&subchunk1Size), 4);
    out.write(reinterpret_cast<const char *>(&audioFormat), 2);
    out.write(reinterpret_cast<const char *>(&numChannels), 2);
    out.write(reinterpret_cast<const char *>(&sampleRate_), 4);
    out.write(reinterpret_cast<const char *>(&byteRate), 4);
    out.write(reinterpret_cast<const char *>(&blockAlign), 2);
    out.write(reinterpret_cast<const char *>(&bitsPerSample), 2);

    // data subchunk
    out.write("data", 4);
    out.write(reinterpret_cast<const char *>(&dataSize), 4);
    out.write(reinterpret_cast<const char *>(samples.data()), dataSize);
    out.close();
}

// List available devices
std::vector<DeviceInfo> listDevices(bool capture)
{
    std::vector<DeviceInfo> devices;
    void **hints;
    int err = snd_device_name_hint(-1, "pcm", &hints);
    if (err < 0)
    {
        std::cerr << "Error listing devices: " << snd_strerror(err) << "\n";
        return devices;
    }

    std::cout << (capture ? "\n🎤 Available Capture Devices:\n" : "\n🎧 Available Playback Devices:\n");

    int index = 0;
    for (void **n = hints; *n != nullptr; n++)
    {
        char *name = snd_device_name_get_hint(*n, "NAME");
        char *ioid = snd_device_name_get_hint(*n, "IOID");

        // Filter by playback/capture
        if (ioid)
        {
            if (capture && std::string(ioid) != "Input") { free(name); free(ioid); continue; }
            if (!capture && std::string(ioid) != "Output") { free(name); free(ioid); continue; }
        }

        if (name)
        {
            std::cout << "  [" << index << "] " << name << "\n";
            devices.push_back({name, ""});
            index++;
            free(name);
        }
        if (ioid) free(ioid);
    }
    snd_device_name_free_hint(hints);

    if (devices.empty())
        std::cout << "  (no matching devices found)\n";

    return devices;
}


// Play sine tone on device
void playTone(const std::string &device, int sampleRate, double frequency, int seconds)
{
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    int rc;

    rc = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0)
    {
        std::cerr << "Unable to open playback device: " << snd_strerror(rc) << "\n";
        return;
    }

    snd_pcm_hw_params_malloc(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    unsigned int rate = sampleRate;
    snd_pcm_hw_params_set_rate_near(handle, params, &rate, nullptr);
    snd_pcm_hw_params_set_channels(handle, params, 2);
    snd_pcm_hw_params(handle, params);
    snd_pcm_hw_params_free(params);
    snd_pcm_prepare(handle);

    int framesPerBuffer = 512;
    std::vector<short> buffer(framesPerBuffer * 2);

    double phase = 0.0;
    double step = 2 * M_PI * frequency / sampleRate;

    int totalFrames = sampleRate * seconds;
    for (int i = 0; i < totalFrames; i += framesPerBuffer)
    {
        for (int j = 0; j < framesPerBuffer; j++)
        {
            short value = static_cast<short>(std::sin(phase) * 32767);
            buffer[j * 2] = value;     // Left
            buffer[j * 2 + 1] = value; // Right
            phase += step;
            if (phase > 2 * M_PI)
                phase -= 2 * M_PI;
        }

        snd_pcm_writei(handle, buffer.data(), framesPerBuffer);
    }

    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    std::cout << "Tone finished.\n";
}

// Record from device
void recordAudio(const std::string &device, int sampleRate, int seconds, const std::string &outfile)
{
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    int rc = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0)
    {
        std::cerr << "Unable to open capture device: " << snd_strerror(rc) << "\n";
        return;
    }

    snd_pcm_hw_params_malloc(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    unsigned int rate = sampleRate;
    snd_pcm_hw_params_set_rate_near(handle, params, &rate, nullptr);
    snd_pcm_hw_params_set_channels(handle, params, 1);
    snd_pcm_hw_params(handle, params);
    snd_pcm_hw_params_free(params);
    snd_pcm_prepare(handle);

    int framesPerBuffer = 512;
    std::vector<short> buffer(framesPerBuffer);
    std::vector<short> recorded;
    recorded.reserve(sampleRate * seconds);

    int totalFrames = sampleRate * seconds;
    for (int i = 0; i < totalFrames; i += framesPerBuffer)
    {
        rc = snd_pcm_readi(handle, buffer.data(), framesPerBuffer);
        if (rc < 0)
        {
            snd_pcm_recover(handle, rc, 0);
        }
        else
        {
            recorded.insert(recorded.end(), buffer.begin(), buffer.begin() + rc);
        }
    }

    snd_pcm_drain(handle);
    snd_pcm_close(handle);

    writeWav(outfile, recorded, sampleRate, 1);
    std::cout << "Saved recording to " << outfile << "\n";
}

int main(int argc, char *argv[])
{
    std::cout << "Audio Test Tool 🎧\n";
    std::cout << "1) List Devices\n";
    std::cout << "2) Play Tone\n";
    std::cout << "3) Record Audio\n";
    std::cout << "Choose option: ";
    int choice;
    std::cin >> choice;

    if (choice == 1)
    {
        listDevices(false);
        listDevices(true);
        return 0;
    }
    else if (choice == 2)
    {
        auto devices = listDevices(false);
        if (devices.empty()) return 0;

        std::cout << "\nSelect playback device index: ";
        int idx;
        std::cin >> idx;
        if (idx < 0 || idx >= (int)devices.size())
        {
            std::cerr << "Invalid index.\n";
            return 0;
        }

        double freq = 440.0;
        int secs = 3;
        std::cout << "Enter frequency (Hz, default 440): ";
        std::cin >> freq;
        std::cout << "Enter duration (seconds, default 3): ";
        std::cin >> secs;

        playTone(devices[idx].name, 44100, freq, secs);
    }
    else if (choice == 3)
    {
        auto devices = listDevices(true);
        if (devices.empty()) return 0;

        std::cout << "\nSelect capture device index: ";
        int idx;
        std::cin >> idx;
        if (idx < 0 || idx >= (int)devices.size())
        {
            std::cerr << "Invalid index.\n";
            return 0;
        }

        int secs;
        std::cout << "Enter record duration (seconds): ";
        std::cin >> secs;

        std::string outfile;
        std::cout << "Enter output .wav file path: ";
        std::cin >> outfile;

        recordAudio(devices[idx].name, 44100, secs, outfile);
    }
    else
    {
        std::cout << "Invalid choice.\n";
    }

    return 0;
}

