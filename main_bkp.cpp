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
void listDevices()
{
    void **hints;
    int err = snd_device_name_hint(-1, "pcm", &hints);
    if (err < 0)
    {
        std::cerr << "Error listing devices: " << snd_strerror(err) << "\n";
        return;
    }

    std::cout << "Available ALSA PCM devices:\n";
    for (void **n = hints; *n != nullptr; n++)
    {
        char *name = snd_device_name_get_hint(*n, "NAME");
        char *desc = snd_device_name_get_hint(*n, "DESC");

        if (name)
            std::cout << "  " << name << "\n";
        if (desc)
            std::cout << "    " << desc << "\n";

        if (name)
            free(name);
        if (desc)
            free(desc);
    }
    snd_device_name_free_hint(hints);
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
    if (argc < 2)
    {
        std::cout << "Usage:\n"
                  << "  cpp_audio list\n"
                  << "  cpp_audio play <device> [freq=440] [seconds=3]\n"
                  << "  cpp_audio record <device> <seconds> <outfile.wav>\n";
        return 0;
    }

    std::string cmd = argv[1];
    if (cmd == "list")
    {
        listDevices();
    }
    else if (cmd == "play" && argc >= 3)
    {
        std::string dev = argv[2];
        double freq = argc > 3 ? atof(argv[3]) : 440.0;
        int secs = argc > 4 ? atoi(argv[4]) : 3;
        playTone(dev, 44100, freq, secs);
    }
    else if (cmd == "record" && argc >= 5)
    {
        std::string dev = argv[2];
        int secs = atoi(argv[3]);
        std::string outfile = argv[4];
        recordAudio(dev, 44100, secs, outfile);
    }
    else
    {
        std::cerr << "Invalid arguments.\n";
    }

    return 0;
}
