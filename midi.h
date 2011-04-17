/*
 * Copyright (C) 2011  Gregor Richards
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef MIDI_H
#define MIDI_H

#define Pm_MessageType(msg) (Pm_MessageStatus(msg)>>4)
#define Pm_MessageChannel(msg) (Pm_MessageStatus(msg)&0xF)

/* event types */
#define MIDI_NOTE_OFF               0x8
#define MIDI_NOTE_ON                0x9
#define MIDI_NOTE_AFTERTOUCH        0xA
#define MIDI_CONTROLLER             0xB
#define MIDI_PROGRAM_CHANGE         0xC
#define MIDI_CHANNEL_AFTERTOUCH     0xD
#define MIDI_PITCH_BEND             0xE
#define MIDI_META                   0xF

/* status types (where applicable */
#define MIDI_STATUS_SYSEX           0xF0
#define MIDI_STATUS_SYSEX_CONT      0xF7
#define MIDI_STATUS_SYSEX_END       0xF7
#define MIDI_STATUS_META            0xFF

/* Meta (0xFF) events */
#define MIDI_M_SEQUENCE_NUMBER      0x00
#define MIDI_M_TEXT                 0x01
#define MIDI_M_COPYRIGHT            0x02
#define MIDI_M_NAME                 0x03
#define MIDI_M_INSTRUMENT           0x04
#define MIDI_M_LYRIC                0x05
#define MIDI_M_MARKER               0x06
#define MIDI_M_CUE                  0x07
#define MIDI_M_CHANNEL              0x20
#define MIDI_M_END                  0x2F
#define MIDI_M_TEMPO                0x51
#define MIDI_M_SMPTE_OFFSET         0x54
#define MIDI_M_TIME_SIGNATURE       0x58
#define MIDI_M_KEY_SIGNATURE        0x59
#define MIDI_M_SEQ_SPECIFIC         0x7F

/* data accumulators for meta events */
#define MIDI_M_SEQUENCE_NUMBER_LENGTH 2
#define MIDI_M_SEQUENCE_NUMBER_N(data) \
    ((data[0] << 8) + \
      data[1])

#define MIDI_M_CHANNEL_LENGTH 1
#define MIDI_M_CHANNEL_N(data) (data[0])

#define MIDI_M_TEMPO_LENGTH 3
#define MIDI_M_TEMPO_N(data) \
    ((data[0] << 16) + \
     (data[1] << 8) + \
      data[2]);

#define MIDI_M_TIME_SIGNATURE_LENGTH 4
#define MIDI_M_TIME_SIGNATURE_NUMERATOR(data) (data[0])
#define MIDI_M_TIME_SIGNATURE_DENOMINATOR(data) (data[1])
#define MIDI_M_TIME_SIGNATURE_METRONOME(data) (data[2])
#define MIDI_M_TIME_SIGNATURE_32NDS(data) (data[3])

#define MIDI_M_KEY_SIGNATURE_LENGTH 2
#define MIDI_M_KEY_SIGNATURE_KEY (data[0])
#define MIDI_M_KEY_SIGNATURE_MODE (data[1])
#define MIDI_M_KEY_SIGNATURE_MAJOR 0
#define MIDI_M_KEY_SIGNATURE_MINOR 1

#endif
