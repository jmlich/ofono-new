/*
 *  oFono - Open Source Telephony - RIL-based devices
 *
 *  Copyright (C) 2015-2016 Jolla Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "ril_radio.h"
#include "ril_util.h"
#include "ril_log.h"

#include <grilio_queue.h>
#include <grilio_request.h>
#include <grilio_parser.h>

typedef GObjectClass RilRadioClass;
typedef struct ril_radio RilRadio;

/*
 * Object states:
 *
 * 1. Idle (!pending && !retry)
 * 2. Power on/off request pending (pending)
 * 3. Power on retry has been scheduled (retry)
 */
struct ril_radio_priv {
	GRilIoChannel *io;
	GRilIoQueue *q;
	gulong state_event_id;
	char *log_prefix;
	GHashTable *req_table;
	guint pending_id;
	guint retry_id;
	guint state_changed_while_request_pending;
	enum ril_radio_state last_known_state;
	gboolean power_cycle;
	gboolean next_state_valid;
	gboolean next_state;
};

enum ril_radio_signal {
	SIGNAL_STATE_CHANGED,
	SIGNAL_COUNT
};

#define POWER_RETRY_SECS (1)

#define SIGNAL_STATE_CHANGED_NAME       "ril-radio-state-changed"

static guint ril_radio_signals[SIGNAL_COUNT] = { 0 };

G_DEFINE_TYPE(RilRadio, ril_radio, G_TYPE_OBJECT)
#define RIL_RADIO_TYPE (ril_radio_get_type())
#define RIL_RADIO(obj) (G_TYPE_CHECK_INSTANCE_CAST(obj,RIL_RADIO_TYPE,RilRadio))

static void ril_radio_submit_power_request(struct ril_radio *self, gboolean on);

G_INLINE_FUNC gboolean ril_radio_power_should_be_on(struct ril_radio *self)
{
	struct ril_radio_priv *priv = self->priv;

	return g_hash_table_size(priv->req_table) && !priv->power_cycle;
}

G_INLINE_FUNC gboolean ril_radio_state_off(enum ril_radio_state radio_state)
{
	return radio_state == RADIO_STATE_OFF;
}

G_INLINE_FUNC gboolean ril_radio_state_on(enum ril_radio_state radio_state)
{
	return !ril_radio_state_off(radio_state);
}

static gboolean ril_radio_power_request_retry_cb(gpointer user_data)
{
	struct ril_radio *self = user_data;
	struct ril_radio_priv *priv = self->priv;

	DBG("%s", priv->log_prefix);
	GASSERT(priv->retry_id);
	priv->retry_id = 0;
	ril_radio_submit_power_request(self, ril_radio_power_should_be_on(self));
	return FALSE;
}

static void ril_radio_cancel_retry(struct ril_radio *self)
{
	struct ril_radio_priv *priv = self->priv;

	if (priv->retry_id) {
		DBG("%sretry cancelled", priv->log_prefix);
		g_source_remove(priv->retry_id);
		priv->retry_id = 0;
	}
}

static void ril_radio_check_state(struct ril_radio *self)
{
	struct ril_radio_priv *priv = self->priv;

	if (!priv->pending_id) {
		const gboolean should_be_on = ril_radio_power_should_be_on(self);

		if (ril_radio_state_on(self->priv->last_known_state) ==
								should_be_on) {
			/* All is good, cancel pending retry if there is one */
			ril_radio_cancel_retry(self);
		} else if (priv->state_changed_while_request_pending) {
			/* Hmm... RIL's reaction was inadequate, repeat */
			ril_radio_submit_power_request(self, should_be_on);
		} else if (!priv->retry_id) {
			/* There has been no reaction so far, wait a bit */
			DBG("%sretry scheduled", priv->log_prefix);
			priv->retry_id = g_timeout_add_seconds(POWER_RETRY_SECS,
					ril_radio_power_request_retry_cb, self);
		}
	}

	/* Don't update public state while something is pending */
	if (!priv->pending_id && !priv->retry_id &&
				self->state != priv->last_known_state) {
		DBG("%s%s -> %s", priv->log_prefix,
			ril_radio_state_to_string(self->state),
			ril_radio_state_to_string(priv->last_known_state));
		self->state = priv->last_known_state;
		g_signal_emit(self, ril_radio_signals[SIGNAL_STATE_CHANGED], 0);
	}
}

static void ril_radio_power_request_cb(GRilIoChannel *channel, int ril_status,
				const void *data, guint len, void *user_data)
{
	struct ril_radio *self = user_data;
	struct ril_radio_priv *priv = self->priv;

	GASSERT(priv->pending_id);
	priv->pending_id = 0;

	if (ril_status != RIL_E_SUCCESS) {
		ofono_error("Power request failed: %s",
					ril_error_to_string(ril_status));
	}

	if (priv->next_state_valid) {
		ril_radio_submit_power_request(self, priv->next_state);
	} else {
		ril_radio_check_state(self);
	}
}

static void ril_radio_submit_power_request(struct ril_radio *self, gboolean on)
{
	struct ril_radio_priv *priv = self->priv;
	GRilIoRequest *req = grilio_request_sized_new(8);

	grilio_request_append_int32(req, 1);
	grilio_request_append_int32(req, on); /* Radio ON=1, OFF=0 */

	priv->next_state_valid = FALSE;
	priv->next_state = on;
	priv->state_changed_while_request_pending = 0;
	ril_radio_cancel_retry(self);

	GASSERT(!priv->pending_id);
	priv->pending_id = grilio_queue_send_request_full(priv->q, req,
		RIL_REQUEST_RADIO_POWER, ril_radio_power_request_cb, NULL, self);
	grilio_request_unref(req);
}

static void ril_radio_power_request(struct ril_radio *self, gboolean on,
							gboolean allow_repeat)
{
	struct ril_radio_priv *priv = self->priv;
	const char *on_off = on ? "on" : "off";

	if (priv->pending_id) {
		if (allow_repeat || priv->next_state != on) {
			/* Wait for the pending request to complete */
			priv->next_state_valid = TRUE;
			priv->next_state = on;
			DBG("%s%s (queued)", priv->log_prefix, on_off);
		} else {
			DBG("%s%s (ignored)", priv->log_prefix, on_off);
		}
	} else {
		DBG("%s%s", priv->log_prefix, on_off);
		ril_radio_submit_power_request(self, on);
	}
}

void ril_radio_confirm_power_on(struct ril_radio *self)
{
	if (G_LIKELY(self) && ril_radio_power_should_be_on(self)) {
		ril_radio_power_request(self, TRUE, TRUE);
	}
}

void ril_radio_power_cycle(struct ril_radio *self)
{
	if (G_LIKELY(self)) {
		struct ril_radio_priv *priv = self->priv;

		if (ril_radio_state_off(priv->last_known_state)) {
			DBG("%spower is already off", priv->log_prefix);
			GASSERT(!priv->power_cycle);
		} else if (priv->power_cycle) {
			DBG("%salready in progress", priv->log_prefix);
		} else {
			DBG("%sinitiated", priv->log_prefix);
			priv->power_cycle = TRUE;
			if (!priv->pending_id) {
				ril_radio_submit_power_request(self, FALSE);
			}
		}
	}
}

void ril_radio_power_on(struct ril_radio *self, gpointer tag)
{
	if (G_LIKELY(self)) {
		struct ril_radio_priv *priv = self->priv;

		if (!g_hash_table_contains(priv->req_table, tag)) {
			gboolean was_on = ril_radio_power_should_be_on(self);

			DBG("%s%p", priv->log_prefix, tag);
			g_hash_table_insert(priv->req_table, tag, tag);
			if (!was_on) {
				ril_radio_power_request(self, TRUE, FALSE);
			}
		}
	}
}

void ril_radio_power_off(struct ril_radio *self, gpointer tag)
{
	if (G_LIKELY(self)) {
		struct ril_radio_priv *priv = self->priv;

		if (g_hash_table_remove(priv->req_table, tag)) {
			DBG("%s%p", priv->log_prefix, tag);
			if (!ril_radio_power_should_be_on(self)) {
				/* The last one turns the lights off */
				ril_radio_power_request(self, FALSE, FALSE);
			}
		}
	}
}

gulong ril_radio_add_state_changed_handler(struct ril_radio *self,
					ril_radio_cb_t cb, void *arg)
{
	return (G_LIKELY(self) && G_LIKELY(cb)) ? g_signal_connect(self,
		SIGNAL_STATE_CHANGED_NAME, G_CALLBACK(cb), arg) : 0;
}

void ril_radio_remove_handler(struct ril_radio *self, gulong id)
{
	if (G_LIKELY(self) && G_LIKELY(id)) {
		g_signal_handler_disconnect(self, id);
	}
}

enum ril_radio_state ril_radio_state_parse(const void *data, guint len)
{
	GRilIoParser rilp;
	int radio_state;

	grilio_parser_init(&rilp, data, len);
	if (grilio_parser_get_int32(&rilp, &radio_state)) {
		return radio_state;
	} else {
		ofono_error("Error parsing radio state");
		return RADIO_STATE_UNAVAILABLE;
	}
}

static void ril_radio_state_changed(GRilIoChannel *io, guint code,
				const void *data, guint len, void *user_data)
{
	struct ril_radio *self = user_data;
	enum ril_radio_state radio_state = ril_radio_state_parse(data, len);

	GASSERT(code == RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED);
	if (radio_state != RADIO_STATE_UNAVAILABLE) {
		struct ril_radio_priv *priv = self->priv;

		DBG("%s%s", priv->log_prefix,
			ril_radio_state_to_string(radio_state));
		GASSERT(!priv->pending_id || !priv->retry_id);

		if (priv->power_cycle && ril_radio_state_off(radio_state)) {
			DBG("%sswitched off for power cycle", priv->log_prefix);
			priv->power_cycle = FALSE;
		}

		if (priv->pending_id) {
			priv->state_changed_while_request_pending++;
		}

		priv->last_known_state = radio_state;
		ril_radio_check_state(self);
	}
}

struct ril_radio *ril_radio_new(GRilIoChannel *io)
{
	struct ril_radio *self = g_object_new(RIL_RADIO_TYPE, NULL);
	struct ril_radio_priv *priv = self->priv;

	priv->io = grilio_channel_ref(io);
	priv->q = grilio_queue_new(priv->io);
	priv->log_prefix =
		(io && io->name && io->name[0] && strcmp(io->name, "RIL")) ?
		g_strconcat(io->name, " ", NULL) : g_strdup("");
	DBG("%s", priv->log_prefix);
	priv->state_event_id = grilio_channel_add_unsol_event_handler(priv->io,
				ril_radio_state_changed,
				RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, self);
	return self;
}

struct ril_radio *ril_radio_ref(struct ril_radio *self)
{
	if (G_LIKELY(self)) {
		g_object_ref(RIL_RADIO(self));
		return self;
	} else {
		return NULL;
	}
}

void ril_radio_unref(struct ril_radio *self)
{
	if (G_LIKELY(self)) {
		g_object_unref(RIL_RADIO(self));
	}
}

static void ril_radio_init(struct ril_radio *self)
{
	struct ril_radio_priv *priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
					RIL_RADIO_TYPE, struct ril_radio_priv);
	self->priv = priv;
	priv->req_table = g_hash_table_new_full(g_direct_hash, g_direct_equal,
								NULL, NULL);
}

static void ril_radio_dispose(GObject *object)
{
	struct ril_radio *self = RIL_RADIO(object);
	struct ril_radio_priv *priv = self->priv;

	if (priv->state_event_id) {
		grilio_channel_remove_handler(priv->io, priv->state_event_id);
		priv->state_event_id = 0;
	}
	if (priv->pending_id) {
		grilio_queue_cancel_request(priv->q, priv->pending_id, FALSE);
		priv->pending_id = 0;
	}
	priv->next_state_valid = FALSE;
	ril_radio_cancel_retry(self);
	grilio_queue_cancel_all(priv->q, FALSE);
	G_OBJECT_CLASS(ril_radio_parent_class)->dispose(object);
}

static void ril_radio_finalize(GObject *object)
{
	struct ril_radio *self = RIL_RADIO(object);
	struct ril_radio_priv *priv = self->priv;

	DBG("%s", priv->log_prefix);
	g_free(priv->log_prefix);
	grilio_channel_unref(priv->io);
	grilio_queue_unref(priv->q);
	g_hash_table_unref(priv->req_table);
	G_OBJECT_CLASS(ril_radio_parent_class)->finalize(object);
}

static void ril_radio_class_init(RilRadioClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->dispose = ril_radio_dispose;
	object_class->finalize = ril_radio_finalize;
	g_type_class_add_private(klass, sizeof(struct ril_radio_priv));
	ril_radio_signals[SIGNAL_STATE_CHANGED] =
		g_signal_new(SIGNAL_STATE_CHANGED_NAME,
			G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_FIRST,
			0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
