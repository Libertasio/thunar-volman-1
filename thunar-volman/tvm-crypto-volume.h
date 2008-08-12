/* $Id$ */
/*-
 * Copyright (c) 2008 Benedikt Meurer <benny@xfce.org>.
 * Copyright (c) 2008 Colin Leroy <colin@colino.net>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __TVM_CRYPTO_VOLUME_H__
#define __TVM_CRYPTO_VOLUME_H__

#include <thunar-volman/tvm-device.h>

G_BEGIN_DECLS

gboolean tvm_crypto_volume_setup (TvmPreferences *preferences,
                                  LibHalContext  *context,
                                  const gchar    *udi,
                                  GError        **error) G_GNUC_INTERNAL;

G_END_DECLS

#endif /* !__TVM_CRYPTO_VOLUME_H__ */
