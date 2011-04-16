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

#ifndef MIDIFSTREAM_H
#define MIDIFSTREAM_H

#include "midifile.h"
#include "porttime.h"

/* types */
typedef struct __MfStream MfStream;

/* an active filestream */
struct __MfStream {
    MfFile *file;

    /* info for calculating tempo: timestamp when tempo was last changed, tick
     * when tempo was last changed, and new tempo */
    PtTimestamp tempoTs;
    int tempoUs; /* microseconds */
    uint32_t tempoTick, tempo;
};

/* open a stream for a file */
MfStream *Mf_OpenStream(MfFile *of);

/* start a stream at this timestamp */
PmError Mf_StartStream(MfStream *stream, PtTimestamp timestamp);

/* close a stream, returning the now-complete file if you were writing (also
 * adds TrkEnd events and sets the format) */
MfFile *Mf_CloseStream(MfStream *stream);

/* poll for events from the stream */
PmError Mf_StreamPoll(MfStream *stream);

/* what's the tick of the next event on the stream? */
uint32_t Mf_StreamNext(MfStream *stream);

/* is the stream empty? */
PmError Mf_StreamEmpty(MfStream *stream);

/* read events from the stream (loses ownership of events) */
int Mf_StreamReadUntil(MfStream *stream, MfEvent **into, int *track, int32_t length, uint32_t maxTm);
int Mf_StreamRead(MfStream *stream, MfEvent **into, int *track, int32_t length);
int Mf_StreamReadNormal(MfStream *stream, MfEvent **into, int *track, int32_t length);

/* write events into the stream (takes ownership of events) */
PmError Mf_StreamWrite(MfStream *stream, int track, MfEvent **events, int32_t length);
PmError Mf_StreamWriteOne(MfStream *stream, int track, MfEvent *event);

/* get the current tempo from this filestream */
uint32_t Mf_StreamGetTempo(MfStream *stream);

/* get a tick from this filestream at a given timestamp */
uint32_t Mf_StreamGetTick(MfStream *stream, PtTimestamp timestamp);

/* get a timestamp from this filestream at a given tick */
PtTimestamp Mf_StreamGetTimestamp(MfStream *stream, int *us, uint32_t tick);

/* update all tempo info for this filestream */
PmError Mf_StreamSetTempo(MfStream *stream, PtTimestamp ts, int us, uint32_t tick, uint32_t tempo);

/* update the tempo for this filestream at a tick, writes the timestamp of the update into ts */
PmError Mf_StreamSetTempoTick(MfStream *stream, PtTimestamp *ts, uint32_t tick, uint32_t tempo);

/* update the tempo for this filestream at a timestamp, writes the tick of the update into tick */
PmError Mf_StreamSetTempoTimestamp(MfStream *stream, uint32_t *tick, PtTimestamp ts, uint32_t tempo);

#endif
