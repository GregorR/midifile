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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "midifstream.h"

#include "midifile.h"
#include "midifilealloc.h"

/* file-local miscellany */
static void Mf_FinalizeTrack(MfTrack *track);
static MfTrack *Mf_AssertTrack(MfFile *file, int track);

/* open a stream for a file */
MfStream *Mf_OpenStream(MfFile *of)
{
    MfStream *ret = Mf_New(MfStream);
    ret->file = of;
    return ret;
}

/* start a stream at this timestamp */
PmError Mf_StartStream(MfStream *stream, PtTimestamp timestamp)
{
    stream->tempoTs = timestamp;
    stream->tempoUs = 0;
    stream->tempoTick = 0;
    stream->tempo = 500000; /* 120BPM = 500K us/qn */
    return pmNoError;
}

/* close a stream, returning the now-complete file if you were writing (also
 * adds TrkEnd events and sets the format) */
MfFile *Mf_CloseStream(MfStream *stream)
{
    int i;
    MfFile *file = stream->file;
    AL.free(stream);

    /* finalize all the tracks */
    for (i = 0; i < file->trackCt; i++) {
        Mf_FinalizeTrack(file->tracks[i]);
    }

    /* and mark the format */
    file->format = (file->trackCt > 1) ? 1 : 0;

    return file;
}

static void Mf_FinalizeTrack(MfTrack *track)
{
    MfEvent *event;
    int mustFinalize = 0;

    if (track->tail) {
        event = track->tail;
        if (!(event->meta) || event->meta->type != 0x2F) { /* last isn't end-of-stream */
            mustFinalize = 1;
        }
    } else {
        mustFinalize = 1;
    }

    if (mustFinalize) {
        /* OK, we have to finalize */
        event = Mf_NewEvent();
        event->e.message = Pm_Message(0xFF, 0, 0);
        event->meta = Mf_NewMeta(0);
        event->meta->type = 0x2F;
        Mf_PushEvent(track, event);
    }
}

/* poll for events from the stream */
PmError Mf_StreamPoll(MfStream *stream)
{
    int i;
    MfFile *file;
    MfTrack *track;
    uint32_t curTick;

    /* calculate the current tick */
    curTick = Mf_StreamGetTick(stream, Pt_Time());

    file = stream->file;
    for (i = 0; i < file->trackCt; i++) {
        track = file->tracks[i];
        if (track->head && track->head->absoluteTm <= curTick) return TRUE;
    }

    return FALSE;
}

/* is the stream empty? */
PmError Mf_StreamEmpty(MfStream *stream)
{
    int i;
    MfFile *file;
    MfTrack *track;

    file = stream->file;
    for (i = 0; i < file->trackCt; i++) {
        track = file->tracks[i];
        if (track->head) return FALSE;
    }

    return TRUE;
}

/* read events from the stream (loses ownership of events) */
int Mf_StreamRead(MfStream *stream, MfEvent **into, int *ptrack, int32_t length)
{
    int rd = 0, i;
    MfFile *file;
    MfTrack *track;
    uint32_t curTick;

    /* calculate the current tick */
    curTick = Mf_StreamGetTick(stream, Pt_Time());

    file = stream->file;
    for (i = 0; i < file->trackCt; i++) {
        track = file->tracks[i];
        if (track->head && track->head->absoluteTm <= curTick) {
            /* read in this one */
            into[rd] = track->head;
            ptrack[rd] = i;
            track->head->e.timestamp = Mf_StreamGetTimestamp(stream, NULL, track->head->absoluteTm);
            track->head = track->head->next;
            if (!(track->head)) track->tail = NULL;
            rd++;

            /* stop if we're out of room */
            if (rd >= length) break;
        }
    }

    return rd;
}

int Mf_StreamReadNormal(MfStream *stream, MfEvent **into, int *ptrack, int32_t length)
{
    int32_t i;
    MfEvent *event;

    for (i = 0; i < length; i++) {
        if (Mf_StreamRead(stream, into + i, ptrack + i, 1) == 1) {
            event = into[i];

            /* check if it's a meta-event */
            if (event->meta) {
                /* don't send it to the user, just check it */
                i--;
                if (event->meta->type == 0x51 && event->meta->length == 3) { /* tempo change */
                    /* send the tempo change back */
                    PtTimestamp ts;
                    unsigned char *data = event->meta->data;
                    uint32_t tempo = (data[0] << 16) +
                        (data[1] << 8) +
                        data[2];
                    Mf_StreamSetTempoTick(stream, &ts, event->absoluteTm, tempo);
                }
                Mf_FreeEvent(event);
            }
        } else {
            break;
        }
    }

    return i;
}

/* write events into the stream (takes ownership of events) */
PmError Mf_StreamWrite(MfStream *stream, int track, MfEvent **events, int32_t length)
{
    int i;
    PmError perr;

    for (i = 0; i < length; i++) {
        if ((perr = Mf_StreamWriteOne(stream, track, events[i]))) return perr;
    }

    return pmNoError;
}

PmError Mf_StreamWriteOne(MfStream *stream, int trackno, MfEvent *event)
{
    MfTrack *track = Mf_AssertTrack(stream->file, trackno);

    /* first correct the event's delta time */
    if (event->deltaTm == 0) {
        if (event->absoluteTm == 0 && event->e.timestamp != 0) {
            /* OK, convert back the timestamp */
            event->absoluteTm = Mf_StreamGetTick(stream, event->e.timestamp);
        }

        if (event->absoluteTm != 0) {
            /* subtract away the delta */
            if (track->tail) {
                event->deltaTm = event->absoluteTm - track->tail->absoluteTm;
            } else {
                event->deltaTm = event->absoluteTm;
            }
        }
    }

    /* then add it */
    Mf_PushEvent(track, event);
    return pmNoError;
}

static MfTrack *Mf_AssertTrack(MfFile *file, int track)
{
    while (file->trackCt <= track) Mf_NewTrack(file);
    return file->tracks[track];
}

/* get the current tempo from this filestream */
uint32_t Mf_StreamGetTempo(MfStream *stream)
{
    return stream->tempo;
}

/* get a tick from this filestream */
uint32_t Mf_StreamGetTick(MfStream *stream, PtTimestamp timestamp)
{
    uint64_t tsus = 0;

    /* adjust for the current tick */
    timestamp -= stream->tempoTs;
    if (stream->tempoUs > 0) {
        timestamp--;
        tsus = 1000 - stream->tempoUs;
    }
    tsus += (uint64_t) timestamp * 1000;

    /* tempo is in microseconds per quarter note, ticks are timeDivision per
     * quarter note, so the algo is:
     * tsus / tempo * timeDivision */
    return stream->tempoTick + tsus * stream->file->timeDivision / stream->tempo;
}

/* get a timestamp from this filestream at a given tick */
PtTimestamp Mf_StreamGetTimestamp(MfStream *stream, int *us, uint32_t tick)
{
    uint64_t tickl = tick - stream->tempoTick;
    uint64_t tsbase = ((uint64_t) stream->tempoTs * 1000) + stream->tempoUs;

    tsbase = tsbase + tickl * stream->tempo / stream->file->timeDivision;

    if (us) *us = tsbase % 1000;
    tsbase /= 1000;

    return tsbase;
}

/* update the tempo for this filestream at a tick, writes the timestamp of the update into ts */
PmError Mf_StreamSetTempoTick(MfStream *stream, PtTimestamp *ts, uint32_t tick, uint32_t tempo)
{
    int us;
    *ts = Mf_StreamGetTimestamp(stream, &us, tick);
    stream->tempoTs = *ts;
    stream->tempoUs = us;
    stream->tempoTick = tick;
    stream->tempo = tempo;
    return pmNoError;
}

/* update the tempo for this filestream at a timestamp, writes the tick of the update into tick */
PmError Mf_StreamSetTempoTimestamp(MfStream *stream, uint32_t *tick, PtTimestamp ts, uint32_t tempo)
{
    *tick = Mf_StreamGetTick(stream, ts);
    stream->tempoTs = ts;
    stream->tempoUs = 0;
    stream->tempoTick = *tick;
    stream->tempo = tempo;
    return pmNoError;
}
