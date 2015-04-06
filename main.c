/* Generate thumbnails with the Freedesktop Thumbnailer D-Bus interface
 * using glib/GIO GDBusProxy and multiple threads
 * (c) 2015 Wolfgang Frisch <wfr@roembden.net>
 * 
 * https://wiki.gnome.org/DraftSpecs/ThumbnailerSpec
 * https://developer.gnome.org/gio/stable/GDBusProxy.html
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gio/gdbusproxy.h>

typedef struct
{
    GMainLoop *loop;
    guint32 handle;
    gboolean success;
    const char *error_message;
} ThumbnailerState;


void
thumbnailer_signal_cb (GDBusProxy *proxy,
                       gchar      *sender_name,
                       gchar      *signal_name,
                       GVariant   *parameters,
                       gpointer    thumbnailer_state)
{
  ThumbnailerState *state = (ThumbnailerState *) thumbnailer_state;
  guint32 signal_handle;
  const gchar **uris;

  printf("thumbnailer_signal_cb (..., %s, %s, ...), state->handle=%d\n",
          signal_name,
          g_variant_print(parameters, FALSE),
          state->handle);

  if (g_strcmp0 (signal_name, "Error") == 0)
    {
      // (uasis) = (handle, [uris], error_code, message)
      g_variant_get(parameters, "(uasis)", &signal_handle, NULL, NULL, &state->error_message);
      if (signal_handle == state->handle)
        {
          state->success = FALSE;
        }
    }
  else if (g_strcmp0 (signal_name, "Ready") == 0)
    {
      // https://wiki.gnome.org/DraftSpecs/ThumbnailerSpec#Thumbnails_are_ready_for_use
      g_variant_get(parameters, "(u^as)", &signal_handle, &uris);
      if (signal_handle == state->handle)
        {
          state->success = TRUE;
        }
    }
  else if (g_strcmp0 (signal_name, "Finished") == 0)
    {
      g_main_loop_quit(state->loop);
    }
}


gpointer
run_thumbnail_thread (gpointer data)
{
  gchar *uri;
  GDBusConnection *connection;
  GDBusProxy *proxy;
  ThumbnailerState state;
  GVariant *result = NULL;
  GError *error = NULL;
  
  gchar *filename = (gchar *) data;
  gchar *type = g_content_type_guess(filename, NULL, 0, NULL);
  gchar *mimetype = g_content_type_get_mime_type(type);
  g_free(type);
  printf("(%s) %s\n", mimetype, filename);
  
  GMainContext *thread_context = g_main_context_new ();
  state.loop = g_main_loop_new (thread_context, FALSE);
  g_main_context_push_thread_default (thread_context);

  connection = g_dbus_connection_new_for_address_sync (
      g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION, NULL, NULL),
      G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION | G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
      NULL,
      NULL,
      &error);
  g_assert_no_error (error);

  proxy = g_dbus_proxy_new_sync (connection,
                                 G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                 NULL,
                                 "org.freedesktop.thumbnails.Thumbnailer1",
                                 "/org/freedesktop/thumbnails/Thumbnailer1",
                                 "org.freedesktop.thumbnails.Thumbnailer1",
                                 NULL,   /* cancellable */
                                 &error);
  if(!proxy)
    {
      printf("ERROR in g_dbus_proxy_new_for_bus_sync()\n");
      return (gpointer) 1;
    }
  else
    {
      printf("D-Bus connected.\n");
    }
 
  printf ("Queueing thumbnail request ...\n");
  uri = g_strdup_printf("file://%s", filename);
  const gchar *uris[] = { uri, NULL };
  const gchar *mimetypes[] = { mimetype, NULL };
  g_signal_connect (G_OBJECT (proxy), "g-signal",
                    G_CALLBACK (thumbnailer_signal_cb), &state);
  result = g_dbus_proxy_call_sync (proxy,
                                    "Queue",
                                    g_variant_new("(^as^asssu)",
                                                  uris,
                                                  mimetypes,
                                                  "normal",
                                                  "default",
                                                  0),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    G_MAXINT,
                                    NULL,
                                    &error);
  g_free (uri);
  if(!result || error)
    {
      g_error ("ERROR in g_dbus_proxy_call_sync(): %s", error->message);
      return (gpointer) 1;
    }
  g_variant_get (result, "(u)", &(state.handle));
  g_variant_unref (result);

  g_main_loop_run (state.loop);
  // g_main_loop_quit happens in the D-Bus callback
  g_main_loop_unref (state.loop);
  g_object_unref (proxy);
  if(state.success)
    {
      printf ("Thumbnail generated!\n");
    }
  else
    {
      printf ("%s\n", state.error_message);
      return (gpointer) 1;
    }
  return (gpointer) 0;
}



int
main(int argc, char **argv)
{
  GThread **threads;

  if (argc < 3)
    {
      printf("usage: %s filenames\n", argv[0]);
      return 1;
    }

  threads = g_new(GThread*, argc);
  for (int argn = 1; argn < argc; argn++)
    {
      threads[argn] = g_thread_new("thumbnail-thread", run_thumbnail_thread, argv[argn]);
    }
  for (int argn = 1; argn < argc; argn++)
    {
      g_thread_join(threads[argn]);
      g_thread_unref(threads[argn]);
    }
  g_free(threads);

  return 0;
}


/* vim: set expandtab cindent shiftwidth=2 softtabstop=2 textwidth=79 fo-=ro fo+=cql tabstop=2 : */
