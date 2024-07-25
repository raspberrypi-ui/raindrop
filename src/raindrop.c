#include <gtk/gtk.h>
#include <glib/gi18n.h>

/*----------------------------------------------------------------------------*/
/* Types and macros */
/*----------------------------------------------------------------------------*/

typedef struct {
    int width;
    int height;
    float freq;
} output_mode_t;

typedef struct {
    char *name;
    gboolean enabled;
    int width;
    int height;
    int x;
    int y;
    int rotation;
    float freq;
    GList *modes;
} monitor_t;

#define MAX_MONS 10

#define SNAP_DISTANCE 200

#define SCALE(n) (((n) * scalen) / scaled)
#define UPSCALE(n) (((n) * scaled) / scalen)

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

const char *orients[4] = { "normal", "90", "180", "270" };

monitor_t mons[MAX_MONS];
int mousex, mousey;
int screenw, screenh;
int curmon;
int scalen = 1, scaled = 16;
GtkWidget *da, *win, *undo;

/*----------------------------------------------------------------------------*/
/* Helper functions */
/*----------------------------------------------------------------------------*/

int screen_w (monitor_t mon)
{
    if (mon.rotation == 90 || mon.rotation == 270) return mon.height;
    else return mon.width;
}

int screen_h (monitor_t mon)
{
    if (mon.rotation == 90 || mon.rotation == 270) return mon.width;
    else return mon.height;
}

/*----------------------------------------------------------------------------*/
/* Drawing */
/*----------------------------------------------------------------------------*/

void draw (GtkDrawingArea *, cairo_t *cr, gpointer)
{
    PangoLayout *layout;
    PangoFontDescription *font;
    int m, w, h, charwid;

    GdkRGBA bg = { 0.25, 0.25, 0.25, 1.0 };
    GdkRGBA fg = { 1.0, 1.0, 1.0, 0.75 };
    GdkRGBA bk = { 0.0, 0.0, 0.0, 1.0 };

    screenw = gtk_widget_get_allocated_width (GTK_WIDGET (win));
    screenh = gtk_widget_get_allocated_height (GTK_WIDGET (win));

    // window fill
    gdk_cairo_set_source_rgba (cr, &bg);
    cairo_rectangle (cr, 0, 0, screenw, screenh);
    cairo_fill (cr);

    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL || mons[m].enabled == FALSE) continue;

        // background
        gdk_cairo_set_source_rgba (cr, &fg);
        cairo_rectangle (cr, SCALE(mons[m].x), SCALE(mons[m].y), SCALE(screen_w (mons[m])), SCALE(screen_h (mons[m])));
        cairo_fill (cr);

        // border
        gdk_cairo_set_source_rgba (cr, &bk);
        cairo_rectangle (cr, SCALE(mons[m].x), SCALE(mons[m].y), SCALE(screen_w (mons[m])), SCALE(screen_h (mons[m])));
        cairo_stroke (cr);

        // text label
        cairo_save (cr);
        font = pango_font_description_from_string ("sans");
        charwid = SCALE (mons[m].width) / strlen (mons[m].name);
        pango_font_description_set_size (font, charwid * PANGO_SCALE);
        layout = pango_cairo_create_layout (cr);
        pango_layout_set_text (layout, mons[m].name, -1);
        pango_layout_set_font_description (layout, font);
        pango_layout_get_pixel_size (layout, &w, &h);
        cairo_move_to (cr, SCALE(mons[m].x + screen_w (mons[m]) / 2), SCALE(mons[m].y + screen_h (mons[m]) / 2));
        cairo_rotate (cr, mons[m].rotation * G_PI / 180.0);
        cairo_rel_move_to (cr, -w / 2, -h / 2);
        pango_cairo_show_layout (cr, layout);
        cairo_restore (cr);
    }
}

/*----------------------------------------------------------------------------*/
/* Dragging monitor outlines */
/*----------------------------------------------------------------------------*/

void drag_motion (GtkWidget *da, GdkDragContext *, gint x, gint y, guint time)
{
    int m, xs, ys;

    if (curmon != -1)
    {
        mons[curmon].x = UPSCALE(x - mousex);
        mons[curmon].y = UPSCALE(y - mousey);

        // constrain to screen
        if (mons[curmon].x < 0) mons[curmon].x = 0;
        if (mons[curmon].y < 0) mons[curmon].y = 0;

        // snap top and left to other windows bottom or right, or to 0,0
        for (m = 0; m < MAX_MONS; m++)
        {
            if (mons[m].modes == NULL) continue;

            xs = m != curmon ? mons[m].x + screen_w (mons[m]) : 0;
            ys = m != curmon ? mons[m].y + screen_h (mons[m]) : 0;
            if (mons[curmon].x > xs - SNAP_DISTANCE && mons[curmon].x < xs + SNAP_DISTANCE) mons[curmon].x = xs;
            if (mons[curmon].y > ys - SNAP_DISTANCE && mons[curmon].y < ys + SNAP_DISTANCE) mons[curmon].y = ys;
        }

        gtk_widget_queue_draw (da);
    }
}

void drag_end (GtkWidget *, GdkDragContext *, gpointer)
{
    curmon = -1;
}

/*----------------------------------------------------------------------------*/
/* Context menu */
/*----------------------------------------------------------------------------*/

void check_frequency (int mon)
{
    GList *model;
    output_mode_t *mode;

    // set the highest frequency for this mode
    model = mons[mon].modes;
    while (model)
    {
        mode = (output_mode_t *) model->data;
        if (mons[mon].width == mode->width && mons[mon].height == mode->height)
        {
            mons[mon].freq = mode->freq;
            return;
        }
        model = model->next;
    }
}

void set_resolution (GtkMenuItem *item, gpointer data)
{
    int w, h;
    int mon = (long) data;

    sscanf (gtk_menu_item_get_label (item), "%dx%d", &w, &h);
    if (w != mons[mon].width || h != mons[mon].height)
    {
        mons[mon].width = w;
        mons[mon].height = h;
        check_frequency (mon);
    }
    gtk_widget_queue_draw (da);
}

void add_resolution (GtkWidget *menu, long mon, int width, int height)
{
    char *label = g_strdup_printf ("%dx%d", width, height);
    GtkWidget *item = gtk_check_menu_item_new_with_label (label);
    g_free (label);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[mon].width == width && mons[mon].height == height);
    g_signal_connect (item, "activate", G_CALLBACK (set_resolution), (gpointer) mon);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

void set_frequency (GtkMenuItem *item, gpointer data)
{
    int mon = (long) data;
    sscanf (gtk_menu_item_get_label (item), "%fHz", &(mons[mon].freq));
}

void add_frequency (GtkWidget *menu, long mon, float freq)
{
    char *label = g_strdup_printf ("%.3fHz", freq);
    GtkWidget *item = gtk_check_menu_item_new_with_label (label);
    g_free (label);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[mon].freq == freq);
    g_signal_connect (item, "activate", G_CALLBACK (set_frequency), (gpointer) mon);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

void set_orientation (GtkMenuItem *item, gpointer data)
{
    int mon = (long) data;
    sscanf (gtk_widget_get_name (GTK_WIDGET (item)), "%d", &(mons[mon].rotation));
    gtk_widget_queue_draw (da);
}

void add_orientation (GtkWidget *menu, long mon, const char *orient, int rotation)
{
    GtkWidget *item = gtk_check_menu_item_new_with_label (orient);
    char *tag = g_strdup_printf ("%d", rotation);
    gtk_widget_set_name (item, tag);
    g_free (tag);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[mon].rotation == rotation);
    g_signal_connect (item, "activate", G_CALLBACK (set_orientation), (gpointer) mon);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

void set_enable (GtkCheckMenuItem *item, gpointer data)
{
    int mon = (long) data;
    mons[mon].enabled = gtk_check_menu_item_get_active (item);
    gtk_widget_queue_draw (da);
}

GtkWidget *create_menu (long mon)
{
    GList *model;
    GtkWidget *item, *menu, *rmenu, *fmenu, *omenu;
    int lastw, lasth;
    float lastf;
    output_mode_t *mode;

    menu = gtk_menu_new ();

    item = gtk_check_menu_item_new_with_label (_("Active"));
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[mon].enabled);
    g_signal_connect (item, "activate", G_CALLBACK (set_enable), (gpointer) mon);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    if (!mons[mon].enabled)
    {
        gtk_widget_show_all (menu);
        return menu;
    }

    // resolution and frequency menus from mode data for this monitor
    // pre-sort list here?
    rmenu = gtk_menu_new ();
    fmenu = gtk_menu_new ();

    lastw = 0;
    lasth = 0;
    lastf = 0.0;
    model = mons[mon].modes;
    while (model)
    {
        mode = (output_mode_t *) model->data;
        if (lastw != mode->width || lasth != mode->height)
        {
            add_resolution (rmenu, mon, mode->width, mode->height);
            lastw = mode->width;
            lasth = mode->height;
        }
        if (mode->width == mons[mon].width && mode->height == mons[mon].height && lastf != mode->freq)
        {
            add_frequency (fmenu, mon, mode->freq);
            lastf = mode->freq;
        }
        model = model->next;
    }

    item = gtk_menu_item_new_with_label (_("Resolution"));
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), rmenu);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = gtk_menu_item_new_with_label (_("Frequency"));
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), fmenu);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    // orientation menu - generic
    omenu = gtk_menu_new ();
    add_orientation (omenu, mon, _("Normal"), 0);
    add_orientation (omenu, mon, _("Left"), 90);
    add_orientation (omenu, mon, _("Inverted"), 180);
    add_orientation (omenu, mon, _("Right"), 270);
    item = gtk_menu_item_new_with_label (_("Orientation"));
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), omenu);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    gtk_widget_show_all (menu);
    return menu;
}

/*----------------------------------------------------------------------------*/
/* Pop-up menu */
/*----------------------------------------------------------------------------*/

GtkWidget *create_popup (void)
{
    GtkWidget *item, *menu, *pmenu;
    int m;

    menu = gtk_menu_new ();

    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        item = gtk_menu_item_new_with_label (mons[m].name);
        pmenu = create_menu (m);
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), pmenu);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }
    gtk_widget_show_all (menu);
    return menu;
}

/*----------------------------------------------------------------------------*/
/* Loading initial config */
/*----------------------------------------------------------------------------*/

void add_mode (int monitor, int w, int h, float f)
{
    output_mode_t *mod;
    mod = g_new0 (output_mode_t, 1);
    mod->width = w;
    mod->height = h;
    mod->freq = f;
    mons[monitor].modes = g_list_append (mons[monitor].modes, mod);
}

void load_current_config (void)
{
    FILE *fp;
    char *line, *cptr;
    size_t len;
    int mon, w, h, i;
    float f;

    for (mon = 0; mon < MAX_MONS; mon++)
    {
        mons[mon].width = 0;
        mons[mon].height = 0;
        mons[mon].x = 0;
        mons[mon].y = 0;
        mons[mon].freq = 0.0;
        mons[mon].rotation = 0;
        mons[mon].modes = NULL;
        mons[mon].enabled = FALSE;
    }

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
}

/*----------------------------------------------------------------------------*/
/* Writing config */
/*----------------------------------------------------------------------------*/

gboolean copy_profile (FILE *fp, FILE *foutp, int nmons)
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

int write_config (FILE *fp)
{
    int m, nmons = 0;

    fprintf (fp, "profile {\n");
    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        nmons++;
        if (mons[m].enabled == FALSE)
        {
            fprintf (fp, "\t\toutput %s disable\n", mons[m].name);
        }
        else
        {
            fprintf (fp, "\t\toutput %s mode %dx%d@%.3f position %d,%d transform %s\n",
                mons[m].name, mons[m].width, mons[m].height, mons[m].freq,
                mons[m].x, mons[m].y, orients[mons[m].rotation / 90]);
        }
    }
    fprintf (fp, "}\n\n");

    return nmons;
}

void merge_configs (const char *infile, const char *outfile)
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

/*----------------------------------------------------------------------------*/
/* Event handlers */
/*----------------------------------------------------------------------------*/

void button_press_event (GtkWidget *, GdkEventButton ev, gpointer)
{
    GtkWidget *menu;
    int m;

    curmon = -1;
    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL || mons[m].enabled == FALSE) continue;

        if (ev.x > SCALE(mons[m].x) && ev.x < SCALE(mons[m].x + screen_w (mons[m]))
            && ev.y > SCALE(mons[m].y) && ev.y < SCALE(mons[m].y + screen_h (mons[m])))
        {
            curmon = m;
            mousex = ev.x - SCALE(mons[m].x);
            mousey = ev.y - SCALE(mons[m].y);
        }
    }

    if (ev.button == 3)
    {
        menu = create_menu (curmon);
        gtk_menu_popup_at_pointer (GTK_MENU (menu), gtk_get_current_event ());
    }
}

void handle_close (GtkButton *, gpointer)
{
    gtk_main_quit ();
}

void handle_apply (GtkButton *, gpointer)
{
    char *infile = g_build_filename (g_get_user_config_dir (), "kanshi/config.bak", NULL);
    char *outfile = g_build_filename (g_get_user_config_dir (), "kanshi/config", NULL);
    char *cmd = g_strdup_printf ("cp %s %s", outfile, infile);
    system (cmd);
    g_free (cmd);

    merge_configs (infile, outfile);

    g_free (infile);
    g_free (outfile);

    system ("pkill --signal SIGHUP kanshi");
    load_current_config ();
    gtk_widget_queue_draw (da);
    gtk_widget_set_sensitive (undo, TRUE);
}

void handle_undo (GtkButton *, gpointer)
{
    char *infile = g_build_filename (g_get_user_config_dir (), "kanshi/config.bak", NULL);
    char *outfile = g_build_filename (g_get_user_config_dir (), "kanshi/config", NULL);
    char *cmd = g_strdup_printf ("cp %s %s", infile, outfile);
    system (cmd);
    g_free (cmd);

    g_free (infile);
    g_free (outfile);

    system ("pkill --signal SIGHUP kanshi");
    load_current_config ();
    gtk_widget_queue_draw (da);
    gtk_widget_set_sensitive (undo, FALSE);
}

void handle_zoom (GtkButton *, gpointer data)
{
    if ((long) data == -1 && scaled < 32) scaled *= 2;
    if ((long) data == 1 && scaled > 8) scaled /= 2;
    gtk_widget_queue_draw (da);
}

void handle_menu (GtkButton *btn, gpointer)
{
    gtk_menu_popup_at_widget (GTK_MENU (create_popup ()), GTK_WIDGET (btn),
        GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_SOUTH_WEST, NULL);
}

void end_program (GtkWidget *, GdkEvent *, gpointer)
{
    gtk_main_quit ();
}

/*----------------------------------------------------------------------------*/
/* Main function */
/*----------------------------------------------------------------------------*/

int main (int argc, char *argv[])
{
    GtkBuilder *builder;

    load_current_config ();

    gtk_init (&argc, &argv);

    // build the UI
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/raindrop.ui");

    win = (GtkWidget *) gtk_builder_get_object (builder, "main_win");
    g_signal_connect (win, "delete-event", G_CALLBACK (end_program), NULL);

    da = (GtkWidget *) gtk_builder_get_object (builder, "da");
    gtk_drag_source_set (da, GDK_BUTTON1_MASK, NULL, 0, 0);
    gtk_drag_dest_set (da, 0, NULL, 0, 0);
    g_signal_connect (da, "draw", G_CALLBACK (draw), NULL);
    g_signal_connect (da, "button-press-event", G_CALLBACK (button_press_event), NULL);
    g_signal_connect (da, "drag-motion", G_CALLBACK (drag_motion), NULL);
    g_signal_connect (da, "drag-end", G_CALLBACK (drag_end), NULL);
    gtk_window_set_default_size (GTK_WINDOW (win), 500, 400);

    undo = (GtkWidget *) gtk_builder_get_object (builder, "btn_undo");
    g_signal_connect (undo, "clicked", G_CALLBACK (handle_undo), NULL);
    g_signal_connect (gtk_builder_get_object (builder, "btn_close"), "clicked", G_CALLBACK (handle_close), NULL);
    g_signal_connect (gtk_builder_get_object (builder, "btn_apply"), "clicked", G_CALLBACK (handle_apply), NULL);
    g_signal_connect (gtk_builder_get_object (builder, "btn_zin"), "clicked", G_CALLBACK (handle_zoom), (gpointer) 1);
    g_signal_connect (gtk_builder_get_object (builder, "btn_zout"), "clicked", G_CALLBACK (handle_zoom), (gpointer) -1);
    g_signal_connect (gtk_builder_get_object (builder, "btn_menu"), "clicked", G_CALLBACK (handle_menu), NULL);
    gtk_widget_set_sensitive (undo, FALSE);

    gtk_widget_show_all (win);

    gtk_main ();
    return 0;
}

/* End of file */
/*============================================================================*/
