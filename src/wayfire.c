/*============================================================================
Copyright (c) 2024 Raspberry Pi Holdings Ltd.
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
#include <glib.h>
#include "raindrop.h"

extern void load_labwc_config (void);
extern void noop (void);

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

extern const char *orients[4];

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

void update_wayfire_system_config (void);
void save_wayfire_config (void);
void revert_wayfire_config (void);
void load_wayfire_touchscreens (void);

/*----------------------------------------------------------------------------*/
/* System update */
/*----------------------------------------------------------------------------*/

void update_wayfire_system_config (void)
{
    system (SUDO_PREFIX "cp /tmp/greeter.ini /usr/share/greeter.ini");
}

/*----------------------------------------------------------------------------*/
/* Writing config */
/*----------------------------------------------------------------------------*/

static void update_wayfire_ini (char *filename)
{
    GKeyFile *kf;
    char *grp, *set;
    int m;

    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, filename, G_KEY_FILE_KEEP_COMMENTS, NULL);
    
    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        grp = g_strdup_printf ("output:%s", mons[m].name);
        g_key_file_remove_group (kf, grp, NULL);
        if (mons[m].enabled)
        {
            set = g_strdup_printf ("%dx%d@%d", mons[m].width, mons[m].height, (int)((mons[m].freq + 0.0005) * 1000.0));
            g_key_file_set_string (kf, grp, "mode", set);
            g_free (set);

            set = g_strdup_printf ("%d,%d", mons[m].x, mons[m].y);
            g_key_file_set_string (kf, grp, "position", set);
            g_free (set);
            
            g_key_file_set_string (kf, grp, "transform", orients[mons[m].rotation / 90]);
        }
        else g_key_file_set_string (kf, grp, "mode", "off");
        
        if (mons[m].touchscreen)
        {
            set = g_strdup_printf ("input-device:%s", mons[m].touchscreen);
            g_key_file_set_string (kf, set, "output", mons[m].name);
            g_free (set);
        }
        g_free (grp);
    }

    g_key_file_save_to_file (kf, filename, NULL);
    g_key_file_free (kf);
}

void save_wayfire_config (void)
{
    char *infile, *outfile, *cmd;

    infile = g_build_filename (g_get_user_config_dir (), "wayfire.bak", NULL);
    outfile = g_build_filename (g_get_user_config_dir (), "wayfire.ini", NULL);

    cmd = g_strdup_printf ("test -f %s", outfile);
    if (!system (cmd))
    {
        g_free (cmd);
        cmd = g_strdup_printf ("cp /etc/wayfire/template.ini %s", outfile);
        system (cmd);
    }
    g_free (cmd);

    cmd = g_strdup_printf ("cp %s %s", outfile, infile);
    system (cmd);
    g_free (infile);
    g_free (cmd);

    update_wayfire_ini (outfile);
    g_free (outfile);

    if (system ("test -f /usr/share/greeter.ini"))
        system ("cp /usr/share/greeter.ini /tmp/greeter.ini");
    else
        system ("cp /etc/wayfire/gtemplate.ini /tmp/greeter.ini");

    update_wayfire_ini ("/tmp/greeter.ini");
}

/*----------------------------------------------------------------------------*/
/* Reload / reversion */
/*----------------------------------------------------------------------------*/

void revert_wayfire_config (void)
{
    char *infile, *outfile, *cmd;

    infile = g_build_filename (g_get_user_config_dir (), "wayfire.bak", NULL);
    outfile = g_build_filename (g_get_user_config_dir (), "wayfire.ini", NULL);
    cmd = g_strdup_printf ("cp %s %s", infile, outfile);
    system (cmd);
    g_free (cmd);
    g_free (infile);
    g_free (outfile);
}

/*----------------------------------------------------------------------------*/
/* Touchscreens */
/*----------------------------------------------------------------------------*/

void load_wayfire_touchscreens (void)
{
    GKeyFile *kf;
    GError *err;
    char *infile;
    gchar **grps, *mon;
    int i, m;
    gsize ngrps;

    infile = g_build_filename (g_get_user_config_dir (), "wayfire.ini", NULL);

    kf = g_key_file_new ();

    err = NULL;
    g_key_file_load_from_file (kf, infile, G_KEY_FILE_KEEP_COMMENTS, &err);
    if (!err)
    {
        grps = g_key_file_get_groups (kf, &ngrps);

        for (i = 0; i < ngrps; i++)
        {
            if (strstr (grps[i], "input-device") != NULL)
            {
                err = NULL;
                mon = g_key_file_get_string (kf, grps[i], "output", &err);

                if (!err)
                {
                    for (m = 0; m < MAX_MONS; m++)
                    {
                        if (mons[m].modes == NULL) continue;
                        if (!g_strcmp0 (mon, mons[m].name))
                            mons[m].touchscreen = g_strdup (grps[i] + 13);
                    }
                }
                else g_error_free (err);

                g_free (mon);
            }
        }

        g_strfreev (grps);
    }
    else g_error_free (err);

    g_key_file_free (kf);
    g_free (infile);
}

/*----------------------------------------------------------------------------*/
/* Function table */
/*----------------------------------------------------------------------------*/

wm_functions_t wayfire_functions = {
    .load_config = load_labwc_config,
    .load_touchscreens = load_wayfire_touchscreens,
    .save_config = save_wayfire_config,
    .save_touchscreens = noop,
    .reload_config = noop,
    .reload_touchscreens = noop,
    .revert_config = revert_wayfire_config,
    .revert_touchscreens = noop,
    .update_system_config = update_wayfire_system_config
};

/* End of file */
/*============================================================================*/
