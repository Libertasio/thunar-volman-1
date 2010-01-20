/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2010 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of 
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write to the Free 
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include <gudev/gudev.h>

#include <gtk/gtk.h>

#include <xfconf/xfconf.h>

#include <thunar-volman/tvm-prompt.h>



static gchar *
tvm_run_resolve_mount_point (GUdevDevice *device)
{
  GVolumeMonitor *monitor;
  GMount         *mount;
  GFile          *file;
  GList          *volumes;
  GList          *lp;
  gchar          *unix_device;
  gchar          *mount_point = NULL;

  /* get the default GIO volume monitor */
  monitor = g_volume_monitor_get ();

  /* retrieve the list of available volumes from the monitor */
  volumes = g_volume_monitor_get_volumes (monitor);

  /* iterate over all volumes */
  for (lp = volumes; lp != NULL; lp = lp->next)
    {
      /* determine the device file corresponding to the volume */
      unix_device = g_volume_get_identifier (lp->data, 
                                             G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);

      /* match the device file of the volume against the device file of our device */
      if (g_strcmp0 (unix_device, g_udev_device_get_device_file (device)) == 0)
        {
          /* found the correct volume, now check if it is mounted */
          mount = g_volume_get_mount (lp->data);
          if (mount != NULL)
            {
              /* volume is mounted, determine mount point path */
              file = g_mount_get_root (mount);
              mount_point = g_file_get_path (file);

              /* clean up */
              g_object_unref (file);
              g_object_unref (mount);
            }
        }

      /* free the device file string */
      g_free (unix_device);

      /* release the volume */
      g_object_unref (lp->data);
    }

  /* destroy the volume list */
  g_list_free (volumes);

  /* we no longer need the monitor */
  g_object_unref (monitor);

  return mount_point;
}



gboolean 
tvm_run_burn_software (GUdevClient   *client,
                       GUdevDevice   *device,
                       XfconfChannel *channel,
                       GError       **error)
{
  static const gchar *cd_criteria[] = {
    "ID_CDROM_MEDIA_CD_R",
    "ID_CDROM_MEDIA_CD_RW",
  };
  
  static const gchar *dvd_criteria[] = {
    "ID_CDROM_MEDIA_DVD_R",
    "ID_CDROM_MEDIA_DVD_RW",
    "ID_CDROM_MEDIA_DVD_PLUS_R",
    "ID_CDROM_MEDIA_DVD_PLUS_RW",
  };
  gboolean            autoburn;
  gboolean            is_cd = FALSE;
  gboolean            is_dvd = FALSE;
  gboolean            result = FALSE;
  const gchar        *command_property;
  gchar              *command;
  guint               n;
  gint                response;

  g_return_val_if_fail (G_UDEV_IS_CLIENT (client), FALSE);
  g_return_val_if_fail (G_UDEV_IS_DEVICE (device), FALSE);
  g_return_val_if_fail (XFCONF_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* abort without error if autoburning is turned off */
  if (!xfconf_channel_get_bool (channel, "/autoburn/enabled", FALSE))
    return FALSE;

  /* check if the disc is a CD */
  for (n = 0; !is_cd && n < G_N_ELEMENTS (cd_criteria); ++n)
    if (g_udev_device_get_property_as_boolean (device, cd_criteria[n]))
      is_cd = TRUE;

  /* check if the disc is a DVD */
  for (n = 0; !is_cd && !is_dvd && n < G_N_ELEMENTS (dvd_criteria); ++n)
    if (g_udev_device_get_property_as_boolean (device, dvd_criteria[n]))
      is_dvd = TRUE;

  g_debug ("is_cd = %i, is_dvd = i", is_cd, is_dvd);

  if (is_dvd)
    {
      /* ask what to do with the empty DVD */
      response = tvm_prompt (client, device, "gnome-dev-disc-dvdr",
                             _("Blank DVD inserted"),
                             _("You have inserted a blank DVD."),
                             _("What would you like to do?"),
                             _("Ig_nore"), GTK_RESPONSE_CANCEL,
                             _("Burn _DVD"), TVM_RESPONSE_BURN_DATA_CD, 
                             NULL);
    }
  else
    {
      /* ask whether to burn data or audio CD */
      response = tvm_prompt (client, device, "gnome-dev-disc-cdr",
                             _("Blank CD inserted"),
                             _("You have inserted a blank CD."),
                             _("What would you like to do?"),
                             _("Ig_nore"), GTK_RESPONSE_CANCEL,
                             _("Burn _Data CD"), TVM_RESPONSE_BURN_DATA_CD,
                             _("Burn _Audio CD"), TVM_RESPONSE_BURN_AUDIO_CD,
                             NULL);
    }

  /* determine the autoburn command property */
  if (response == TVM_RESPONSE_BURN_DATA_CD)
    command_property = "/autoburn/data-cd-command";
  else if (response == TVM_RESPONSE_BURN_AUDIO_CD)
    command_property = "/autoburn/audio-cd-command";
  else 
    return TRUE;

  /* determine the command to launch */
  command = xfconf_channel_get_string (channel, command_property, NULL);

  /* only try to launch the command if it is set and non-empty */
  if (command != NULL && *command != '\0')
    {
      /* try to execute the preferred burn software */
      result = tvm_run_command (client, device, channel, command, error);
    }
  else
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("The burn command may not be empty"));
    }

  /* free the burn command */
  g_free (command);

  return result;
}



gboolean
tvm_run_cd_player (GUdevClient   *client,
                   GUdevDevice   *device,
                   XfconfChannel *channel,
                   GError       **error)
{
  gboolean result = FALSE;
  gchar   *command;

  g_return_val_if_fail (G_UDEV_IS_CLIENT (client), FALSE);
  g_return_val_if_fail (G_UDEV_IS_DEVICE (device), FALSE);
  g_return_val_if_fail (XFCONF_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* check whether autoplaying audio CDs is enabled */
  if (xfconf_channel_get_bool (channel, "/play-audio-cds/enabled", FALSE))
    {
      /* determine the audio CD player command */
      command = xfconf_channel_get_string (channel, "/play-audio-cds/command", NULL);

      /* check whether the command is set and non-empty */
      if (command != NULL && *command != '\0')
        {
          /* try to lanuch the audio CD player */
          result = tvm_run_command (client, device, channel, command, error);
        }
      else
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                       _("The CD player command may not be empty"));
        }
      
      /* free the command string */
      g_free (command);
    }

  return result;
}



gboolean
tvm_run_command (GUdevClient   *client,
                 GUdevDevice   *device,
                 XfconfChannel *channel,
                 const gchar   *command,
                 GError       **error)
{
  const gchar *p;
  gboolean     result;
  GString     *command_line;
  gchar      **argv;
  gchar       *device_file;
  gchar       *mount_point;
  gchar       *quoted;

  g_return_val_if_fail (G_UDEV_IS_CLIENT (client), FALSE);
  g_return_val_if_fail (G_UDEV_IS_DEVICE (device), FALSE);
  g_return_val_if_fail (XFCONF_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* perform the required substitutions */
  command_line = g_string_new (NULL);
  for (p = command; *p != '\0'; ++p)
    {
      /* check if we should substitute */
      if (G_UNLIKELY (p[0] == '%' && p[1] != '\0'))
        {
          /* check which substitution to perform */
          switch (*++p)
            {
            case 'd': /* device file */
              device_file = g_udev_device_get_device_file (device);
              if (G_LIKELY (device_file != NULL))
                g_string_append (command_line, device_file);
              break;

            case 'm': /* mount point */
              mount_point = tvm_run_resolve_mount_point (device);
              if (G_LIKELY (mount_point != NULL))
                {
                  /* substitute mount point quoted */
                  quoted = g_shell_quote (mount_point);
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              else
                {
                  /* %m must always be substituted */
                  g_string_append (command_line, "\"\"");
                }
              g_free (mount_point);
              break;
              
            case '%':
              g_string_append_c (command_line, '%');
              break;

            default:
              g_string_append_c (command_line, '%');
              g_string_append_c (command_line, *p);
              break;
            }
        }
      else
        {
          /* just append the character */
          g_string_append_c (command_line, *p);
        }
    }

  /* try to parse the command line */
  result = g_shell_parse_argv (command_line->str, NULL, &argv, error);
  if (G_LIKELY (result))
    {
      g_debug ("%s", command_line->str);

      /* try to spawn the command asynchronously in the users home directory */
      result = g_spawn_async (g_get_home_dir (), argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, error);

      /* cleanup */
      g_strfreev (argv);
    }

  /* cleanup */
  g_string_free (command_line, TRUE);

  return result;
}
