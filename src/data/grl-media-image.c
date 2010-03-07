/*
 * Copyright (C) 2010 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Juan A. Suarez Romero <jasuarez@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/*
 * A multimedia data for image.
 *
 * This high level class represents an image multimedia item. It has methods to
 * set and get properties like framerate, width, height, and so on.
 *
 */

#include "grl-media-image.h"


static void grl_media_image_dispose (GObject *object);
static void grl_media_image_finalize (GObject *object);

G_DEFINE_TYPE (GrlMediaImage, grl_media_image, GRL_TYPE_MEDIA);

static void
grl_media_image_class_init (GrlMediaImageClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;

  gobject_class->dispose = grl_media_image_dispose;
  gobject_class->finalize = grl_media_image_finalize;
}

static void
grl_media_image_init (GrlMediaImage *self)
{
}

static void
grl_media_image_dispose (GObject *object)
{
  G_OBJECT_CLASS (grl_media_image_parent_class)->dispose (object);
}

static void
grl_media_image_finalize (GObject *object)
{
  g_signal_handlers_destroy (object);
  G_OBJECT_CLASS (grl_media_image_parent_class)->finalize (object);
}

/**
 * grl_media_image_new:
 *
 * Creates a new data image object.
 *
 * Returns: a newly-allocated data image.
 **/
GrlMedia *
grl_media_image_new (void)
{
  return GRL_MEDIA (g_object_new (GRL_TYPE_MEDIA_IMAGE,
                                  NULL));
}

void
grl_media_image_set_size (GrlMediaImage *image,
                          gint width,
                          int height)
{
  grl_media_image_set_width (image, width);
  grl_media_image_set_height (image, height);
}