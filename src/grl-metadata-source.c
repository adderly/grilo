/*
 * Copyright (C) 2010 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
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

#include "grl-metadata-source.h"
#include "grl-metadata-source-priv.h"
#include "grl-plugin-registry.h"
#include "content/grl-content-media.h"

#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "grl-metadata-source"

#define GRL_METADATA_SOURCE_GET_PRIVATE(object)                 \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                        \
                               GRL_TYPE_METADATA_SOURCE,        \
                               GrlMetadataSourcePrivate))

enum {
  PROP_0,
  PROP_ID,
  PROP_NAME,
  PROP_DESC,
};

struct _GrlMetadataSourcePrivate {
  gchar *id;
  gchar *name;
  gchar *desc;
};

struct ResolveRelayCb {
  GrlMetadataSourceResolveCb user_callback;
  gpointer user_data;
  GrlMetadataSourceResolveSpec *spec;
};

static void grl_metadata_source_finalize (GObject *plugin);
static void grl_metadata_source_get_property (GObject *plugin,
                                              guint prop_id,
                                              GValue *value,
                                              GParamSpec *pspec);
static void grl_metadata_source_set_property (GObject *object,
                                              guint prop_id,
                                              const GValue *value,
                                              GParamSpec *pspec);

static GrlSupportedOps grl_metadata_source_supported_operations_impl (GrlMetadataSource *source);

/* ================ GrlMetadataSource GObject ================ */

G_DEFINE_ABSTRACT_TYPE (GrlMetadataSource,
                        grl_metadata_source,
                        GRL_TYPE_MEDIA_PLUGIN);

static void
grl_metadata_source_class_init (GrlMetadataSourceClass *metadata_source_class)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (metadata_source_class);

  gobject_class->finalize = grl_metadata_source_finalize;
  gobject_class->set_property = grl_metadata_source_set_property;
  gobject_class->get_property = grl_metadata_source_get_property;

  metadata_source_class->supported_operations =
    grl_metadata_source_supported_operations_impl;

  /**
   * GrlMetadataSource:source-id
   *
   * The identifier of the source.
   */
  g_object_class_install_property (gobject_class,
				   PROP_ID,
				   g_param_spec_string ("source-id",
							"Source identifier",
							"The identifier of the source",
							"",
							G_PARAM_READWRITE |
							G_PARAM_CONSTRUCT |
							G_PARAM_STATIC_STRINGS));
  /**
   * GrlMetadataSource:source-name
   *
   * The name of the source.
   */
  g_object_class_install_property (gobject_class,
				   PROP_NAME,
				   g_param_spec_string ("source-name",
							"Source name",
							"The name of the source",
							"",
							G_PARAM_READWRITE |
							G_PARAM_CONSTRUCT |
							G_PARAM_STATIC_STRINGS));
  /**
   * GrlMetadataSource:source-desc
   *
   * A description of the source
   */
  g_object_class_install_property (gobject_class,
				   PROP_DESC,
				   g_param_spec_string ("source-desc",
							"Source description",
							"A description of the source",
							"",
							G_PARAM_READWRITE |
							G_PARAM_CONSTRUCT |
							G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (metadata_source_class,
                            sizeof (GrlMetadataSourcePrivate));
}

static void
grl_metadata_source_init (GrlMetadataSource *source)
{
  source->priv = GRL_METADATA_SOURCE_GET_PRIVATE (source);
  memset (source->priv, 0, sizeof (GrlMetadataSourcePrivate));
}

static void
grl_metadata_source_finalize (GObject *object)
{
  GrlMetadataSource *source;

  g_debug ("grl_metadata_source_finalize");

  source = GRL_METADATA_SOURCE (object);

  g_free (source->priv->id);
  g_free (source->priv->name);
  g_free (source->priv->desc);

  G_OBJECT_CLASS (grl_metadata_source_parent_class)->finalize (object);
}

static void
set_string_property (gchar **property, const GValue *value)
{
  if (*property) {
    g_free (*property);
  }
  *property = g_value_dup_string (value);
}

static void
grl_metadata_source_set_property (GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
  GrlMetadataSource *source;

  source = GRL_METADATA_SOURCE (object);

  switch (prop_id) {
  case PROP_ID:
    set_string_property (&source->priv->id, value);
    break;
  case PROP_NAME:
    set_string_property (&source->priv->name, value);
    break;
  case PROP_DESC:
    set_string_property (&source->priv->desc, value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (source, prop_id, pspec);
    break;
  }
}

static void
grl_metadata_source_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
  GrlMetadataSource *source;

  source = GRL_METADATA_SOURCE (object);

  switch (prop_id) {
  case PROP_ID:
    g_value_set_string (value, source->priv->id);
    break;
  case PROP_NAME:
    g_value_set_string (value, source->priv->name);
    break;
  case PROP_DESC:
    g_value_set_string (value, source->priv->desc);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (source, prop_id, pspec);
    break;
  }
}

/* ================ Utilities ================ */

static void __attribute__ ((unused))
print_keys (gchar *label, const GList *keys)
{
  g_print ("%s: [", label);
  while (keys) {
    g_print (" %" GRL_KEYID_FORMAT, POINTER_TO_GRLKEYID (keys->data));
    keys = g_list_next (keys);
  }
  g_print (" ]\n");
}

static void
resolve_result_relay_cb (GrlMetadataSource *source,
			 GrlContentMedia *media,
			 gpointer user_data,
			 const GError *error)
{
  g_debug ("resolve_result_relay_cb");

  struct ResolveRelayCb *rrc;

  rrc = (struct ResolveRelayCb *) user_data;
  rrc->user_callback (source, media, rrc->user_data, error);

  g_object_unref (rrc->spec->source);
  g_object_unref (rrc->spec->media);
  g_list_free (rrc->spec->keys);
  g_free (rrc->spec);
  g_free (rrc);
}

static gboolean
resolve_idle (gpointer user_data)
{
  g_debug ("resolve_idle");
  GrlMetadataSourceResolveSpec *rs =
    (GrlMetadataSourceResolveSpec *) user_data;
  GRL_METADATA_SOURCE_GET_CLASS (rs->source)->resolve (rs->source, rs);
  return FALSE;
}

/* ================ API ================ */

const GList *
grl_metadata_source_supported_keys (GrlMetadataSource *source)
{
  g_return_val_if_fail (GRL_IS_METADATA_SOURCE (source), NULL);
  return GRL_METADATA_SOURCE_GET_CLASS (source)->supported_keys (source);
}

const GList *
grl_metadata_source_slow_keys (GrlMetadataSource *source)
{
  g_return_val_if_fail (GRL_IS_METADATA_SOURCE (source), NULL);
  if (GRL_METADATA_SOURCE_GET_CLASS (source)->slow_keys) {
    return GRL_METADATA_SOURCE_GET_CLASS (source)->slow_keys (source);
  } else {
    return NULL;
  }
}

const GList *
grl_metadata_source_key_depends (GrlMetadataSource *source, GrlKeyID key_id)
{
  g_return_val_if_fail (GRL_IS_METADATA_SOURCE (source), NULL);
  return GRL_METADATA_SOURCE_GET_CLASS (source)->key_depends (source, key_id);
}

void
grl_metadata_source_resolve (GrlMetadataSource *source,
                             const GList *keys,
                             GrlContentMedia *media,
                             guint flags,
                             GrlMetadataSourceResolveCb callback,
                             gpointer user_data)
{
  GrlMetadataSourceResolveSpec *rs;
  GList *_keys;
  struct ResolveRelayCb *rrc;

  g_debug ("grl_metadata_source_resolve");

  g_return_if_fail (GRL_IS_METADATA_SOURCE (source));
  g_return_if_fail (callback != NULL);
  g_return_if_fail (media != NULL);
  g_return_if_fail (grl_metadata_source_supported_operations (source) &
		    GRL_OP_RESOLVE);

  _keys = g_list_copy ((GList *) keys);

  if (flags & GRL_RESOLVE_FAST_ONLY) {
    grl_metadata_source_filter_slow (source, &_keys, FALSE);
  }

  /* Always hook an own relay callback so we can do some
     post-processing before handing out the results
     to the user */
  rrc = g_new0 (struct ResolveRelayCb, 1);
  rrc->user_callback = callback;
  rrc->user_data = user_data;

  rs = g_new0 (GrlMetadataSourceResolveSpec, 1);
  rs->source = g_object_ref (source);
  rs->keys = _keys;
  rs->media = g_object_ref (media);
  rs->flags = flags;
  rs->callback = resolve_result_relay_cb;
  rs->user_data = rrc;

  /* Save a reference to the operaton spec in the relay-cb's
     user_data so that we can free the spec there */
  rrc->spec = rs;

  g_idle_add (resolve_idle, rs);
}

GList *
grl_metadata_source_filter_supported (GrlMetadataSource *source,
                                      GList **keys,
                                      gboolean return_filtered)
{
  const GList *supported_keys;
  GList *iter_supported;
  GList *iter_keys;
  GrlKeyID key;
  GList *filtered_keys = NULL;
  gboolean got_match;
  GList *iter_keys_prev;
  GrlKeyID supported_key;

  g_return_val_if_fail (GRL_IS_METADATA_SOURCE (source), NULL);

  supported_keys = grl_metadata_source_supported_keys (source);

  iter_keys = *keys;
  while (iter_keys) {
    got_match = FALSE;
    iter_supported = (GList *) supported_keys;

    key = POINTER_TO_GRLKEYID (iter_keys->data);
    while (!got_match && iter_supported) {
      supported_key = POINTER_TO_GRLKEYID (iter_supported->data);
      if (key == supported_key) {
	got_match = TRUE;
      }
      iter_supported = g_list_next (iter_supported);
    }

    iter_keys_prev = iter_keys;
    iter_keys = g_list_next (iter_keys);

    if (got_match) {
      if (return_filtered) {
	filtered_keys = g_list_prepend (filtered_keys,
                                        GRLKEYID_TO_POINTER (key));
      }
      *keys = g_list_delete_link (*keys, iter_keys_prev);
      got_match = FALSE;
    }
  }

  return filtered_keys;
}

GList *
grl_metadata_source_filter_slow (GrlMetadataSource *source,
                                 GList **keys,
                                 gboolean return_filtered)
{
  const GList *slow_keys;
  GList *iter_slow;
  GList *iter_keys;
  GrlKeyID key;
  GList *filtered_keys = NULL;
  gboolean got_match;
  GrlKeyID slow_key;

  g_return_val_if_fail (GRL_IS_METADATA_SOURCE (source), NULL);

  slow_keys = grl_metadata_source_slow_keys (source);
  if (!slow_keys) {
    if (return_filtered) {
      return g_list_copy (*keys);
    } else {
      return NULL;
    }
  }

  iter_slow = (GList *) slow_keys;
  while (iter_slow) {
    got_match = FALSE;
    iter_keys = *keys;

    slow_key = POINTER_TO_GRLKEYID (iter_slow->data);
    while (!got_match && iter_keys) {
      key = POINTER_TO_GRLKEYID (iter_keys->data);
      if (key == slow_key) {
	got_match = TRUE;
      } else {
	iter_keys = g_list_next (iter_keys);
      }
    }

    iter_slow = g_list_next (iter_slow);

    if (got_match) {
      if (return_filtered) {
	filtered_keys =
	  g_list_prepend (filtered_keys, GRLKEYID_TO_POINTER (slow_key));
      }
      *keys = g_list_delete_link (*keys, iter_keys);
      got_match = FALSE;
    }
  }

  return filtered_keys;
}

void
grl_metadata_source_setup_full_resolution_mode (GrlMetadataSource *source,
                                                const GList *keys,
                                                struct SourceKeyMapList *key_mapping)
{
  key_mapping->source_maps = NULL;
  key_mapping->operation_keys = NULL;

  /* key_list holds keys to be resolved */
  GList *key_list = g_list_copy ((GList *) keys);

  /* Filter keys supported by this source */
  key_mapping->operation_keys =
    grl_metadata_source_filter_supported (GRL_METADATA_SOURCE (source),
                                          &key_list, TRUE);

  if (key_list == NULL) {
    g_debug ("Source supports all requested keys");
    goto done;
  }

  /*
   * 1) Find which sources (other than the current one) can resolve
   *    some of the missing keys
   * 2) Check out dependencies for the keys they can resolve
   * 3) For each dependency list check if the original source can resolve them.
   *    3.1) Yes: Add key and dependencies to be resolved
   *    3.2) No: forget about that key and its dependencies
   *         Ideally, we would check if other sources can resolve them
   * 4) Execute the user operation passing in our own callback
   * 5) For each result, check the sources that can resolve
   *    the missing metadata and issue resolution operations on them.
   *    We could do this once per source passing in a list with all the
   *    browse results. Problem is we lose response time although we gain
   *    overall efficiency.
   * 6) Invoke user callback with results
   */

  /* Find which sources resolve which keys */
  GList *supported_keys;
  GrlMetadataSource *_source;
  GrlMediaPlugin **source_list;
  GList *iter;
  GrlPluginRegistry *registry;

  registry = grl_plugin_registry_get_instance ();
  source_list = grl_plugin_registry_get_sources (registry);

  while (*source_list && key_list) {
    gchar *name;

    _source = GRL_METADATA_SOURCE (*source_list);

    source_list++;

    /* Interested in sources other than this  */
    if (_source == GRL_METADATA_SOURCE (source)) {
      continue;
    }

    /* Interested in sources capable of resolving metadata
       based on other metadata */
    GrlMetadataSourceClass *_source_class =
      GRL_METADATA_SOURCE_GET_CLASS (_source);
    if (!_source_class->resolve) {
      continue;
    }

    /* Check if this source supports some of the missing keys */
    g_object_get (_source, "source-name", &name, NULL);
    g_debug ("Checking resolution capabilities for source '%s'", name);
    supported_keys = grl_metadata_source_filter_supported (_source,
                                                           &key_list, TRUE);

    if (!supported_keys) {
      g_debug ("  Source does not support any of the keys, skipping.");
      continue;
    }

    g_debug ("  '%s' can resolve some keys, checking deps", name);

    /* Check the external dependencies for these supported keys */
    GList *supported_deps;
    GList *iter_prev;
    iter = supported_keys;
    while (iter) {
      GrlKeyID key = POINTER_TO_GRLKEYID (iter->data);
      GList *deps =
	g_list_copy ((GList *) grl_metadata_source_key_depends (_source, key));

      iter_prev = iter;
      iter = g_list_next (iter);

      /* deps == NULL means the key cannot be resolved
	 by using only metadata */
      if (!deps) {
	g_debug ("    Key '%u' cannot be resolved from metadata", key);
	supported_keys = g_list_delete_link (supported_keys, iter_prev);
	key_list = g_list_prepend (key_list, GRLKEYID_TO_POINTER (key));
	continue;
      }
      g_debug ("    Key '%u' might be resolved using external metadata", key);

      /* Check if the original source can solve these dependencies */
      supported_deps =
	grl_metadata_source_filter_supported (GRL_METADATA_SOURCE (source),
                                              &deps, TRUE);
      if (deps) {
	g_debug ("      Dependencies not supported by source, dropping key");
	/* Maybe some other source can still resolve it */
	/* TODO: maybe some of the sources already inspected could provide
	   these keys! */
	supported_keys =
	  g_list_delete_link (supported_keys, iter_prev);
	/* Put the key back in the list, maybe some other soure can
	   resolve it */
	key_list = g_list_prepend (key_list, GRLKEYID_TO_POINTER (key));
      } else {
	g_debug ("      Dependencies supported by source, including key");
	/* Add these dependencies to the list of keys for
	   the browse operation */
	/* TODO: maybe some of these keys are in the list already! */
	key_mapping->operation_keys =
	  g_list_concat (key_mapping->operation_keys, supported_deps);
      }
    }

    /* Save the key map for this source */
    if (supported_keys) {
      g_debug ("  Adding source '%s' to the resolution map", name);
      struct SourceKeyMap *source_key_map = g_new (struct SourceKeyMap, 1);
      source_key_map->source = g_object_ref (_source);
      source_key_map->keys = supported_keys;
      key_mapping->source_maps =
	g_list_prepend (key_mapping->source_maps, source_key_map);
    }
  }

  if (key_mapping->source_maps == NULL) {
    g_debug ("No key mapping for other sources, can't resolve more metadata");
  }
 done:
  return;
}

const gchar *
grl_metadata_source_get_id (GrlMetadataSource *source)
{
  g_return_val_if_fail (GRL_IS_METADATA_SOURCE (source), NULL);

  return source->priv->id;
}

const gchar *
grl_metadata_source_get_name (GrlMetadataSource *source)
{
  g_return_val_if_fail (GRL_IS_METADATA_SOURCE (source), NULL);

  return source->priv->name;
}

const gchar *
grl_metadata_source_get_description (GrlMetadataSource *source)
{
  g_return_val_if_fail (GRL_IS_METADATA_SOURCE (source), NULL);

  return source->priv->desc;
}

GrlSupportedOps
grl_metadata_source_supported_operations (GrlMetadataSource *source)
{
  g_return_val_if_fail (GRL_IS_METADATA_SOURCE (source), GRL_OP_NONE);
  return GRL_METADATA_SOURCE_GET_CLASS (source)->supported_operations (source);
}

static GrlSupportedOps
grl_metadata_source_supported_operations_impl (GrlMetadataSource *source)
{
  GrlSupportedOps caps = GRL_OP_NONE;
  GrlMetadataSourceClass *metadata_source_class;
  metadata_source_class = GRL_METADATA_SOURCE_GET_CLASS (source);
  if (metadata_source_class->resolve)
    caps |= GRL_OP_RESOLVE;
  return caps;
}
