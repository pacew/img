#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <gtk/gtk.h>

char *filename;

void
usage (void)
{
	fprintf (stderr, "usage: pview file\n");
	exit (1);
}


GtkWidget *window;
GdkPixbuf *pixbuf;
GdkGC *gc;

gboolean
expose_event ()
{
	if (pixbuf) {
		gdk_draw_rectangle (window->window,
				    gc, TRUE, 0, 0,
				    window->allocation.width,
				    window->allocation.height);

		gdk_pixbuf_render_to_drawable (pixbuf,
					       GDK_WINDOW (window->window),
					       gc,
					       0, 0,
					       0, 0,
					       gdk_pixbuf_get_width (pixbuf),
					       gdk_pixbuf_get_height (pixbuf),
					       GDK_RGB_DITHER_NORMAL,
					       0, 0);
	}
	return (TRUE);
}

void
read_image (void)
{
	if (pixbuf)
		g_object_unref (pixbuf);

	pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
	if (pixbuf == NULL) {
		fprintf (stderr, "can't open %s\n", filename);
		return;
	}

	gtk_window_resize (GTK_WINDOW (window), 
			   gdk_pixbuf_get_width (pixbuf),
			   gdk_pixbuf_get_height (pixbuf));

}

gboolean
key_press_event (GtkWidget *widget, GdkEventKey *ev, gpointer data)
{
	switch (ev->keyval) {
	case 'r':
		read_image ();
		gtk_widget_queue_draw_area (window, 0, 0,
					    window->allocation.width,
					    window->allocation.height);
		break;
	case 'q':
	case 'c':
	case 'w':
		exit (0);
	}
	return (TRUE);
}


int
main (int argc, char **argv)
{
	int c;

	gtk_init (&argc, &argv);

	while ((c = getopt (argc, argv, "")) != EOF) {
		switch (c) {
		default:
			usage ();
		}
	}

	if (optind >= argc)
		usage ();

	filename = argv[optind++];

	if (optind != argc)
		usage ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), filename);

	read_image ();
	if (pixbuf == NULL)
		exit (1); /* error message already printed */

	gtk_window_set_default_size (GTK_WINDOW (window), 
				     gdk_pixbuf_get_width (pixbuf),
				     gdk_pixbuf_get_height (pixbuf));


	g_signal_connect (G_OBJECT (window), "delete_event",
			  G_CALLBACK (gtk_main_quit), NULL);

	g_signal_connect (G_OBJECT (window), "destroy",
			  G_CALLBACK (gtk_main_quit), NULL);

	g_signal_connect (G_OBJECT (window), "expose_event",
			  G_CALLBACK (expose_event), NULL);

	g_signal_connect (G_OBJECT (window), "key_press_event",
			  G_CALLBACK (key_press_event), NULL);


	gtk_widget_show (window);

	gc = gdk_gc_new (window->window);

	gtk_main ();
	return (0);
}
