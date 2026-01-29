/*============================================================================
Copyright (c) 2024 Raspberry Pi
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
============================================================================*/

#include <locale.h>
#include <gtk/gtk.h>
#include "raindrop.h"

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

const char *xorients[4] = { "normal", "left", "inverted", "right" };

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

void update_openbox_system_config (void);
static void add_mode_i (int monitor, int w, int h, float f, gboolean i);
void load_openbox_config (void);
static void write_dispsetup (const char *infile);
void save_openbox_config (void);
void init_openbox_config (void);
void reload_openbox_config (void);
void revert_openbox_config (void);
void load_openbox_touchscreens (void);

/*----------------------------------------------------------------------------*/
/* System update */
/*----------------------------------------------------------------------------*/

void update_openbox_system_config (void)
{
    char *cmd;

    cmd = g_strdup_printf (SUDO_PREFIX "cp /var/tmp/dispsetup.sh /usr/share/dispsetup.sh");
    system (cmd);
    g_free (cmd);
}

/*----------------------------------------------------------------------------*/
/* Loading initial config */
/*----------------------------------------------------------------------------*/

static void add_mode_i (int monitor, int w, int h, float f, gboolean i)
{
    output_mode_t *mod;
    mod = g_new0 (output_mode_t, 1);
    mod->width = w;
    mod->height = h;
    mod->freq = f;
    mod->interlaced = i;
    mons[monitor].modes = g_list_append (mons[monitor].modes, mod);
}

void load_openbox_config (void)
{
    FILE *fp;
    char *line, *cptr;
    size_t len;
    int mon, w, h, i;
    gboolean inter;
    float f;

    char *loc = g_strdup (setlocale (LC_NUMERIC, ""));
    setlocale (LC_NUMERIC, "C");

    mon = -1;

    fp = popen ("xrandr", "r");
    if (fp)
    {
        line = NULL;
        len = 0;
        while (getline (&line, &len, fp) != -1)
        {
            if (line[0] != ' ')
            {
                if (strstr (line, "Screen")) continue;
                if (!strstr (line, " connected")) continue;
                mon++;
                if (strstr (line, "primary")) mons[mon].primary = TRUE;
                cptr = strtok (line, " ");
                mons[mon].name = g_strdup (cptr);
                while (cptr && cptr[0] != '(')
                {
                    if (cptr[0] >= '1' && cptr[0] <= '9')
                    {
                        sscanf (cptr, "%dx%d+%d+%d", &mons[mon].width, &mons[mon].height, &mons[mon].x, &mons[mon].y);
                        mons[mon].enabled = TRUE;
                    }
                    for (i = 0; i < 4; i++)
                        if (strstr (cptr, xorients[i])) mons[mon].rotation = i * 90;
                    cptr = strtok (NULL, " ");
                }
                if (mons[mon].rotation == 90 || mons[mon].rotation == 270)
                {
                    i = mons[mon].width;
                    mons[mon].width = mons[mon].height;
                    mons[mon].height = i;
                }
            }
            else if (line[4] != ' ')
            {
                inter = FALSE;
                cptr = strtok (line, " ");
                while (cptr)
                {
                    if (strstr (cptr, "x"))
                    {
                        sscanf (cptr, "%dx%d", &w, &h);
                        if (strstr (cptr, "i")) inter = TRUE;
                    }
                    if (strstr (cptr, "."))
                    {
                        sscanf (cptr, "%f", &f);
                        add_mode_i (mon, w, h, f, inter);
                    }
                    if (mons[mon].enabled && strstr (cptr, "*"))
                    {
                        mons[mon].freq = f;
                        mons[mon].interlaced = inter;
                    }
                    if (!mons[mon].enabled && strstr (cptr, "+"))
                    {
                        mons[mon].width = w;
                        mons[mon].height = h;
                        mons[mon].freq = f;
                        mons[mon].interlaced = inter;
                    }
                    cptr = strtok (NULL, " ");
                }
            }
        }
        free (line);
        pclose (fp);
    }

    setlocale (LC_NUMERIC, loc);
    g_free (loc);
}

/*----------------------------------------------------------------------------*/
/* Writing config */
/*----------------------------------------------------------------------------*/

static void write_dispsetup (const char *infile)
{
    char *cmd, *mstr, *tmp;
    int m;
    FILE *fp;

    char *loc = g_strdup (setlocale (LC_NUMERIC, ""));
    setlocale (LC_NUMERIC, "C");

    cmd = g_strdup ("xrandr");
    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        if (mons[m].enabled)
            mstr = g_strdup_printf ("--output %s%s --mode %dx%d%s --rate %0.3f --pos %dx%d --rotate %s", mons[m].name, mons[m].primary ? " --primary" : "",
                mons[m].width, mons[m].height, mons[m].interlaced ? "i" : "", mons[m].freq, mons[m].x, mons[m].y, xorients[mons[m].rotation / 90]);
        else
            mstr = g_strdup_printf ("--output %s --off", mons[m].name);
        tmp = g_strdup_printf ("%s %s", cmd, mstr);
        g_free (cmd);
        g_free (mstr);
        cmd = tmp;
    }

    fp = fopen (infile, "wb");
    fprintf (fp, "#!/bin/sh\nif %s --dryrun; then\n\t%s\nfi\n", cmd, cmd);
    g_free (cmd);

    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        if (mons[m].touchscreen == NULL) continue;
        cmd = g_strdup_printf ("xinput --map-to-output pointer:\"%s\" %s", mons[m].touchscreen, mons[m].name);
        fprintf (fp, "if xinput | grep -q \"%s\" ; then\n\t%s\nfi\n", mons[m].touchscreen, cmd);
        g_free (cmd);
    }

    fprintf (fp, "if [ -e /usr/share/ovscsetup.sh ] ; then\n\t/usr/share/ovscsetup.sh\nfi\nexit 0");
    fclose (fp);

    setlocale (LC_NUMERIC, loc);
    g_free (loc);
}

void save_openbox_config (void)
{
    const char *infile = "/var/tmp/dispsetup.bak";
    const char *outfile = "/var/tmp/dispsetup.sh";
    char *cmd;

    cmd = g_strdup_printf ("cp %s %s", outfile, infile);
    system (cmd);
    g_free (cmd);
    write_dispsetup (outfile);
}

void init_openbox_config (void)
{
    const char *outfile = "/var/tmp/dispsetup.sh";
    write_dispsetup (outfile);
}

/*----------------------------------------------------------------------------*/
/* Reload / reversion */
/*----------------------------------------------------------------------------*/

void reload_openbox_config (void)
{
    system ("/bin/bash /var/tmp/dispsetup.sh > /dev/null");
}

void revert_openbox_config (void)
{
    const char *infile = "/var/tmp/dispsetup.bak";
    const char *outfile = "/var/tmp/dispsetup.sh";
    char *cmd;

    cmd = g_strdup_printf ("cp %s %s", infile, outfile);
    system (cmd);
    g_free (cmd);
}

/*----------------------------------------------------------------------------*/
/* Touchscreens */
/*----------------------------------------------------------------------------*/

void load_openbox_touchscreens (void)
{
    FILE *fp;
    GList *ts;
    int sw, sh, tw, th, tx, ty, m;
    char *cmd;
    float matrix[6];

    char *loc = g_strdup (setlocale (LC_NUMERIC, ""));
    setlocale (LC_NUMERIC, "C");

    // get the screen size
    fp = popen ("xrandr | grep current  | cut -d \" \" -f 8,10", "r");
    if (fp)
    {
        if (fscanf (fp, "%d %d,", &sw, &sh) != 2)
        {
            sw = -1;
            sh = -1;
        }
        pclose (fp);
    }
    if (sw == -1 || sh == -1) return;

    // get the coord transform matrix for each touch device and calculate coords of touch device
    ts = touchscreens;
    while (ts)
    {
        cmd = g_strdup_printf ("xinput --list-props \"pointer:%s\" | grep Coordinate | cut -d : -f 2", (char *) ts->data);
        fp = popen (cmd, "r");
        if (fp)
        {
            if (fscanf (fp, "%f, %f, %f, %f, %f, %f,", matrix, matrix + 1, matrix + 2, matrix + 3, matrix + 4, matrix + 5) == 6)
            {
                if (matrix[0] != 1.0 || matrix[1] != 0.0 || matrix[2] != 0.0 || matrix[3] != 0.0 || matrix[4] != 1.0 || matrix[5] != 0.0)
                {
                    tw = ((float) sw + 0.5) * (matrix[0] + matrix[1]);
                    th = ((float) sh + 0.5) * (matrix[3] + matrix[4]);
                    tx = ((float) sw + 0.5) * matrix[2];
                    ty = ((float) sh + 0.5) * matrix[5];
                    if (tw < 0) tx += tw;
                    if (th < 0) ty += th;
                    if (tw * th < 0)
                    {
                        m = tw;
                        tw = th;
                        th = m;
                    }
                    if (tw < 0) tw *= -1;
                    if (th < 0) th *= -1;

                    for (m = 0; m < MAX_MONS; m++)
                    {
                        if (mons[m].modes == NULL) continue;
                        if (mons[m].width == tw && mons[m].height == th && mons[m].x == tx && mons[m].y == ty)
                        {
                            mons[m].touchscreen = g_strdup ((char *) ts->data);
                        }
                    }
                }
            }
            pclose (fp);
        }
        g_free (cmd);
        ts = ts->next;
    }

    setlocale (LC_NUMERIC, loc);
    g_free (loc);
}

void noop (void) {};

/*----------------------------------------------------------------------------*/
/* Function table */
/*----------------------------------------------------------------------------*/

wm_functions_t openbox_functions = {
    .init_config = init_openbox_config,
    .load_config = load_openbox_config,
    .load_touchscreens = load_openbox_touchscreens,
    .save_config = save_openbox_config,
    .save_touchscreens = noop,
    .reload_config = reload_openbox_config,
    .reload_touchscreens = noop,
    .revert_config = revert_openbox_config,
    .revert_touchscreens = noop,
    .update_system_config = update_openbox_system_config
};

/* End of file */
/*============================================================================*/
