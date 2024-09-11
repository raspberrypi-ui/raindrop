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
#include <libxml/xpath.h>
#include "raindrop.h"

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

const char *orients[4] = { "normal", "90", "180", "270" };

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

void update_labwc_system_config (void);
static void add_mode (int monitor, int w, int h, float f);
void load_labwc_config (void);
static gboolean copy_profile (FILE *fp, FILE *foutp, int nmons);
static int write_config (FILE *fp);
static void merge_configs (const char *infile, const char *outfile);
void save_labwc_config (void);
void reload_labwc_config (void);
void revert_labwc_config (void);
static void read_touchscreen_xml (char *filename);
void load_labwc_touchscreens (void);
static void write_touchscreens (char *filename);
void save_labwc_touchscreens (void);
void reload_labwc_touchscreens (void);
void revert_labwc_touchscreens (void);

/*----------------------------------------------------------------------------*/
/* System update */
/*----------------------------------------------------------------------------*/

void update_labwc_system_config (void)
{
    char *cmd;

    system (SUDO_PREFIX "mkdir -p /usr/share/labwc/");

    cmd = g_strdup_printf (SUDO_PREFIX "cp %s/kanshi/config /usr/share/labwc/config.kanshi",
        g_get_user_config_dir ());
    system (cmd);
    g_free (cmd);

    cmd = g_strdup_printf (SUDO_PREFIX "cp %s/labwc/rcgreeter.xml /usr/share/labwc/rc.xml",
        g_get_user_config_dir ());
    system (cmd);
    g_free (cmd);
}

/*----------------------------------------------------------------------------*/
/* Loading initial config */
/*----------------------------------------------------------------------------*/

static void add_mode (int monitor, int w, int h, float f)
{
    output_mode_t *mod;
    mod = g_new0 (output_mode_t, 1);
    mod->width = w;
    mod->height = h;
    mod->freq = f;
    mod->interlaced = FALSE;
    mons[monitor].modes = g_list_append (mons[monitor].modes, mod);
}

void load_labwc_config (void)
{
    FILE *fp;
    char *line, *cptr;
    size_t len;
    int mon, w, h, i;
    float f;

    char *loc = g_strdup (setlocale (LC_NUMERIC, ""));
    setlocale (LC_NUMERIC, "C");

    mon = -1;

    fp = popen ("wlr-randr", "r");
    if (fp)
    {
        line = NULL;
        len = 0;
        while (getline (&line, &len, fp) != -1)
        {
            if (line[0] != ' ')
            {
                mon++;
                cptr = line;
                while (*cptr != ' ') cptr++;
                *cptr = 0;
                mons[mon].name = g_strdup (line);
                if (strstr (line, "NOOP"))
                {
                    // add virtual modes for VNC display
                    add_mode (mon, 640, 480, 0);
                    add_mode (mon, 720, 480, 0);
                    add_mode (mon, 800, 600, 0);
                    add_mode (mon, 1024, 768, 0);
                    add_mode (mon, 1280, 720, 0);
                    add_mode (mon, 1280, 1024, 0);
                    add_mode (mon, 1600, 1200, 0);
                    add_mode (mon, 1920, 1080, 0);
                    add_mode (mon, 2048, 1080, 0);
                    add_mode (mon, 2560, 1440, 0);
                    add_mode (mon, 3200, 1800, 0);
                    add_mode (mon, 3840, 2160, 0);
                }
            }
            else if (line[2] != ' ')
            {
                if (strstr (line, "Position"))
                {
                    sscanf (line, "  Position: %d,%d", &w, &h);
                    mons[mon].x = w;
                    mons[mon].y = h;
                }
                else if (strstr (line, "Transform"))
                {
                    for (i = 0; i < 4; i++)
                        if (strstr (line, orients[i])) mons[mon].rotation = i * 90;
                }
                else if (strstr (line, "Enabled"))
                {
                    if (strstr (line, "no")) mons[mon].enabled = FALSE;
                    else mons[mon].enabled = TRUE;
                }
            }
            else if (line[4] != ' ')
            {
                sscanf (line, "    %dx%d px, %f Hz", &w, &h, &f);
                add_mode (mon, w, h, f);
                if ((mons[mon].enabled && strstr (line, "current"))
                    || (! mons[mon].enabled && strstr (line, "preferred")))
                {
                    mons[mon].width = w;
                    mons[mon].height = h;
                    mons[mon].freq = f;
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

static gboolean copy_profile (FILE *fp, FILE *foutp, int nmons)
{
    char *line;
    size_t len;
    gboolean valid = FALSE;
    char *buf, *tmp;
    int m;

    line = NULL;
    len = 0;
    while (getline (&line, &len, fp) != -1)
    {
        if (strstr (line, "profile"))
        {
            valid = TRUE;
            buf = g_strdup (line);
        }
        else if (valid)
        {
            if (strstr (line, "}"))
            {
                tmp = g_strdup_printf ("%s}\n", buf);
                g_free (buf);
                buf = tmp;

                if (nmons) fprintf (foutp, "%s\n", buf);
                g_free (buf);
                return TRUE;
            }
            else if (strstr (line, "output"))
            {
                tmp = g_strdup_printf ("%s%s", buf, line);
                g_free (buf);
                buf = tmp;

                for (m = 0; m < MAX_MONS; m++)
                {
                    if (mons[m].modes == NULL) continue;
                    if (strstr (line, mons[m].name)) nmons--;
                }
            }
        }
    }
    free (line);
    return FALSE;
}

static int write_config (FILE *fp)
{
    int m, nmons = 0;

    char *loc = g_strdup (setlocale (LC_NUMERIC, ""));
    setlocale (LC_NUMERIC, "C");

    fprintf (fp, "profile {\n");
    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        nmons++;
        if (mons[m].enabled == FALSE)
        {
            fprintf (fp, "\t\toutput %s disable\n", mons[m].name);
        }
        else if (mons[m].freq == 0.0)
        {
            fprintf (fp, "\t\toutput %s enable mode --custom %dx%d position %d,%d transform %s\n",
                mons[m].name, mons[m].width, mons[m].height,
                mons[m].x, mons[m].y, orients[mons[m].rotation / 90]);
        }
        else
        {
            fprintf (fp, "\t\toutput %s enable mode %dx%d@%.3f position %d,%d transform %s\n",
                mons[m].name, mons[m].width, mons[m].height, mons[m].freq,
                mons[m].x, mons[m].y, orients[mons[m].rotation / 90]);
        }
    }
    fprintf (fp, "}\n\n");

    setlocale (LC_NUMERIC, loc);
    g_free (loc);

    return nmons;
}

static void merge_configs (const char *infile, const char *outfile)
{
    FILE *finp = fopen (infile, "r");
    FILE *foutp = fopen (outfile, "w");

    // write the profile for this config
    int nmons = write_config (foutp);

    // copy any other profiles
    while (copy_profile (finp, foutp, nmons));

    fclose (finp);
    fclose (foutp);
}

void save_labwc_config (void)
{
    char *infile, *outfile, *cmd;

    infile = g_build_filename (g_get_user_config_dir (), "kanshi/config.bak", NULL);
    outfile = g_build_filename (g_get_user_config_dir (), "kanshi/config", NULL);
    cmd = g_strdup_printf ("cp %s %s", outfile, infile);
    system (cmd);
    g_free (cmd);
    merge_configs (infile, outfile);
    g_free (infile);
    g_free (outfile);
}

/*----------------------------------------------------------------------------*/
/* Reload / reversion */
/*----------------------------------------------------------------------------*/

void reload_labwc_config (void)
{
    system ("pkill --signal SIGHUP kanshi");
}

void revert_labwc_config (void)
{
    char *infile, *outfile, *cmd;

    infile = g_build_filename (g_get_user_config_dir (), "kanshi/config.bak", NULL);
    outfile = g_build_filename (g_get_user_config_dir (), "kanshi/config", NULL);
    cmd = g_strdup_printf ("cp %s %s", infile, outfile);
    system (cmd);
    g_free (cmd);
    g_free (infile);
    g_free (outfile);
}

/*----------------------------------------------------------------------------*/
/* Touchscreens */
/*----------------------------------------------------------------------------*/

static void read_touchscreen_xml (char *filename)
{
    xmlDocPtr xDoc;
    xmlNode *root_node, *child_node;
    xmlAttr *attr;
    char *dev, *mon;
    int m;

    if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR)) return;

    xmlInitParser ();
    LIBXML_TEST_VERSION

    xDoc = xmlParseFile (filename);
    if (xDoc != NULL)
    {
        root_node = xmlDocGetRootElement (xDoc);
        for (child_node = root_node->children; child_node; child_node = child_node->next)
        {
            if (child_node->type != XML_ELEMENT_NODE) continue;
            if (!g_strcmp0 ((char *) child_node->name, "touch"))
            {
                dev = NULL;
                mon = NULL;
                for (attr = child_node->properties; attr; attr = attr->next)
                {
                    if (!g_strcmp0 ((char *) attr->name, "deviceName"))
                        dev = g_strdup ((char *) attr->children->content);
                    if (!g_strcmp0 ((char *) attr->name, "mapToOutput"))
                        mon = g_strdup ((char *) attr->children->content);
                }
                if (dev && mon)
                {
                    for (m = 0; m < MAX_MONS; m++)
                    {
                        if (mons[m].modes == NULL) continue;
                        if (!g_strcmp0 (mons[m].name, mon))
                        {
                            mons[m].touchscreen = g_strdup (dev);
                        }
                    }
                }
                g_free (dev);
                g_free (mon);
            }
        }

        xmlFreeDoc (xDoc);
    }

    xmlCleanupParser ();
}

void load_labwc_touchscreens (void)
{
    char *infile;

    read_touchscreen_xml ("/etc/xdg/labwc/rc.xml");

    infile = g_build_filename (g_get_user_config_dir (), "labwc/rc.xml", NULL);
    read_touchscreen_xml (infile);
    g_free (infile);
}

static void write_touchscreens (char *filename)
{
    xmlDocPtr xDoc;
    xmlNode *root_node, *child_node, *next;
    int m;

    xmlInitParser ();
    LIBXML_TEST_VERSION
    if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
    {
        xDoc = xmlParseFile (filename);
        if (!xDoc) xDoc = xmlNewDoc ((xmlChar *) "1.0");
    }
    else xDoc = xmlNewDoc ((xmlChar *) "1.0");

    root_node = xmlDocGetRootElement (xDoc);
    if (root_node == NULL)
    {
        root_node = xmlNewNode (NULL, (xmlChar *) "openbox_config");
        xmlDocSetRootElement (xDoc, root_node);
        xmlNewNs (root_node, (xmlChar *) "http://openbox.org/3.4/rc", NULL);
    }

    child_node = root_node->children;
    while (child_node)
    {
        next = child_node->next;
        if (child_node->type == XML_ELEMENT_NODE)
        {
            if (!g_strcmp0 ((char *) child_node->name, "touch"))
            {
                xmlUnlinkNode (child_node);
                xmlFreeNode (child_node);
            }
        }
        child_node = next;
    }

    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        if (mons[m].touchscreen == NULL) continue;
        child_node = xmlNewNode (NULL, (xmlChar *) "touch");
        xmlSetProp (child_node, (xmlChar *) "deviceName", (xmlChar *) mons[m].touchscreen);
        xmlSetProp (child_node, (xmlChar *) "mapToOutput", (xmlChar *) mons[m].name);
        xmlAddChild (root_node, child_node);
    }

    xmlSaveFile (filename, xDoc);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();
}

void save_labwc_touchscreens (void)
{
    char *infile, *outfile, *cmd;

    infile = g_build_filename (g_get_user_config_dir (), "labwc/rc.bak", NULL);
    outfile = g_build_filename (g_get_user_config_dir (), "labwc/rc.xml", NULL);
    cmd = g_strdup_printf ("cp %s %s", outfile, infile);
    system (cmd);
    g_free (cmd);
    write_touchscreens (outfile);
    g_free (infile);
    g_free (outfile);

    outfile = g_build_filename (g_get_user_config_dir (), "labwc/rcgreeter.xml", NULL);
    cmd = g_strdup_printf ("cp /usr/share/labwc/rc.xml %s", outfile);
    system (cmd);
    g_free (cmd);
    write_touchscreens (outfile);
    g_free (outfile);
}

void reload_labwc_touchscreens (void)
{
    system ("labwc --reconfigure");
}

void revert_labwc_touchscreens (void)
{
    char *infile, *outfile, *cmd;

    infile = g_build_filename (g_get_user_config_dir (), "labwc/rc.bak", NULL);
    outfile = g_build_filename (g_get_user_config_dir (), "labwc/rc.xml", NULL);
    cmd = g_strdup_printf ("cp %s %s", infile, outfile);
    system (cmd);
    g_free (cmd);
    g_free (infile);
    g_free (outfile);
}

/*----------------------------------------------------------------------------*/
/* Function table */
/*----------------------------------------------------------------------------*/

wm_functions_t labwc_functions = {
    .load_config = load_labwc_config,
    .load_touchscreens = load_labwc_touchscreens,
    .save_config = save_labwc_config,
    .save_touchscreens = save_labwc_touchscreens,
    .reload_config = reload_labwc_config,
    .reload_touchscreens = reload_labwc_touchscreens,
    .revert_config = revert_labwc_config,
    .revert_touchscreens = revert_labwc_touchscreens,
    .update_system_config = update_labwc_system_config
};

/* End of file */
/*============================================================================*/
