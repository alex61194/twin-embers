/* CtrSettings across a restart: the settings are read once per process, so each "session" here is a new process of this same program
 * (run with no arguments it runs the sessions one after another). Checks that a private room's code survives a restart (also in a file
 * with CRLF line ends), and that a code which is no longer one (an older build's six digits, damage) switches the private room off
 * in the file and leaves a note until the player chooses a room again. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "3ds_settings.h"   /* built with CTR_SETTINGS_PATH "build/settings_reload_test.cfg" */

#define CODE "4829137650218394"

static void PutFile(const char *text)
{
    FILE *file = fopen(CTR_SETTINGS_PATH, "wb");

    assert(file != NULL);
    fputs(text, file);
    fclose(file);
}

static int FileHas(const char *text)
{
    static char block[2048];
    FILE *file = fopen(CTR_SETTINGS_PATH, "rb");
    size_t n;

    assert(file != NULL);
    n = fread(block, 1, sizeof(block) - 1, file);
    fclose(file);
    block[n] = 0;
    return strstr(block, text) != NULL;
}

/* One session: what it does, and what it must find. */
static int Session(const char *name)
{
    if (!strcmp(name, "save")) {                    /* the first run: a code is made and the private room chosen */
        remove(CTR_SETTINGS_PATH);
        CtrSettings_SetRoomCode(CODE);
        CtrSettings_SetPrivateRoom(1);
    } else if (!strcmp(name, "kept")) {             /* the next run finds both, and no note */
        assert(!strcmp(CtrSettings_RoomCode(), CODE) && CtrSettings_PrivateRoom() && !CtrSettings_RoomCodeDropped());
    } else if (!strcmp(name, "crlf")) {             /* a file edited on a computer: CR LF line ends */
        PutFile("turbo=2\r\nprivroom=1\r\nroomcode=" CODE "\r\n");
    } else if (!strcmp(name, "crlf-kept")) {
        assert(CtrSettings_Turbo() == 2 && !strcmp(CtrSettings_RoomCode(), CODE) && CtrSettings_PrivateRoom() && !CtrSettings_RoomCodeDropped());
    } else if (!strcmp(name, "nonl")) {             /* the last line with no line end at all */
        PutFile("privroom=1\nroomcode=" CODE);
    } else if (!strcmp(name, "nonl-kept")) {
        assert(!strcmp(CtrSettings_RoomCode(), CODE) && CtrSettings_PrivateRoom() && !CtrSettings_RoomCodeDropped());
    } else if (!strcmp(name, "legacy")) {           /* an older build: six digits and the private room on */
        PutFile("turbo=3\nprivroom=1\nroomcode=123456\n");
    } else if (!strcmp(name, "dropped")) {          /* it is read: private off, no code, the note, the rest kept ... */
        assert(CtrSettings_Turbo() == 3 && !CtrSettings_PrivateRoom() && !CtrSettings_RoomCode()[0] && CtrSettings_RoomCodeDropped());
    } else if (!strcmp(name, "dropped-file")) {     /* ... and the file itself was rewritten: the old code is gone, the note is kept */
        assert(!FileHas("roomcode=123456") && !FileHas("privroom=1") && FileHas("roomnote=1\n") && FileHas("turbo=3\n"));
    } else if (!strcmp(name, "dropped-again")) {    /* the next run still tells, and is still not in the public room unseen */
        assert(!CtrSettings_PrivateRoom() && CtrSettings_RoomCodeDropped());
    } else if (!strcmp(name, "ack")) {              /* the player has read it */
        assert(CtrSettings_RoomCodeDropped());
        CtrSettings_AckRoomCodeDropped();
        assert(!CtrSettings_RoomCodeDropped());
    } else if (!strcmp(name, "acked")) {
        assert(!CtrSettings_RoomCodeDropped() && !CtrSettings_PrivateRoom() && !FileHas("roomnote"));
    } else if (!strcmp(name, "newcode")) {          /* choosing a new code also ends the note, and the room is private again */
        assert(CtrSettings_RoomCodeDropped());
        CtrSettings_SetRoomCode(CODE);
        CtrSettings_SetPrivateRoom(1);
        assert(!CtrSettings_RoomCodeDropped());
    } else if (!strcmp(name, "newcode-kept")) {
        assert(!strcmp(CtrSettings_RoomCode(), CODE) && CtrSettings_PrivateRoom() && !CtrSettings_RoomCodeDropped() && !FileHas("roomnote"));
    } else if (!strcmp(name, "bad17")) {            /* 17 digits */
        PutFile("privroom=1\nroomcode=" CODE "5\n");
    } else if (!strcmp(name, "badtext")) {          /* letters */
        PutFile("privroom=1\nroomcode=48291376502183ab\n");
    } else if (!strcmp(name, "plain")) {            /* too plain (five different digits) */
        PutFile("privroom=1\nroomcode=1212121212123434\n");
    } else if (!strcmp(name, "short")) {            /* fifteen digits */
        PutFile("privroom=1\nroomcode=482913765021839\n");
    } else if (!strcmp(name, "nocode")) {           /* the private room on and no code at all */
        PutFile("privroom=1\n");
    } else if (!strcmp(name, "bad-dropped")) {      /* each of these is read as a dropped code */
        assert(!CtrSettings_PrivateRoom() && !CtrSettings_RoomCode()[0] && CtrSettings_RoomCodeDropped());
    } else if (!strcmp(name, "public")) {           /* the public room with no code: nothing was lost, so no note */
        PutFile("turbo=2\n");
    } else if (!strcmp(name, "public-kept")) {
        assert(!CtrSettings_PrivateRoom() && !CtrSettings_RoomCode()[0] && !CtrSettings_RoomCodeDropped() && CtrSettings_Turbo() == 2);
    } else if (!strcmp(name, "style-none")) {       /* no file at all: Poke Ball, the default, and nothing about it is written */
        remove(CTR_SETTINGS_PATH);
        assert(CtrSettings_Style() == 1);
        CtrSettings_SetTurbo(2);                    /* a write of the file, with the style at its default */
        assert(!FileHas("style="));
    } else if (!strcmp(name, "style-none-kept")) {
        assert(CtrSettings_Style() == 1 && CtrSettings_Turbo() == 2);
    } else if (!strcmp(name, "style-embers")) {     /* the choice of Ascuas is kept, as style=0 */
        remove(CTR_SETTINGS_PATH);
        CtrSettings_SetStyle(0);
        assert(FileHas("style=0\n"));
    } else if (!strcmp(name, "style-embers-kept")) {
        assert(CtrSettings_Style() == 0);
        CtrSettings_SetStyle(1);                    /* and back to the default: the line goes */
        assert(!FileHas("style="));
    } else if (!strcmp(name, "style-back-kept")) {
        assert(CtrSettings_Style() == 1);
    } else if (!strcmp(name, "style-old1")) {       /* an earlier build wrote style=1 for Poke Ball: still Poke Ball */
        PutFile("turbo=3\nstyle=1\n");
    } else if (!strcmp(name, "style-old1-kept")) {
        assert(CtrSettings_Style() == 1 && CtrSettings_Turbo() == 3);
    } else if (!strcmp(name, "style-bad")) {        /* a damaged value is the default */
        PutFile("style=7\n");
    } else if (!strcmp(name, "style-bad-kept")) {
        assert(CtrSettings_Style() == 1);
    } else if (!strcmp(name, "codeoff")) {          /* a valid code with the private room off stays a code, with no note */
        PutFile("roomcode=" CODE "\n");
    } else if (!strcmp(name, "codeoff-kept")) {
        assert(!CtrSettings_PrivateRoom() && !strcmp(CtrSettings_RoomCode(), CODE) && !CtrSettings_RoomCodeDropped());
    } else {
        fprintf(stderr, "unknown session %s\n", name);
        return 1;
    }
    return 0;
}

static int Run(const char *self, const char *name)
{
    char command[512];
    const char *c;
    size_t n = 0;

    /* The shell of Windows wants backslashes in a path it is given. */
    for (c = self; *c && n < sizeof(command) - 64; ++c)
#ifdef _WIN32
        command[n++] = *c == '/' ? '\\' : *c;
#else
        command[n++] = *c;
#endif
    command[n] = 0;
#ifndef _WIN32
    if (!strchr(self, '/'))
        strcpy(command, "./"), strcat(command, self);
#endif
    strcat(command, " ");
    strcat(command, name);
    return system(command);
}

int main(int argc, char **argv)
{
    static const char *const sessions[][4] = {
        { "save", "kept", NULL, NULL },
        { "crlf", "crlf-kept", NULL, NULL },
        { "nonl", "nonl-kept", NULL, NULL },
        { "legacy", "dropped", "dropped-file", "dropped-again" },
        { "ack", "acked", NULL, NULL },
        { "legacy", "dropped", "newcode", "newcode-kept" },
        { "bad17", "bad-dropped", NULL, NULL },
        { "badtext", "bad-dropped", NULL, NULL },
        { "plain", "bad-dropped", NULL, NULL },
        { "short", "bad-dropped", NULL, NULL },
        { "nocode", "bad-dropped", NULL, NULL },
        { "public", "public-kept", NULL, NULL },
        { "codeoff", "codeoff-kept", NULL, NULL },
        { "style-none", "style-none-kept", NULL, NULL },
        { "style-embers", "style-embers-kept", "style-back-kept", NULL },
        { "style-old1", "style-old1-kept", NULL, NULL },
        { "style-bad", "style-bad-kept", NULL, NULL },
    };

    if (argc > 1)
        return Session(argv[1]);
    for (size_t i = 0; i < sizeof(sessions) / sizeof(sessions[0]); ++i)
        for (size_t k = 0; k < 4 && sessions[i][k]; ++k)
            if (Run(argv[0], sessions[i][k]) != 0) {
                fprintf(stderr, "settings_reload_test: session \"%s\" (group %u) failed\n", sessions[i][k], (unsigned)i);
                return 1;
            }
    remove(CTR_SETTINGS_PATH);
    puts("settings_reload_test ok");
    return 0;
}
