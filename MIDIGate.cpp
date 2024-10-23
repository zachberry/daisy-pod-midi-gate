#include "daisy_pod.h"
#include "daisysp.h"
#include <stdio.h>
#include <string.h>

#define MAX_GATE_OPEN_MS 1000.0f;

using namespace daisy;
using namespace daisysp;

enum Mode
{
    BOOT,
    GATE,
    MIDI_LEARN
};

DaisyPod    hw;
Parameter   p_knob1, p_knob2;
TimerHandle timer;
bool        isGateOpen = true;
float       gateOpenMs = 100.0f;
uint32_t    freq;
uint32_t    lastTime;
Mode        mode               = BOOT;
bool        isBypassed         = false;
int         targetChannel      = 1;
uint8_t     targetNote         = 60;
bool        shouldSaveSettings = false;

struct Settings
{
    int     targetChannel;
    uint8_t targetNote;

    //Overloading the != operator
    //This is necessary as this operator is used in the PersistentStorage source code
    bool operator!=(const Settings &a) const
    {
        return a.targetChannel != targetChannel || a.targetNote != targetNote;
    }
};

//https://forum.electro-smith.com/t/saving-values-to-flash-memory-using-persistentstorage-class-on-daisy-pod/4306
//Persistent Storage Declaration. Using type Settings and passed the devices qspi handle
PersistentStorage<Settings> SavedSettings(hw.seed.qspi);

void Save()
{
    //Reference to local copy of settings stored in flash
    Settings &LocalSettings = SavedSettings.GetSettings();

    LocalSettings.targetChannel = targetChannel;
    LocalSettings.targetNote    = targetNote;

    shouldSaveSettings = true;
}

void Load()
{
    //Reference to local copy of settings stored in flash
    Settings &LocalSettings = SavedSettings.GetSettings();

    targetChannel = LocalSettings.targetChannel;
    targetNote    = LocalSettings.targetNote;
}

void OpenGate()
{
    timer.Stop();

    if(mode != GATE)
    {
        return;
    }

    isGateOpen = true;

    timer.Start();
    freq     = timer.GetFreq();
    lastTime = timer.GetTick();
}

void CloseGate()
{
    timer.Stop();

    if(mode != GATE)
    {
        return;
    }

    isGateOpen = false;
}

void SetMode(Mode m)
{
    mode = m;
}

void CommitMIDINote(int channel, uint8_t note)
{
    targetChannel = channel;
    targetNote    = note;

    Save();
    SetMode(GATE);
}

void ReadControls()
{
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();

    // Read the first knob (how long to keep the gate open for when triggered)
    float k1   = hw.knob1.Process();
    gateOpenMs = k1 * MAX_GATE_OPEN_MS;

    // Read the button (to test-trigger the gate open)
    if(hw.button1.RisingEdge())
    {
        OpenGate();
    }

    // Read the second knob (to toggle modes)
    if(hw.button2.RisingEdge())
    {
        if(mode == MIDI_LEARN)
        {
            SetMode(GATE);
        }
        else
        {
            SetMode(MIDI_LEARN);
        }
    }

    // Read the encoder button (to toggle bypass)
    if(hw.encoder.RisingEdge())
    {
        isBypassed = !isBypassed;
    }
}

void UpdateGate()
{
    if(!isGateOpen || mode != GATE)
    {
        return;
    }

    // https://forum.electro-smith.com/t/solved-how-to-do-mcu-utilization-measurements/1236/29?page=2
    uint32_t newTick      = timer.GetTick();
    float    intervalMsec = 1000. * ((float)(newTick - lastTime) / (float)freq);

    if(intervalMsec >= gateOpenMs)
    {
        CloseGate();
    }
}

void HandleMidiMessage(MidiEvent m)
{
    switch(m.type)
    {
        case NoteOn:
        {
            NoteOnEvent p = m.AsNoteOn();

            if(p.velocity > 0)
            {
                switch(mode)
                {
                    case GATE:
                    {
                        if(p.channel == targetChannel && p.note == targetNote)
                        {
                            OpenGate();
                        }
                    }
                    break;

                    case MIDI_LEARN:
                    {
                        CommitMIDINote(p.channel, p.note);
                    }
                    break;

                    default: break;
                }
            }
        }
        break;
        default: break;
    }
}

void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    for(size_t i = 0; i < size; i += 2)
    {
        if(isGateOpen || mode == MIDI_LEARN || isBypassed)
        {
            out[i]     = in[i];
            out[i + 1] = in[i + 1];
        }
        else
        {
            out[i]     = 0;
            out[i + 1] = 0;
        }
    }
}

void UpdateLEDs()
{
    switch(mode)
    {
        case GATE:
        {
            if(isBypassed)
            {
                hw.led1.Set(1.0f, 0, 0);
                hw.led2.Set(1.0f, 0, 0);
            }
            else if(isGateOpen)
            {
                hw.led1.Set(0, 1.0f, 0);
                hw.led2.Set(0, 0, 0);
            }
            else
            {
                hw.led1.Set(1.0f, 0, 0);
                hw.led2.Set(0, 0, 0);
            }
        }
        break;

        case MIDI_LEARN:
        {
            hw.led1.Set(0, 0, 1.0f);
            hw.led2.Set(0, 0, 1.0f);
        }
        break;

        case BOOT: break;
    }

    hw.UpdateLeds();
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4);

    hw.seed.usb_handle.Init(UsbHandle::FS_INTERNAL);
    System::Delay(250);

    p_knob1.Init(hw.knob1, 0, 1, Parameter::LINEAR);
    p_knob2.Init(hw.knob2, 0, 1, Parameter::LINEAR);

    TimerHandle::Config timerConfig;
    timerConfig.periph = TimerHandle::Config::Peripheral::TIM_2;
    timerConfig.dir    = TimerHandle::Config::CounterDir::UP;
    timer.Init(timerConfig);

    //Initilize the PersistentStorage Object with default values.
    //Defaults will be the first values stored in flash when the device is first turned on.
    //They can also be restored at a later date using the RestoreDefaults method
    Settings DefaultSettings = {1, 60};
    SavedSettings.Init(DefaultSettings);

    Load();

    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    hw.midi.StartReceive();

    SetMode(GATE);
    CloseGate();

    for(;;)
    {
        hw.midi.Listen();

        while(hw.midi.HasEvents())
        {
            HandleMidiMessage(hw.midi.PopEvent());
        }

        ReadControls();
        UpdateGate();

        UpdateLEDs();

        if(shouldSaveSettings)
        {
            SavedSettings.Save();
            shouldSaveSettings = false;

            System::Delay(100);
        }
    }
}
