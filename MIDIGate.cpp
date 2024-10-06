// To build: CMD+P, type "task build_all"

// To burn onto Daisy (using vscode):
// 1. Connect micro-usb cable to Daisy (on the raised board, not the main board)
// 2. Put the Dasiy in firmware-flash mode:
//      a. Press and hold the BOOT button on the raised board
//      b. Press and release the RESET button on the raised board
//      c. You should see the flashing LED stop flashing
// 3. CMD+P, type "task build_and_program_dfu"

// To burn onto Daisy (using the web):
// 1. Connect micro-usb cable to Daisy (on the raised board, not the main board)
// 2. Put the Dasiy in firmware-flash mode:
//      a. Press and hold the BOOT button on the raised board
//      b. Press and release the RESET button on the raised board
//      c. You should see the flashing LED stop flashing
// 3. Go to https://electro-smith.github.io/Programmer/
// 4. Click "Connect"
// 5. Select "DFU in FS mode" from the options
// 6. Click "Choose File" and select the .bin file you want to burn
// 7. Once completed, press the RESET button on the raised board

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
Mode        mode          = BOOT;
bool        isBypassed    = false;
int         targetChannel = 1;
uint8_t     targetNote    = 60;
// static AdEnv env;

// uint32_t    hackyCounter = 0;


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
    // env.Trigger();
}

void SetMode(Mode m)
{
    mode = m;

    // switch(m)
    // {
    //     case GATE: CloseGate(); break;
    //     case MIDI_LEARN: break;
    //     case BOOT: break;
    // }
}

void CommitMIDINote(int channel, uint8_t note)
{
    targetChannel = channel;
    targetNote    = note;

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
    }
}
