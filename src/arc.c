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
    int width;
    int height;
    int x;
    int y;
    int rotation;
    float freq;
    GList *modes;
} monitor_t;

#define MAX_MONS 2

#define SNAP_DISTANCE 200

#define SCALE(n) (((n) * scalen) / scaled)
#define UPSCALE(n) (((n) * scaled) / scalen)

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

monitor_t mons[MAX_MONS];
int mousex, mousey;
int screenw, screenh;
int curmon;
int scalen = 1, scaled = 16;
GtkWidget *da;

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

    // window fill
    gdk_cairo_set_source_rgba (cr, &bg);
    cairo_rectangle (cr, 0, 0, screenw, screenh);
    cairo_fill (cr);

    for (m = 0; m < MAX_MONS; m++)
    {
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
        if (SCALE(mons[curmon].x + screen_w (mons[curmon])) > screenw) mons[curmon].x = UPSCALE(screenw - SCALE(screen_w (mons[curmon])));
        if (SCALE(mons[curmon].y + screen_h (mons[curmon])) > screenh) mons[curmon].y = UPSCALE(screenh - SCALE(screen_h (mons[curmon])));

        // snap top and left to other windows bottom or right, or to 0,0
        for (m = 0; m < MAX_MONS; m++)
        {
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

void check_frequency (void)
{
    GList *model;
    output_mode_t *mode;

    // set the highest frequency for this mode
    model = mons[curmon].modes;
    while (model)
    {
        mode = (output_mode_t *) model->data;
        if (mons[curmon].width == mode->width || mons[curmon].height == mode->height)
        {
            mons[curmon].freq = mode->freq;
            return;
        }
        model = model->next;
    }
}

void set_resolution (GtkMenuItem *item, gpointer)
{
    int w, h;

    sscanf (gtk_menu_item_get_label (item), "%dx%d", &w, &h);
    if (w != mons[curmon].width || h != mons[curmon].height)
    {
        mons[curmon].width = w;
        mons[curmon].height = h;
        check_frequency ();
    }
    gtk_widget_queue_draw (da);
}

void add_resolution (GtkWidget *menu, int width, int height)
{
    char *label = g_strdup_printf ("%dx%d", width, height);
    GtkWidget *item = gtk_check_menu_item_new_with_label (label);
    g_free (label);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[curmon].width == width && mons[curmon].height == height);
    g_signal_connect (item, "activate", G_CALLBACK (set_resolution), NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

void set_frequency (GtkMenuItem *item, gpointer)
{
    sscanf (gtk_menu_item_get_label (item), "%fHz", &(mons[curmon].freq));
}

void add_frequency (GtkWidget *menu, float freq)
{
    char *label = g_strdup_printf ("%.3fHz", freq);
    GtkWidget *item = gtk_check_menu_item_new_with_label (label);
    g_free (label);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[curmon].freq == freq);
    g_signal_connect (item, "activate", G_CALLBACK (set_frequency), NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

void set_orientation (GtkMenuItem *item, gpointer)
{
    sscanf (gtk_menu_item_get_label (item), "%d", &(mons[curmon].rotation));
    gtk_widget_queue_draw (da);
}

void add_orientation (GtkWidget *menu, int rotation)
{
    char *label = g_strdup_printf ("%d", rotation);
    GtkWidget *item = gtk_check_menu_item_new_with_label (label);
    g_free (label);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[curmon].rotation == rotation);
    g_signal_connect (item, "activate", G_CALLBACK (set_orientation), NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

void show_menu (void)
{
    GList *model;
    GtkWidget *item, *menu, *rmenu, *fmenu, *omenu;
    int lastw, lasth;
    output_mode_t *mode;

    menu = gtk_menu_new ();

    // resolution and frequency menus from mode data for this monitor
    // pre-sort list here?
    rmenu = gtk_menu_new ();
    fmenu = gtk_menu_new ();

    lastw = 0;
    lasth = 0;
    model = mons[curmon].modes;
    while (model)
    {
        mode = (output_mode_t *) model->data;
        if (lastw != mode->width || lasth != mode->height)
        {
            add_resolution (rmenu, mode->width, mode->height);
            lastw = mode->width;
            lasth = mode->height;
        }
        if (mode->width == mons[curmon].width && mode->height == mons[curmon].height)
            add_frequency (fmenu, mode->freq);
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
    add_orientation (omenu, 0);
    add_orientation (omenu, 90);
    add_orientation (omenu, 180);
    add_orientation (omenu, 270);
    item = gtk_menu_item_new_with_label (_("Orientation"));
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), omenu);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    gtk_widget_show_all (menu);
    gtk_menu_popup_at_pointer (GTK_MENU (menu), gtk_get_current_event ());
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
    mons[0].x = 0;
    mons[0].y = 0;
    mons[0].width = 3840;
    mons[0].height = 2160;
    mons[0].freq = 59.94;
    mons[0].rotation = 0;
    mons[0].name = g_strdup ("HDMI-A-1");
    mons[0].modes = NULL;
    add_mode (0, 3840, 2160, 60.0);
    add_mode (0, 3840, 2160, 59.94);
    add_mode (0, 3840, 2160, 50.0);
    add_mode (0, 1920, 1080, 60.0);
    add_mode (0, 1920, 1080, 50.0);
    add_mode (0, 1600, 1200, 50.0);
    add_mode (0, 800, 600, 50.0);

    mons[1].x = 3840;
    mons[1].y = 0;
    mons[1].width = 1920;
    mons[1].height = 1080;
    mons[1].freq = 50.0;
    mons[1].rotation = 90;
    mons[1].name = g_strdup ("HDMI-A-2");
    mons[1].modes = NULL;
    add_mode (1, 1920, 1080, 50.0);
}

/*----------------------------------------------------------------------------*/
/* Event handlers */
/*----------------------------------------------------------------------------*/

void button_press_event (GtkWidget *, GdkEventButton ev, gpointer)
{
    int m;

    curmon = -1;
    for (m = 0; m < MAX_MONS; m++)
    {
        if (ev.x > SCALE(mons[m].x) && ev.x < SCALE(mons[m].x + screen_w (mons[m]))
            && ev.y > SCALE(mons[m].y) && ev.y < SCALE(mons[m].y + screen_h (mons[m])))
        {
            curmon = m;
            switch (ev.button)
            {
                case 1 :    mousex = ev.x - SCALE(mons[m].x);
                            mousey = ev.y - SCALE(mons[m].y);
                            break;

                case 3 :    show_menu ();
                            break;
            }
        }
    }
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
    load_current_config ();

    gtk_init (&argc, &argv);
    GtkWidget *win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (win, "delete-event", G_CALLBACK (end_program), NULL);

    da = gtk_drawing_area_new ();
    gtk_drag_source_set (da, GDK_BUTTON1_MASK, NULL, 0, 0);
    gtk_drag_dest_set (da, 0, NULL, 0, 0);
    g_signal_connect (da, "draw", G_CALLBACK (draw), NULL);
    g_signal_connect (da, "button-press-event", G_CALLBACK (button_press_event), NULL);
    g_signal_connect (da, "drag-motion", G_CALLBACK (drag_motion), NULL);
    g_signal_connect (da, "drag-end", G_CALLBACK (drag_end), NULL);
    gtk_widget_set_size_request (da, 500, 400);
    gtk_container_add (GTK_CONTAINER (win), da);

    gtk_widget_show_all (win);
    screenw = gtk_widget_get_allocated_width (GTK_WIDGET (da));
    screenh = gtk_widget_get_allocated_height (GTK_WIDGET (da));

    gtk_main ();
    return 0;
}

/* End of file */
/*============================================================================*/
