# 


## Channel selection:


### Output (Playback) — USB Audio Device

sysdefault:CARD=Device
    USB Audio Device, USB Audio
Default Audio Device



🔹 front:CARD=Device,DEV=0 → the stereo front-out of your USB audio.
🔹 sysdefault:CARD=Device → the “system default” for that card (safe choice).


### Input (Capture) — Baseus USB Audio


sysdefault:CARD=Audio
    Baseus USB Audio, USB Audio
Default Audio Device




```bash
# Debian/Raspbian
sudo apt update
sudo apt install build-essential libasound2-dev
```
```bash
g++ -std=c++20 -g main.cpp -o cpp_audio -lasound -lpthread -ldl -lm
```


## Command considerations  

When using:
```bash 
hw:CARD=Audio,DEV=0
```  

you’re talking directly to the hardware, without any automatic resampling or format conversion.
If the device doesn’t support 44100 Hz / 16-bit exactly as you requested, ALSA won’t fix it — it’ll just record at whatever format the hardware runs at (e.g., 48000 Hz), but your code still interprets it as 44100 Hz, causing the playback to sound slower and deeper (lower pitch).

Why ```sysdefault:``` Works
```sysdefault:``` and ```default:``` go through ALSA’s “plug” plugin, which automatically converts sample rate, format, and channels for you.

So, when your hardware only supports 48000 Hz, but you request 44100 Hz, ALSA resamples in software to match your request — and it sounds correct.

Use plughw: instead of hw: — it gives you direct device access, with conversion:

in general:
```sysdefault:``` Matches system
```plughw:``` Direct device, but flexible
```hw:``` Must match exact device format



### Play  

```bash
./main play plughw:CARD=Device,DEV=0 440 3

# less reliable
./main play hw:CARD=Device,DEV=0 440 3 
./main play front:CARD=Device,DEV=0 440 3
```
### Record   

```shell
./main record plughw:CARD=Audio,DEV=0 5 test.wav


# less reliable
./main record hw:CARD=Audio,DEV=0 5 test.wav
./main record front:CARD=Audio,DEV=0 5 test.wav

./main record sysdefault:CARD=Audio 5 test.wav

```

### Playrecord

```bash
./main playrecord plughw:0,0 plughw:3,0 5 ./sweep_record2.wav


# Less reliable
./main playrecord plughw:CARD=Audio,DEV=0 plughw:CARD=Device,DEV=0 5 ./sweep_record2.wav

./main playrecord hw:CARD=Audio,DEV=0 hw:CARD=Device,DEV=0 5 ./sweep_record2.wav
./main playrecord sysdefault:CARD=Device sysdefault:CARD=Audio 5 ./sweep_record.wav
```


### Passthrough

✅ sysdefault:CARD=Audio
✅ front:CARD=Audio,DEV=0
✅ hw:CARD=Audio,DEV=0

```bash
./main passthrough plughw:CARD=Audio,DEV=0 plughw:CARD=Device,DEV=0 10


# Less reliable
./main passthrough plughw:0,0 plughw:3,0 10

./main passthrough hw:CARD=Audio,DEV=0 hw:CARD=Device,DEV=0 10
```



## Troubleshooting:

```shell
❯ cat /proc/asound/cards

 0 [Device         ]: USB-Audio - USB Audio Device
                      GeneralPlus USB Audio Device at usb-0000:00:1a.0-1.1.4, full speed
 1 [PCH            ]: HDA-Intel - HDA Intel PCH
                      HDA Intel PCH at 0xc3400000 irq 32
 2 [NVidia         ]: HDA-Intel - HDA NVidia
                      HDA NVidia at 0xc3000000 irq 17
 3 [Audio          ]: USB-Audio - Baseus USB Audio
                      Generic Baseus USB Audio at usb-0000:00:1a.0-1.2, full speed
```

using pipewire-jack to manage the clock




