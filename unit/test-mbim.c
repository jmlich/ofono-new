/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2017  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/uio.h>
#include <linux/types.h>
#include <assert.h>

#include <ell/ell.h>

#include "drivers/mbimmodem/mbim.h"
#include "drivers/mbimmodem/mbim-message.h"
#include "drivers/mbimmodem/mbim-private.h"

struct message_data {
	uint32_t tid;
	const unsigned char *binary;
	size_t binary_len;
};

static const unsigned char message_binary_device_caps[] = {
	0x03, 0x00, 0x00, 0x80, 0x08, 0x01, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA2, 0x89, 0xCC, 0x33,
	0xBC, 0xBB, 0x8B, 0x4F, 0xB6, 0xB0, 0x13, 0x3E, 0xC2, 0xAA, 0xE6, 0xDF,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD8, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00,
	0x60, 0x00, 0x00, 0x00, 0x3A, 0x00, 0x00, 0x00, 0x9C, 0x00, 0x00, 0x00,
	0x3C, 0x00, 0x00, 0x00, 0x33, 0x00, 0x35, 0x00, 0x39, 0x00, 0x33, 0x00,
	0x33, 0x00, 0x36, 0x00, 0x30, 0x00, 0x35, 0x00, 0x30, 0x00, 0x30, 0x00,
	0x31, 0x00, 0x38, 0x00, 0x37, 0x00, 0x31, 0x00, 0x37, 0x00, 0x00, 0x00,
	0x46, 0x00, 0x49, 0x00, 0x48, 0x00, 0x37, 0x00, 0x31, 0x00, 0x36, 0x00,
	0x30, 0x00, 0x5F, 0x00, 0x56, 0x00, 0x31, 0x00, 0x2E, 0x00, 0x31, 0x00,
	0x5F, 0x00, 0x4D, 0x00, 0x4F, 0x00, 0x44, 0x00, 0x45, 0x00, 0x4D, 0x00,
	0x5F, 0x00, 0x30, 0x00, 0x31, 0x00, 0x2E, 0x00, 0x31, 0x00, 0x34, 0x00,
	0x30, 0x00, 0x38, 0x00, 0x2E, 0x00, 0x30, 0x00, 0x37, 0x00, 0x00, 0x00,
	0x58, 0x00, 0x4D, 0x00, 0x4D, 0x00, 0x37, 0x00, 0x31, 0x00, 0x36, 0x00,
	0x30, 0x00, 0x5F, 0x00, 0x56, 0x00, 0x31, 0x00, 0x2E, 0x00, 0x31, 0x00,
	0x5F, 0x00, 0x4D, 0x00, 0x42, 0x00, 0x49, 0x00, 0x4D, 0x00, 0x5F, 0x00,
	0x47, 0x00, 0x4E, 0x00, 0x53, 0x00, 0x53, 0x00, 0x5F, 0x00, 0x4E, 0x00,
	0x41, 0x00, 0x4E, 0x00, 0x44, 0x00, 0x5F, 0x00, 0x52, 0x00, 0x45, 0x00
};

static const struct message_data message_data_device_caps = {
	.tid		= 2,
	.binary		= message_binary_device_caps,
	.binary_len	= sizeof(message_binary_device_caps),
};

static const unsigned char message_binary_device_caps_query[] = {
	0x03, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA2, 0x89, 0xCC, 0x33,
	0xBC, 0xBB, 0x8B, 0x4F, 0xB6, 0xB0, 0x13, 0x3E, 0xC2, 0xAA, 0xE6, 0xDF,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const struct message_data message_data_device_caps_query = {
	.tid = 2,
	.binary = message_binary_device_caps_query,
	.binary_len = sizeof(message_binary_device_caps_query),
};

static const unsigned char message_binary_subscriber_ready_status[] = {
	0x03, 0x00, 0x00, 0x80, 0xB4, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA2, 0x89, 0xCC, 0x33,
	0xBC, 0xBB, 0x8B, 0x4F, 0xB6, 0xB0, 0x13, 0x3E, 0xC2, 0xAA, 0xE6, 0xDF,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x84, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00,
	0x44, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x6C, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00,
	0x33, 0x00, 0x31, 0x00, 0x30, 0x00, 0x34, 0x00, 0x31, 0x00, 0x30, 0x00,
	0x32, 0x00, 0x32, 0x00, 0x37, 0x00, 0x39, 0x00, 0x32, 0x00, 0x33, 0x00,
	0x33, 0x00, 0x37, 0x00, 0x34, 0x00, 0x00, 0x00, 0x38, 0x00, 0x39, 0x00,
	0x30, 0x00, 0x31, 0x00, 0x34, 0x00, 0x31, 0x00, 0x30, 0x00, 0x34, 0x00,
	0x32, 0x00, 0x31, 0x00, 0x32, 0x00, 0x32, 0x00, 0x37, 0x00, 0x39, 0x00,
	0x32, 0x00, 0x33, 0x00, 0x33, 0x00, 0x37, 0x00, 0x34, 0x00, 0x37, 0x00,
	0x31, 0x00, 0x35, 0x00, 0x31, 0x00, 0x32, 0x00, 0x34, 0x00, 0x33, 0x00,
	0x31, 0x00, 0x30, 0x00, 0x35, 0x00, 0x39, 0x00, 0x36, 0x00, 0x00, 0x00
};

static const struct message_data message_data_subscriber_ready_status = {
	.tid		= 2,
	.binary		= message_binary_subscriber_ready_status,
	.binary_len	= sizeof(message_binary_subscriber_ready_status),
};

static const unsigned char message_binary_phonebook_read[] = {
	0x03, 0x00, 0x00, 0x80, 0x68, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4B, 0xF3, 0x84, 0x76,
	0x1E, 0x6A, 0x41, 0xDB, 0xB1, 0xD8, 0xBE, 0xD2, 0x89, 0xC2, 0x5B, 0xDB,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x2C, 0x00, 0x00, 0x00,
	0x03, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00,
	0x28, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x39, 0x00, 0x32, 0x00,
	0x31, 0x00, 0x31, 0x00, 0x32, 0x00, 0x33, 0x00, 0x34, 0x00, 0x35, 0x00,
	0x36, 0x00, 0x00, 0x00, 0x54, 0x00, 0x53, 0x00,
};

static const struct message_data message_data_phonebook_read = {
	.tid		= 2,
	.binary		= message_binary_phonebook_read,
	.binary_len	= sizeof(message_binary_phonebook_read),
};

static void do_debug(const char *str, void *user_data)
{
	const char *prefix = user_data;

	l_info("%s%s", prefix, str);
}

static struct mbim_message *build_message(const struct message_data *msg_data)
{
	static const unsigned int frag_size = 64;
	struct mbim_message *msg;
	struct iovec *iov;
	size_t n_iov;
	unsigned int i;

	n_iov = align_len(msg_data->binary_len, frag_size) / frag_size;
	iov = l_new(struct iovec, n_iov);

	iov[0].iov_len = msg_data->binary_len < frag_size ?
					msg_data->binary_len - 20 :
					frag_size - 20;
	iov[0].iov_base = l_memdup(msg_data->binary + 20, iov[0].iov_len);

	if (n_iov == 1)
		goto done;

	for (i = 1; i < n_iov - 1; i++) {
		iov[i].iov_base = l_memdup(msg_data->binary + i * frag_size,
							frag_size);
		iov[i].iov_len = frag_size;
	}

	iov[i].iov_len = msg_data->binary_len - i * frag_size;
	iov[i].iov_base = l_memdup(msg_data->binary + i * frag_size,
							iov[i].iov_len);

done:
	msg = _mbim_message_build(msg_data->binary, iov, n_iov);
	assert(msg);

	return msg;
}

static bool check_message(struct mbim_message *message,
					const struct message_data *msg_data)
{
	size_t len;
	void *message_binary = _mbim_message_to_bytearray(message, &len);
	bool r = false;

	l_util_hexdump(false, msg_data->binary, msg_data->binary_len,
			do_debug, "[MSG] ");

	l_util_hexdump(true, message_binary, len, do_debug, "[MSG] ");

	assert(message_binary);
	if (len != msg_data->binary_len)
		goto done;

	r = memcmp(message_binary, msg_data->binary, len) == 0;
done:
	l_free(message_binary);
	return r;
}

static void parse_device_caps(const void *data)
{
	struct mbim_message *msg = build_message(data);
	uint32_t device_type;
	uint32_t cellular_class;
	uint32_t voice_class;
	uint32_t sim_class;
	uint32_t data_class;
	uint32_t sms_caps;
	uint32_t control_caps;
	uint32_t max_sessions;
	char *custom_data_class;
	char *device_id;
	char *firmware_info;
	char *hardware_info;
	bool r;

	r = mbim_message_get_arguments(msg, "uuuuuuuussss",
					&device_type, &cellular_class,
					&voice_class, &sim_class, &data_class,
					&sms_caps, &control_caps, &max_sessions,
					&custom_data_class, &device_id,
					&firmware_info, &hardware_info);
	assert(r);

	assert(device_type == 1);
	assert(cellular_class = 1);
	assert(voice_class == 1);
	assert(sim_class == 2);
	assert(data_class == 0x3f);
	assert(sms_caps == 0x3);
	assert(control_caps == 1);
	assert(max_sessions == 16);
	assert(custom_data_class == NULL);
	assert(device_id);
	assert(!strcmp(device_id, "359336050018717"));
	assert(firmware_info);
	assert(!strcmp(firmware_info, "FIH7160_V1.1_MODEM_01.1408.07"));
	assert(hardware_info);
	assert(!strcmp(hardware_info, "XMM7160_V1.1_MBIM_GNSS_NAND_RE"));

	l_free(custom_data_class);
	l_free(device_id);
	l_free(firmware_info);
	l_free(hardware_info);
	mbim_message_unref(msg);
}

static void build_device_caps(const void *data)
{
	const struct message_data *msg_data = data;
	bool r;
	struct mbim_message *message;
	struct mbim_message_builder *builder;
	uint32_t device_type = 1;
	uint32_t cellular_class = 1;
	uint32_t voice_class = 1;
	uint32_t sim_class = 2;
	uint32_t data_class = 0x3f;
	uint32_t sms_caps = 0x3;
	uint32_t control_caps = 1;
	uint32_t max_sessions = 16;

	message = _mbim_message_new_command_done(mbim_uuid_basic_connect,
							1, 0);
	assert(message);

	builder = mbim_message_builder_new(message);
	assert(builder);

	assert(mbim_message_builder_append_basic(builder, 'u', &device_type));
	assert(mbim_message_builder_append_basic(builder, 'u',
							&cellular_class));
	assert(mbim_message_builder_append_basic(builder, 'u', &voice_class));
	assert(mbim_message_builder_append_basic(builder, 'u', &sim_class));
	assert(mbim_message_builder_append_basic(builder, 'u', &data_class));
	assert(mbim_message_builder_append_basic(builder, 'u', &sms_caps));
	assert(mbim_message_builder_append_basic(builder, 'u', &control_caps));
	assert(mbim_message_builder_append_basic(builder, 'u', &max_sessions));

	assert(mbim_message_builder_append_basic(builder, 's', NULL));
	assert(mbim_message_builder_append_basic(builder, 's',
							"359336050018717"));
	assert(mbim_message_builder_append_basic(builder, 's',
					"FIH7160_V1.1_MODEM_01.1408.07"));
	assert(mbim_message_builder_append_basic(builder, 's',
					"XMM7160_V1.1_MBIM_GNSS_NAND_RE"));

	assert(mbim_message_builder_finalize(builder));
	mbim_message_builder_free(builder);

	_mbim_message_set_tid(message, msg_data->tid);
	assert(check_message(message, msg_data));
	mbim_message_unref(message);

	/* now try to build the same message using set_arguments */
	message = _mbim_message_new_command_done(mbim_uuid_basic_connect,
							1, 0);
	assert(message);
	r = mbim_message_set_arguments(message, "uuuuuuuussss",
					1, 1, 1, 2, 0x3f, 0x3, 1, 16,
					NULL, "359336050018717",
					"FIH7160_V1.1_MODEM_01.1408.07",
					"XMM7160_V1.1_MBIM_GNSS_NAND_RE");
	assert(r);

	_mbim_message_set_tid(message, msg_data->tid);
	assert(check_message(message, msg_data));
	mbim_message_unref(message);
}

static void build_device_caps_query(const void *data)
{
	const struct message_data *msg_data = data;
	struct mbim_message *message;

	message = mbim_message_new(mbim_uuid_basic_connect, 1,
						MBIM_COMMAND_TYPE_QUERY);
	assert(message);
	assert(mbim_message_set_arguments(message, ""));

	_mbim_message_set_tid(message, msg_data->tid);
	assert(check_message(message, msg_data));
	mbim_message_unref(message);
}

static void parse_subscriber_ready_status(const void *data)
{
	struct mbim_message *msg = build_message(data);
	uint32_t ready_state;
	char *imsi;
	char *iccid;
	uint32_t ready_info;
	uint32_t n_phone_numbers;
	char *phone_number;
	struct mbim_message_iter array;
	bool r;

	r = mbim_message_get_arguments(msg, "ussuas",
					&ready_state, &imsi, &iccid,
					&ready_info,
					&n_phone_numbers, &array);
	assert(r);

	assert(ready_state == 1);
	assert(imsi);
	assert(!strcmp(imsi, "310410227923374"));
	assert(iccid);
	assert(!strcmp(iccid, "89014104212279233747"));
	assert(ready_info == 0);

	assert(n_phone_numbers == 1);
	assert(mbim_message_iter_next_entry(&array, &phone_number));

	assert(phone_number);
	assert(!strcmp(phone_number, "15124310596"));
	l_free(phone_number);

	assert(!mbim_message_iter_next_entry(&array, &phone_number));

	l_free(imsi);
	l_free(iccid);
	mbim_message_unref(msg);
}

static void build_subscriber_ready_status(const void *data)
{
	const struct message_data *msg_data = data;
	bool r;
	struct mbim_message *message;

	message = _mbim_message_new_command_done(mbim_uuid_basic_connect,
							2, 0);
	assert(message);

	r = mbim_message_set_arguments(message, "ussuas",
			1, "310410227923374", "89014104212279233747", 0,
			1, "15124310596");
	assert(r);

	_mbim_message_set_tid(message, msg_data->tid);
	assert(check_message(message, msg_data));
	mbim_message_unref(message);
}

static void parse_phonebook_read(const void *data)
{
	struct mbim_message *msg = build_message(data);
	uint32_t n_items;
	struct mbim_message_iter array;
	uint32_t index;
	char *number;
	char *name;
	bool r;

	r = mbim_message_get_arguments(msg, "a(uss)", &n_items, &array);
	assert(r);

	assert(n_items == 1);
	assert(mbim_message_iter_next_entry(&array, &index, &number, &name));
	assert(index == 3);
	assert(number);
	assert(!strcmp(number, "921123456"));
	assert(name);
	assert(!strcmp(name, "TS"));
	l_free(number);
	l_free(name);

	assert(!mbim_message_iter_next_entry(&array, &index, &number, &name));
	mbim_message_unref(msg);
}

static void build_phonebook_read(const void *data)
{
	const struct message_data *msg_data = data;
	bool r;
	struct mbim_message *message;

	message = _mbim_message_new_command_done(mbim_uuid_phonebook, 2, 0);
	assert(message);

	r = mbim_message_set_arguments(message, "a(uss)", 1,
			3, "921123456", "TS");
	assert(r);

	_mbim_message_set_tid(message, msg_data->tid);
	assert(check_message(message, msg_data));
	mbim_message_unref(message);
}

int main(int argc, char *argv[])
{
	l_test_init(&argc, &argv);

	l_test_add("Device Caps (parse)",
			parse_device_caps, &message_data_device_caps);
	l_test_add("Device Caps (build)",
			build_device_caps, &message_data_device_caps);

	l_test_add("Device Caps Query (build)", build_device_caps_query,
					&message_data_device_caps_query);

	l_test_add("Subscriber Ready Status (parse)",
			parse_subscriber_ready_status,
			&message_data_subscriber_ready_status);
	l_test_add("Subscriber Ready Status (build)",
			build_subscriber_ready_status,
			&message_data_subscriber_ready_status);

	l_test_add("Phonebook Read (parse)", parse_phonebook_read,
			&message_data_phonebook_read);
	l_test_add("Phonebook Read (build)", build_phonebook_read,
			&message_data_phonebook_read);

	return l_test_run();
}
