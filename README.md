# Daisy Pod MIDIGate

## Author

Zachary Berry

## Description

A MIDI-Controlled hard on/off audio gate

## Flashing to the Daisy Pod

> This assumes you are using VSCode

1. Follow the instructions in https://github.com/electro-smith/DaisyExamples to install the necessary libraries and tools
1. Clone this repository so that it lives in a directory inside the `DaisyExamples/pod/` folder
1. Connect the Daisy Pod to your computer via USB (Make sure you connect to the upper smaller board)
1. Hold down the `BOOT` button on the Daisy Pod and press the `RESET` button
1. In VSCode type CTRL/CMD+P and type `task build_and_program_dfu` and wait for the process to complete

## Controls

| Control        | Description            |
| -------------- | ---------------------- |
| Button 1       | Manually trigger gate  |
| Button 2       | Toggle MIDI Learn mode |
| Knob 1         | Gate open time         |
| Knob 2         | (Unused)               |
| Encoder Button | Toggle bypass          |
| Encoder        | (Unused)               |

## Operation

1. Connect a MIDI device to the Daisy Pod
1. Press Knob 2 on the Daisy Pod to enter **MIDI Learn mode**. You should see the two LEDs turn blue.
1. Press a note on your MIDI device to send a Note On event to the Daisy Pod. The blue LEDs should turn off if the note is received - the specific note number and channel will be saved - and the Daisy Pod will now be in **gate mode**. Alternately, you can press Button 2 again to exit MIDI Learn mode without saving a note.
1. In **gate mode**, the first LED will glow red (the second LED will remain dark). Sending the saved note (with the velocity set to anything greater than zero) the trigger the gate to open, causing the led to glow green, and the audio coming into the line input will be repeated to the line output. The gate will close after the gate open time has elapsed, at which point the output will be muted. At any time in **gate mode** you can press Button 1 to manually trigger the gate.
1. Turn Knob 1 to adjust the gate open time - turn the knob clockwise to increase the open item.
1. Press the Encoder down to enable bypass. When bypass is active the gate will always be open (or in other words, the incoming audio will be repeated on the output), and both LEDs will glow red. Press the Encoder again to disable bypass.

> Note: The gate is also left open whenever you are in MIDI Learn Mode

## Video

[YouTube Video](https://www.youtube.com/watch?v=v5GjT15CzVs)
