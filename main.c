/* Generate a thumbnail using the Freedesktop Thumbnailer D-Bus interface
 * using glib/GIO GDBusProxy
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
proxy_signal_cb (GDBusProxy *proxy,
                 gchar      *sender_name,
                 gchar      *signal_name,
                 GVariant   *parameters,
                 gpointer    thumbnailer_state)
{
  ThumbnailerState *state = (ThumbnailerState *) thumbnailer_state;
  guint32 signal_handle;
  const gchar **uris;
  const gchar *dump = g_variant_print(parameters, FALSE);
  printf("proxy_signal_cb(..., %s, %s, %s, ...)\n",
          sender_name,
          signal_name,
          dump);
  if (g_strcmp0 (signal_name, "Error") == 0)
    {
      // parameters contents: (uasis)
      // (handle, [uris], error_code, message)
      g_variant_get(parameters, "(uasis)", &signal_handle, NULL, NULL, &state->error_message);
      if (signal_handle == state->handle)
        {
          state->success = FALSE;
        }
    }
  else if (g_strcmp0 (signal_name, "Ready") == 0)
    {
      // parameters contents: (uas)
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

int
main(int argc, char **argv)
{
  const gchar *filename = NULL, *mimetype = NULL;
  gchar *uri;
  GDBusProxy *proxy;
  ThumbnailerState state;
  GVariant *result = NULL;
  GError *error = NULL;

  if (argc != 3)
    {
      printf("usage: %s filename mimetype\n", argv[0]);
      return 1;
    }
  filename = argv[1];
  mimetype = argv[2];


  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL,
                                         "org.freedesktop.thumbnails.Thumbnailer1",
                                         "/org/freedesktop/thumbnails/Thumbnailer1",
                                         "org.freedesktop.thumbnails.Thumbnailer1",
                                         NULL,   /* cancellable */
                                         &error);
  if(!proxy)
    {
      printf("ERROR in g_dbus_proxy_new_for_bus_sync()\n");
      return 1;
    }
  else
    {
      printf("D-Bus connected.\n");
    }
 
  printf("Queueing thumbnail request ...\n");
  uri = g_strdup_printf("file://%s", filename);
  const gchar *uris[] = { uri, NULL };
  const gchar *mimetypes[] = { mimetype, NULL };
  g_signal_connect (G_OBJECT (proxy), "g-signal",
                    G_CALLBACK (proxy_signal_cb), &state);
  state.loop = g_main_loop_new(NULL, FALSE);
  result = g_dbus_proxy_call_sync (proxy,
                                    "Queue",
                                    g_variant_new("(^as^asssu)",
                                                  uris,
                                                  mimetypes,
                                                  "normal",
                                                  "default",
                                                  0),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    G_MAXINT, // timeout
                                    NULL,
                                    &error);
  g_free(uri);
  if(!result || error)
    {
      printf("ERROR in g_dbus_proxy_call_sync(): %s\n", error->message);
      return 1;
    }
  g_variant_get(result, "(u)", &(state.handle));
  printf("state.handle=%d\n", state.handle);
  g_variant_unref(result);
  g_main_loop_run(state.loop);
  // g_main_loop_quit happens in the D-Bus callback
  g_main_loop_unref(state.loop);
  g_object_unref(proxy);
  if(state.success)
    {
      printf("Thumbnail generated!\n");
    }
  else
    {
      printf("%s\n", state.error_message);
      return 1;
    }

  return 0;
}


/* vim: set expandtab cindent cinoptions=>4,n-2,{2,^-2,:2,=2,g0,h2,p5,t0,+2,(0,u0,w1,m1 shiftwidth=2 softtabstop=2 textwidth=79 fo-=ro fo+=cql tabstop=2 : */
