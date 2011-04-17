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

#ifndef MIDIFILE_H
#define MIDIFILE_H

#include <stdio.h>
#include <stdlib.h>

#include "portmidi.h"

/* This is a MIDI file type handler designed to work with PortMidi. */

/* basic types */
typedef struct __MfFile MfFile;
typedef struct __MfTrack MfTrack;
typedef struct __MfEvent MfEvent;
typedef struct __MfMeta MfMeta;

/* initialization */
PmError Mf_Initialize(void);

/* MIDI file */
struct __MfFile {
    uint16_t format, timeDivision;
    uint16_t trackCt;
    MfTrack **tracks;
};
void Mf_FreeFile(MfFile *file);
MfFile *Mf_NewFile(uint16_t timeDivision);

/* track */
struct __MfTrack {
    MfEvent *head, *tail;
};
void Mf_FreeTrack(MfTrack *track);
MfTrack *Mf_NewTrack(MfFile *file);
void Mf_PushTrack(MfFile *file, MfTrack *track);

/* and event */
struct __MfEvent {
    MfEvent *next;
    uint32_t deltaTm, absoluteTm;
    PmEvent e;
    MfMeta *meta;
};
void Mf_FreeEvent(MfEvent *event);
MfEvent *Mf_NewEvent(void);
void Mf_PushEvent(MfTrack *track, MfEvent *event);
void Mf_PushEventHead(MfTrack *track, MfEvent *event);

/* meta-events have extra fields */
struct __MfMeta {
    uint8_t type;
    uint32_t length;
    unsigned char data[1];
};
void Mf_FreeMeta(MfMeta *meta);
MfMeta *Mf_NewMeta(uint32_t length);

/* read in a MIDI file */
PmError Mf_ReadMidiFile(MfFile **into, FILE *from);

/* write out a MIDI file */
PmError Mf_WriteMidiFile(FILE *into, MfFile *from);

#endif
