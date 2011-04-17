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

#include <errno.h>
#include <string.h>

#include "midifile.h"
#include "midifilealloc.h"

/* MISCELLANY HERE */

/* default strerror */
static const char *mallocStrerror()
{
    return strerror(errno);
}

/* internal functions */
static MfFile *Mf_AllocFile(void);
static MfTrack *Mf_AllocTrack(void);
static MfEvent *Mf_AllocEvent(void);
static MfMeta *Mf_AllocMeta(uint32_t length);

static PmError Mf_ReadMidiHeader(MfFile **into, FILE *from, uint16_t *expectedTracks);
static PmError Mf_ReadMidiTrack(MfFile *file, FILE *from);
static PmError Mf_ReadMidiEvent(MfTrack *track, FILE *from, uint8_t *pstatus, uint32_t *sz);
static PmError Mf_ReadMidiBignum(uint32_t *into, FILE *from, uint32_t *sz);
static PmError Mf_WriteMidiHeader(FILE *into, MfFile *from);
static PmError Mf_WriteMidiTrack(FILE *into, MfTrack *track);
static PmError Mf_WriteMidiEvent(FILE *into, MfEvent *event, uint8_t *pstatus);
static uint32_t Mf_GetMidiEventLength(MfEvent *event, uint8_t *pstatus);
static PmError Mf_WriteMidiBignum(FILE *into, uint32_t val);
static uint32_t Mf_GetMidiBignumLength(uint32_t val);

#define BAD_DATA { *((int *) 0) = 0; return pmBadData; }

#define MIDI_READ_N(into, fh, n) do { \
    if (fread((into), 1, n, (fh)) != n) BAD_DATA; \
} while (0)

#define MIDI_READ1(into, fh) do { \
    MIDI_READ_N(&(into), fh, 1); \
} while (0)

#define MIDI_READ2(into, fh) do { \
    unsigned char __mrbuf[2]; \
    MIDI_READ_N(__mrbuf, fh, 2); \
    (into) = (__mrbuf[0] << 8) + \
              __mrbuf[1]; \
} while (0)

#define MIDI_READ4(into, fh) do { \
    unsigned char __mrbuf[4]; \
    MIDI_READ_N(__mrbuf, fh, 4); \
    (into) = (__mrbuf[0] << 24) + \
             (__mrbuf[1] << 16) + \
             (__mrbuf[2] << 8) + \
              __mrbuf[3]; \
} while (0)

#define MIDI_WRITE1(fh, val) do { \
    fwrite(&(val), 1, 1, fh); \
} while (0)

#define MIDI_WRITE2(fh, val) do { \
    fprintf(fh, "%c%c", \
        (char) (((val) & 0xFF00) >> 8), \
        (char) ( (val) & 0x00FF)); \
} while (0)

#define MIDI_WRITE4(fh, val) do { \
    fprintf(fh, "%c%c%c%c", \
        (char) (((val) & 0xFF000000) >> 24), \
        (char) (((val) & 0x00FF0000) >> 16), \
        (char) (((val) & 0x0000FF00) >> 8), \
        (char) ( (val) & 0x000000FF)); \
} while (0)

/* only some message types have a data2 field */
#define TYPE_HAS_DATA2(status) (!(status >= 0xC0 && status <= 0xDF))

/* END MISCELLANY */

/* initialization */
PmError Mf_Initialize()
{
    AL.malloc = malloc;
    AL.strerror = mallocStrerror;
    AL.free = free;
    return pmNoError;
}

/* MIDI file */
static MfFile *Mf_AllocFile()
{
    return Mf_New(MfFile);
}

void Mf_FreeFile(MfFile *file)
{
    int i;
    for (i = 0; i < file->trackCt; i++) {
        Mf_FreeTrack(file->tracks[i]);
    }
    if (file->tracks) AL.free(file->tracks);
    AL.free(file);
}

MfFile *Mf_NewFile(uint16_t timeDivision)
{
    MfFile *ret = Mf_AllocFile();
    ret->timeDivision = timeDivision;
    return ret;
}

/* track */
static MfTrack *Mf_AllocTrack()
{
    return Mf_New(MfTrack);
}

void Mf_FreeTrack(MfTrack *track)
{
    MfEvent *ev = track->head, *next;
    while (ev) {
        next = ev->next;
        Mf_FreeEvent(ev);
        ev = next;
    }
    AL.free(track);
}

MfTrack *Mf_NewTrack(MfFile *file)
{
    MfTrack *track = Mf_AllocTrack();
    Mf_PushTrack(file, track);
    return track;
}

void Mf_PushTrack(MfFile *file, MfTrack *track)
{
    MfTrack **newTracks;
    if (file->tracks) {
        newTracks = Mf_Malloc((file->trackCt + 1) * sizeof(MfTrack *));
        memcpy(newTracks, file->tracks, file->trackCt * sizeof(MfTrack *));
        newTracks[file->trackCt++] = track;
        AL.free(file->tracks);
        file->tracks = newTracks;
    } else {
        file->tracks = Mf_Malloc(sizeof(MfTrack *));
        file->tracks[0] = track;
        file->trackCt = 1;
    }
}

/* and event */
static MfEvent *Mf_AllocEvent()
{
    return Mf_New(MfEvent);
}

void Mf_FreeEvent(MfEvent *event)
{
    if (event->meta) Mf_FreeMeta(event->meta);
    AL.free(event);
}

MfEvent *Mf_NewEvent()
{
    return Mf_AllocEvent();
}

void Mf_PushEvent(MfTrack *track, MfEvent *event)
{
    if (track->tail) {
        track->tail->next = event;
        event->absoluteTm = track->tail->absoluteTm + event->deltaTm;
        track->tail = event;
    } else {
        track->head = track->tail = event;
        event->absoluteTm = event->deltaTm;
    }
    event->next = NULL;
}

void Mf_PushEventHead(MfTrack *track, MfEvent *event)
{
    if (track->head) {
        event->next = track->head;
        track->head = event;
    } else {
        event->next = NULL;
        track->head = track->tail = event;
    }
}

/* meta-events have extra fields */
static MfMeta *Mf_AllocMeta(uint32_t length)
{
    MfMeta *ret = Mf_Calloc(sizeof(MfMeta) + length);
    ret->length = length;
    return ret;
}

void Mf_FreeMeta(MfMeta *meta)
{
    AL.free(meta);
}

MfMeta *Mf_NewMeta(uint32_t length)
{
    return Mf_AllocMeta(length);
}

/* read in a MIDI file */
PmError Mf_ReadMidiFile(MfFile **into, FILE *from)
{
    MfFile *file;
    PmError perr;
    int i;
    uint16_t expectedTracks;

    if ((perr = Mf_ReadMidiHeader(&file, from, &expectedTracks))) return perr;
    *into = file;

    for (i = 0; i < expectedTracks; i++) {
        if ((perr = Mf_ReadMidiTrack(file, from))) return perr;
    }

    return pmNoError;
}

static PmError Mf_ReadMidiHeader(MfFile **into, FILE *from, uint16_t *expectedTracks)
{
    MfFile *file;
    char magic[4];
    uint32_t chunkSize;

    /* check that the magic is right */
    if (fread(magic, 1, 4, from) != 4) BAD_DATA;
    if (memcmp(magic, "MThd", 4)) BAD_DATA;

    file = Mf_AllocFile();

    /* get the chunk size */
    MIDI_READ4(chunkSize, from);
    if (chunkSize != 6) BAD_DATA;

    MIDI_READ2(file->format, from);
    MIDI_READ2(*expectedTracks, from);
    MIDI_READ2(file->timeDivision, from);

    *into = file;
    return pmNoError;
}

static PmError Mf_ReadMidiTrack(MfFile *file, FILE *from)
{
    MfTrack *track;
    PmError perr;
    char magic[4];
    uint8_t status;
    uint32_t chunkSize;
    uint32_t rd;

    /* make sure it's a track */
    if (fread(magic, 1, 4, from) != 4) BAD_DATA;
    if (memcmp(magic, "MTrk", 4)) BAD_DATA;

    track = Mf_NewTrack(file);

    /* get the chunk size to be read */
    MIDI_READ4(chunkSize, from);

    /* and read it */
    status = 0;
    while (chunkSize > 0) {
        if ((perr = Mf_ReadMidiEvent(track, from, &status, &rd))) return perr;
        if (rd > chunkSize) BAD_DATA;
        chunkSize -= rd;
    }

    return pmNoError;
}

static PmError Mf_ReadMidiEvent(MfTrack *track, FILE *from, uint8_t *pstatus, uint32_t *sz)
{
    MfEvent *event;
    PmError perr;
    uint32_t deltaTm;
    uint8_t status, data1, data2;
    uint32_t rd = 0;

    /* read the delta time */
    if ((perr = Mf_ReadMidiBignum(&deltaTm, from, &rd))) return perr;

    event = Mf_NewEvent();
    event->deltaTm = deltaTm;
    Mf_PushEvent(track, event);

    /* and the rest */
    MIDI_READ1(status, from);
    rd++;

    /* hopefully it's a simple event */
    if (status < 0xF0) {
        if (status < 0x80) {
            /* status is from last event */
            data1 = status;
            status = *pstatus;
        } else {
            MIDI_READ1(data1, from); rd++;
        }

        /* not all have data2 (argh) */
        if (TYPE_HAS_DATA2(status)) {
            MIDI_READ1(data2, from); rd++;
        }

    } else if (status == 0xF0 || status == 0xF7 || status == 0xFF) { /* SysEx or meta */
        uint8_t mtype;
        uint32_t srd, length;
        MfMeta *meta;

        /* meta type */
        if (status == 0xFF) { /* actual meta */
            MIDI_READ1(mtype, from);
            rd++;
        } else {
            mtype = status;
        }

        /* data length */
        if ((perr = Mf_ReadMidiBignum(&length, from, &srd))) return perr;
        rd += srd;

        meta = Mf_NewMeta(length);
        meta->type = mtype;
        event->meta = meta;

        /* and the data itself */
        if (fread(meta->data, 1, length, from) != length) BAD_DATA;
        rd += length;

        /* carry over some data for convenience */
        data1 = data2 = 0;
        if (length >= 1) data1 = meta->data[0];
        if (length >= 2) data2 = meta->data[1];

    } else {
        fprintf(stderr, "Unrecognized MIDI event type %02X!\n", status);
        BAD_DATA;

    }

    event->e.message = Pm_Message(status, data1, data2);
    *pstatus = status;
    *sz = rd;
    return pmNoError;
}

static PmError Mf_ReadMidiBignum(uint32_t *into, FILE *from, uint32_t *sz)
{
    uint32_t ret = 0;
    int more = 1;
    unsigned char cur;

    *sz = 0;
    while (more) {
        if (fread(&cur, 1, 1, from) != 1) BAD_DATA;
        (*sz)++;

        /* is there more? */
        if (cur & 0x80) {
            more = 1;
            cur &= 0x7F;
        } else {
            more = 0;
        }

        ret <<= 7;
        ret += cur;
    }

    *into = ret;
    return pmNoError;
}

/* write out a MIDI file */
PmError Mf_WriteMidiFile(FILE *into, MfFile *from)
{
    PmError perr;
    int i;

    if ((perr = Mf_WriteMidiHeader(into, from))) return perr;

    for (i = 0; i < from->trackCt; i++) {
        if ((perr = Mf_WriteMidiTrack(into, from->tracks[i]))) return perr;
    }

    return pmNoError;
}

static PmError Mf_WriteMidiHeader(FILE *into, MfFile *from)
{
    /* magic and chunk size */
    fwrite("MThd\0\0\0\x06", 1, 8, into);

    /* and the rest */
    MIDI_WRITE2(into, from->format);
    MIDI_WRITE2(into, from->trackCt);
    MIDI_WRITE2(into, from->timeDivision);

    return pmNoError;
}

static PmError Mf_WriteMidiTrack(FILE *into, MfTrack *track)
{
    PmError perr;
    MfEvent *event;
    uint8_t status;
    uint32_t chunkSize;

    /* track header */
    fwrite("MTrk", 1, 4, into);

    /* get the chunk size to be written */
    chunkSize = 0;
    event = track->head;
    status = 0;
    while (event) {
        chunkSize += Mf_GetMidiEventLength(event, &status);
        event = event->next;
    }
    MIDI_WRITE4(into, chunkSize);

    /* and write it */
    event = track->head;
    status = 0;
    while (event) {
        if ((perr = Mf_WriteMidiEvent(into, event, &status))) return perr;
        event = event->next;
    }

    return pmNoError;
}

static PmError Mf_WriteMidiEvent(FILE *into, MfEvent *event, uint8_t *pstatus)
{
    PmError perr;
    uint8_t status, data1, data2;

    /* write the delta time */
    if ((perr = Mf_WriteMidiBignum(into, event->deltaTm))) return perr;

    /* get out the parts */
    status = Pm_MessageStatus(event->e.message);
    data1 = Pm_MessageData1(event->e.message);
    data2 = Pm_MessageData2(event->e.message);

    /* hopefully it's a simple event */
    if (status < 0xF0) {
        /* write the status if we need to */
        if (status != *pstatus) {
            MIDI_WRITE1(into, status);
        }

        /* then write data1 */
        MIDI_WRITE1(into, data1);

        /* not all have data2 (argh) */
        if (TYPE_HAS_DATA2(status)) {
            MIDI_WRITE1(into, data2);
        }

    } else if (event->meta) { /* has metadata */
        MfMeta *meta = event->meta;

        MIDI_WRITE1(into, status);

        /* meta type */
        if (status == 0xFF) { /* actual meta */
            MIDI_WRITE1(into, meta->type);
        }

        /* data length */
        if ((perr = Mf_WriteMidiBignum(into, meta->length))) return perr;

        /* and the data itself */
        fwrite(meta->data, 1, meta->length, into);

    } else {
        fprintf(stderr, "Unrecognized MIDI event type %02X!\n", status);
        BAD_DATA;

    }

    *pstatus = status;
    return pmNoError;
}

static uint32_t Mf_GetMidiEventLength(MfEvent *event, uint8_t *pstatus)
{
    uint32_t sz = 0;
    uint8_t status;

    /* delta time */
    sz += Mf_GetMidiBignumLength(event->deltaTm);

    /* get out the parts */
    status = Pm_MessageStatus(event->e.message);

    if (status < 0xF0) {
        /* write the status if we need to */
        if (status != *pstatus) sz++;

        /* then data1 */
        sz++;

        /* not all have data2 (argh) */
        if (TYPE_HAS_DATA2(status)) sz++;

    } else if (event->meta) { /* has metadata */
        MfMeta *meta = event->meta;

        /* status */
        sz++;

        /* meta type */
        if (status == 0xFF) sz++;

        /* data length */
        sz += Mf_GetMidiBignumLength(meta->length);

        /* and the data itself */
        sz += meta->length;

    } else {
        fprintf(stderr, "Unrecognized MIDI event type %02X!\n", status);
        BAD_DATA;

    }

    *pstatus = status;
    return sz;
}

static PmError Mf_WriteMidiBignum(FILE *into, uint32_t val)
{
    unsigned char buf[5];
    int bufl, i;

    /* write it into the buf first */
    buf[0] = 0;
    for (bufl = 0; val > 0; bufl++) {
        buf[bufl] = (val & 0x7F);
        val >>= 7;
    }
    if (bufl == 0) bufl = 1;

    /* mark the high bits */
    for (i = 1; i < bufl; i++) {
        buf[i] |= 0x80;
    }

    /* then write it out */
    for (i = bufl - 1; i >= 0; i--) {
        MIDI_WRITE1(into, buf[i]);
    }

    return pmNoError;
}

static uint32_t Mf_GetMidiBignumLength(uint32_t val)
{
    uint32_t sz = 1;
    val >>= 7;

    while (val > 0) {
        sz++;
        val >>= 7;
    }

    return sz;
}
