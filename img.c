#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <sys/time.h>
#include <sys/inotify.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

char *filename;
int inotify_fd;

int auto_raise = 1;

void
usage (void)
{
	fprintf (stderr, "usage: img [-n] filename\n");
	exit (1);
}

GtkWidget *window;
GdkPixbuf *pixbuf;

gboolean
expose_event ()
{
	if (pixbuf) {
		gdk_draw_pixbuf (window->window, NULL, pixbuf,
				 0, 0, 0, 0,
				 -1, -1,
				 GDK_RGB_DITHER_NORMAL, 0, 0);
	}
	return (TRUE);
}

int
read_image (void)
{
	GdkPixbuf *new_pixbuf;

	new_pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
	if (new_pixbuf == NULL)
		return (-1);

	if (pixbuf)
		g_object_unref (pixbuf);

	pixbuf = new_pixbuf;

	gtk_window_resize (GTK_WINDOW (window), 
			   gdk_pixbuf_get_width (pixbuf),
			   gdk_pixbuf_get_height (pixbuf));

	gtk_widget_queue_draw (window);

	if (auto_raise && window->window)
		gdk_window_raise (GDK_WINDOW (window->window));
	
	return (0);
}

gboolean
key_press_event (GtkWidget *widget, GdkEventKey *ev, gpointer data)
{
	switch (ev->keyval) {
	case 'q': /* covers q or ALT-q */
	case 'c': /* covers CTL-c */
	case 'w': /* covers CTL-w */
	case GDK_Escape:
		gtk_main_quit ();
	}
	return (TRUE);
}

struct monitor_file {
	struct monitor_file *next;
	char *dirname;
	char *filename;
	void (*func)(struct monitor_file *);
};

struct monitor_file *monitor_files;

int inotify_fd = -1;

gboolean
inotify_handler (GIOChannel *source, GIOCondition condition, gpointer data)
{
	char buf[10000];
	struct inotify_event ev;
	ssize_t avail, offset;
	struct monitor_file *mp;

	while ((avail = read (inotify_fd, buf, sizeof buf)) > 0) {
		offset = 0;
		while (offset + sizeof ev <= avail) {
			memcpy (&ev, buf + offset, sizeof ev);
			filename = buf + offset + sizeof ev;

			if (ev.mask & (IN_CLOSE_WRITE | IN_MOVED_TO)) {
				for (mp = monitor_files; mp; mp = mp->next) {
					if (strcmp (mp->filename,
						    filename) == 0) {
						(*mp->func)(mp);
					}
				}
			}
			offset += sizeof ev + ev.len;
		}
	}

	return (TRUE);
}

struct monitor_file *
monitor_file (char *filename_arg,
	      void (*file_changed_func)(struct monitor_file *))
{
	char *dirname;
	int total_len, dirname_len, filename_len, idx;
	struct monitor_file *mp;
	void *mpbuf;

	if ((total_len = strlen (filename_arg)) == 0)
		return (NULL);

	idx = total_len;
	while (idx > 0 && filename_arg[idx-1] != '/')
		idx--;

	filename = filename_arg + idx;
	filename_len = total_len - idx;

	while (idx > 0 && filename_arg[idx-1] == '/')
		idx--;

	if (idx == 0) {
		dirname = (filename_arg[0] == '/') ? "/" : ".";
		dirname_len = 1;
	} else {
		dirname = filename_arg;
		dirname_len = idx;
	}
		
	if ((mpbuf = malloc (sizeof *mp
			     + filename_len + 1
			     + dirname_len + 1)) == NULL) {
		return (NULL);
	}

	mp = mpbuf;
	memset (mp, 0, sizeof *mp);

	mp->filename = mpbuf + sizeof *mp;
	strncpy (mp->filename, filename, filename_len);
	mp->filename[filename_len] = 0;

	mp->dirname = mp->filename + filename_len + 1;
	strncpy (mp->dirname, dirname, dirname_len);
	mp->dirname[dirname_len] = 0;
	
	if (inotify_add_watch (inotify_fd, mp->dirname, 
			       IN_CLOSE_WRITE | IN_MOVED_TO) < 0) {
		free (mp);
		return (NULL);
	}

	mp->func = file_changed_func;

	mp->next = monitor_files;
	monitor_files = mp;

	return (mp);
}

void
monitor_file_func (struct monitor_file *mp)
{
	read_image ();
}

int
main (int argc, char **argv)
{
	int c;

	gtk_init (&argc, &argv);

	while ((c = getopt (argc, argv, "n")) != EOF) {
		switch (c) {
		case 'n':
			auto_raise = 0;
			break;
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

	g_signal_connect (G_OBJECT (window), "delete_event",
			  G_CALLBACK (gtk_main_quit), NULL);

	g_signal_connect (G_OBJECT (window), "destroy",
			  G_CALLBACK (gtk_main_quit), NULL);

	g_signal_connect (G_OBJECT (window), "expose_event",
			  G_CALLBACK (expose_event), NULL);

	g_signal_connect (G_OBJECT (window), "key_press_event",
			  G_CALLBACK (key_press_event), NULL);

	gtk_widget_show (window);

	if ((inotify_fd = inotify_init1 (IN_NONBLOCK | IN_CLOEXEC)) < 0) {
		fprintf (stderr, "error doing inotify_init\n");
		exit (1);
	}

	g_io_add_watch (g_io_channel_unix_new (inotify_fd), G_IO_IN,
			inotify_handler, NULL);

	if (monitor_file (filename, monitor_file_func) == NULL) {
		fprintf (stderr, "error setting up monitor_file\n");
		exit (1);
	}

	if (read_image () < 0) {
		fprintf (stderr, "can't open %s\n", filename);
		exit (1);
	}

	gtk_main ();
	return (0);
}

