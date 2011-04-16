#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "midifstream.h"

#define PCHECK(perr) do { \
    if (perr != pmNoError) { \
        fprintf(stderr, "%s\n", Pm_GetErrorText(perr)); \
        exit(1); \
    } \
} while (0)

#define PSF(perr, fun, args) do { \
    perr = fun args; \
    PCHECK(perr); \
} while (0)

#define PTCHECK(perr) do { \
    if (perr != ptNoError) { \
        fprintf(stderr, "PortTime error %d\n", (int) perr); \
        exit(1); \
    } \
} while (0)

#define PTSF(perr, fun, args) do { \
    perr = fun args; \
    PTCHECK(perr); \
} while (0)

#define MIDI_M_TEMPO 0x51

MfStream *stream = NULL;
PortMidiStream *ostream = NULL;
int ready = 0;

void play(PtTimestamp timestamp, void *ignore);

int main(int argc, char **argv)
{
    FILE *f;
    PmError perr;
    PtError pterr;
    MfFile *pf;
    int argi, i;
    char *arg, *nextarg, *file;

    PmDeviceID dev = -1;
    int list = 0;
    file = NULL;

    for (argi = 1; argi < argc; argi++) {
        arg = argv[argi];
        nextarg = argv[argi+1];
        if (arg[0] == '-') {
            if (!strcmp(arg, "-l")) {
                list = 1;
            } else if (!strcmp(arg, "-o") && nextarg) {
                dev = atoi(nextarg);
                argi++;
            } else {
                fprintf(stderr, "Invalid invocation.\n");
                exit(1);
            }
        } else {
            file = arg;
        }
    }

    PSF(perr, Pm_Initialize, ());
    PSF(perr, Mf_Initialize, ());
    PTSF(pterr, Pt_Start, (1, play, NULL));

    /* list devices */
    if (list) {
        int ct = Pm_CountDevices();
        PmDeviceID def = Pm_GetDefaultInputDeviceID();
        const PmDeviceInfo *devinf;

        for (i = 0; i < ct; i++) {
            devinf = Pm_GetDeviceInfo(i);
            printf("%d%s: %s%s %s\n", i, (def == i) ? "*" : "",
                (devinf->input) ? "I" : "",
                (devinf->output) ? "O" : "",
                devinf->name);
        }
    }

    /* choose device */
    if (dev == -1) {
        fprintf(stderr, "No device selected.\n");
        exit(1);
    }

    /* open it for input */
    PSF(perr, Pm_OpenOutput, (&ostream, dev, NULL, 1024, NULL, NULL, 0));

    /* open it for input */
    f = fopen(file, "rb");
    if (f == NULL) {
        perror(file);
        exit(1);
    }

    /* and read it */
    PSF(perr, Mf_ReadMidiFile, (&pf, f));
    fclose(f);

    /* now start running */
    stream = Mf_OpenStream(pf);
    Mf_StartStream(stream, Pt_Time());

    /* FIXME: I sure hope this doesn't get reordered >_> */
    ready = 1;

    while (1) Pt_Sleep(1<<30);

    return 0;
}

void play(PtTimestamp timestamp, void *ignore)
{
    MfEvent *event;
    int track;
    PmEvent ev;

    if (!ready) return;

    while (Mf_StreamRead(stream, &event, &track, 1) == 1) {
        ev = event->e;

        if (event->meta) {
            if (event->meta->type == MIDI_M_TEMPO && event->meta->length == 3) {
                /* send the tempo change back */
                PtTimestamp ts;
                unsigned char *data = event->meta->data;
                uint32_t tempo = (data[0] << 16) +
                    (data[1] << 8) +
                     data[2];
                Mf_StreamSetTempoTick(stream, &ts, event->absoluteTm, tempo);
            }
        } else {
            Pm_WriteShort(ostream, 0, ev.message);
        }

        Mf_FreeEvent(event);
    }

    if (Mf_StreamEmpty(stream) == TRUE) {
        Mf_FreeFile(Mf_CloseStream(stream));
        Pm_Terminate();
        exit(0);
    }
}
