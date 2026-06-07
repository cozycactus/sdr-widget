#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

#define DEFAULT_VENDOR_ID      0x16c0u
#define DEFAULT_PRODUCT_ID     0x05dcu
#define DEFAULT_SERIAL         "1.0.0.0.0.0.0"

#define REQTYPE_CLASS_IN       0xa1u
#define REQTYPE_CLASS_OUT      0x21u
#define AUDIO_REQ_SET_CUR      0x01u
#define AUDIO_REQ_GET_CUR      0x81u
#define AUDIO_CONTROL_MUTE     0x01u
#define AUDIO_CONTROL_VOLUME   0x02u
#define AUDIO_CONTROL_IFACE    0x01u
#define AUDIO_MIC_FEATURE_UNIT 0x02u
#define AUDIO_SPK_FEATURE_UNIT 0x12u
#define AUDIO_CHANNEL_MASTER   0u
#define AUDIO_CHANNEL_LEFT     1u
#define AUDIO_CHANNEL_RIGHT    2u

static uint16_t vendor_id = DEFAULT_VENDOR_ID;
static uint16_t product_id = DEFAULT_PRODUCT_ID;
static const char *serial_filter = DEFAULT_SERIAL;
static unsigned int usb_timeout_ms = 2000u;

static const char *usb_error(int status)
{
	const char *name = libusb_error_name(status);

	return (name != NULL) ? name : "UNKNOWN";
}

static int parse_device_id(const char *text)
{
	char *end = NULL;
	unsigned long vendor = strtoul(text, &end, 16);
	unsigned long product;

	if ((end == text) || (*end != ':')) {
		return 0;
	}
	product = strtoul(end + 1, &end, 16);
	if ((*end != '\0') || (vendor > 0xfffful) || (product > 0xfffful)) {
		return 0;
	}

	vendor_id = (uint16_t)vendor;
	product_id = (uint16_t)product;
	return 1;
}

static libusb_device_handle *open_matching_device(char *opened_serial, size_t opened_serial_size)
{
	libusb_device **devices = NULL;
	libusb_device_handle *handle = NULL;
	ssize_t count = libusb_get_device_list(NULL, &devices);
	ssize_t index;

	if (count < 0) {
		fprintf(stderr, "uac-control: libusb_get_device_list failed: %s\n", usb_error((int)count));
		return NULL;
	}

	for (index = 0; index < count; index++) {
		struct libusb_device_descriptor desc;
		char serial[256] = "";
		int status;

		if (libusb_get_device_descriptor(devices[index], &desc) != 0) {
			continue;
		}
		if ((desc.idVendor != vendor_id) || (desc.idProduct != product_id)) {
			continue;
		}

		status = libusb_open(devices[index], &handle);
		if (status != 0) {
			fprintf(stderr, "uac-control: libusb_open(%04x:%04x) failed: %s\n",
			    desc.idVendor, desc.idProduct, usb_error(status));
			handle = NULL;
			continue;
		}

		if (desc.iSerialNumber != 0u) {
			status = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber,
			    (unsigned char *)serial, sizeof(serial) - 1u);
			if (status > 0) {
				serial[status] = '\0';
			}
		}

		if ((serial_filter != NULL) && (strcmp(serial, serial_filter) != 0)) {
			libusb_close(handle);
			handle = NULL;
			continue;
		}

		snprintf(opened_serial, opened_serial_size, "%s", serial);
		break;
	}

	libusb_free_device_list(devices, 1);
	return handle;
}

static int control_get(libusb_device_handle *handle, uint8_t unit, uint8_t control,
    uint8_t channel, uint8_t *data, uint16_t length, const char *label)
{
	int status = libusb_control_transfer(handle, REQTYPE_CLASS_IN, AUDIO_REQ_GET_CUR,
	    ((uint16_t)control << 8) | channel,
	    ((uint16_t)unit << 8) | AUDIO_CONTROL_IFACE,
	    data, length, usb_timeout_ms);

	if (status != (int)length) {
		fprintf(stderr, "uac-control: GET_CUR %s returned %s (%d), expected %u bytes\n",
		    label, (status < 0) ? usb_error(status) : "short read", status, length);
		return 0;
	}
	return 1;
}

static int control_set(libusb_device_handle *handle, uint8_t unit, uint8_t control,
    uint8_t channel, const uint8_t *data, uint16_t length, const char *label)
{
	int status = libusb_control_transfer(handle, REQTYPE_CLASS_OUT, AUDIO_REQ_SET_CUR,
	    ((uint16_t)control << 8) | channel,
	    ((uint16_t)unit << 8) | AUDIO_CONTROL_IFACE,
	    (unsigned char *)data, length, usb_timeout_ms);

	if (status != (int)length) {
		fprintf(stderr, "uac-control: SET_CUR %s returned %s (%d), expected %u bytes\n",
		    label, (status < 0) ? usb_error(status) : "short write", status, length);
		return 0;
	}
	return 1;
}

static uint16_t read_le16(const uint8_t *data)
{
	return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static void write_le16(uint8_t *data, uint16_t value)
{
	data[0] = (uint8_t)(value & 0xffu);
	data[1] = (uint8_t)(value >> 8);
}

static int verify_mute(libusb_device_handle *handle, uint8_t unit, const char *name)
{
	uint8_t data[1];
	int ok = 1;

	data[0] = 1u;
	ok &= control_set(handle, unit, AUDIO_CONTROL_MUTE, AUDIO_CHANNEL_MASTER,
	    data, sizeof(data), name);
	data[0] = 0u;
	ok &= control_get(handle, unit, AUDIO_CONTROL_MUTE, AUDIO_CHANNEL_MASTER,
	    data, sizeof(data), name);
	if (data[0] != 1u) {
		fprintf(stderr, "uac-control: %s mute read %u, expected 1\n", name, data[0]);
		ok = 0;
	}

	data[0] = 0u;
	ok &= control_set(handle, unit, AUDIO_CONTROL_MUTE, AUDIO_CHANNEL_MASTER,
	    data, sizeof(data), name);
	ok &= control_get(handle, unit, AUDIO_CONTROL_MUTE, AUDIO_CHANNEL_MASTER,
	    data, sizeof(data), name);
	if (data[0] != 0u) {
		fprintf(stderr, "uac-control: %s mute restore read %u, expected 0\n", name, data[0]);
		ok = 0;
	}
	printf("uac-control: %s mute round-trip %s\n", name, ok ? "pass" : "fail");
	return ok;
}

static int verify_volume_channel(libusb_device_handle *handle, uint8_t unit,
    uint8_t channel, const char *name, uint16_t value)
{
	uint8_t data[2];
	uint16_t actual;
	int ok = 1;

	write_le16(data, value);
	ok &= control_set(handle, unit, AUDIO_CONTROL_VOLUME, channel, data, sizeof(data), name);
	data[0] = 0u;
	data[1] = 0u;
	ok &= control_get(handle, unit, AUDIO_CONTROL_VOLUME, channel, data, sizeof(data), name);
	actual = read_le16(data);
	if (actual != value) {
		fprintf(stderr, "uac-control: %s volume read 0x%04x, expected 0x%04x\n",
		    name, actual, value);
		ok = 0;
	}

	write_le16(data, 0u);
	ok &= control_set(handle, unit, AUDIO_CONTROL_VOLUME, channel, data, sizeof(data), name);
	ok &= control_get(handle, unit, AUDIO_CONTROL_VOLUME, channel, data, sizeof(data), name);
	actual = read_le16(data);
	if (actual != 0u) {
		fprintf(stderr, "uac-control: %s volume restore read 0x%04x, expected 0\n",
		    name, actual);
		ok = 0;
	}
	printf("uac-control: %s volume round-trip %s\n", name, ok ? "pass" : "fail");
	return ok;
}

static int verify_controls(libusb_device_handle *handle)
{
	int ok = 1;

	ok &= verify_mute(handle, AUDIO_MIC_FEATURE_UNIT, "mic");
	ok &= verify_mute(handle, AUDIO_SPK_FEATURE_UNIT, "speaker");
	ok &= verify_volume_channel(handle, AUDIO_MIC_FEATURE_UNIT,
	    AUDIO_CHANNEL_LEFT, "mic-left", 0x0100u);
	ok &= verify_volume_channel(handle, AUDIO_MIC_FEATURE_UNIT,
	    AUDIO_CHANNEL_RIGHT, "mic-right", 0xff00u);
	ok &= verify_volume_channel(handle, AUDIO_SPK_FEATURE_UNIT,
	    AUDIO_CHANNEL_LEFT, "speaker-left", 0x0200u);
	ok &= verify_volume_channel(handle, AUDIO_SPK_FEATURE_UNIT,
	    AUDIO_CHANNEL_RIGHT, "speaker-right", 0xfe00u);

	printf("uac-control: %s\n", ok ? "pass" : "fail");
	return ok ? 0 : 1;
}

static void usage(const char *program)
{
	fprintf(stderr,
	    "usage: %s [--verify] [--device VID:PID] [--serial SERIAL] [--timeout-ms N]\n",
	    program);
}

int main(int argc, char **argv)
{
	libusb_device_handle *handle;
	char opened_serial[256] = "";
	int status;
	int index;

	for (index = 1; index < argc; index++) {
		if (strcmp(argv[index], "--verify") == 0) {
			continue;
		} else if ((strcmp(argv[index], "--device") == 0) && ((index + 1) < argc)) {
			if (!parse_device_id(argv[++index])) {
				fprintf(stderr, "uac-control: invalid --device value\n");
				return 1;
			}
		} else if ((strcmp(argv[index], "--serial") == 0) && ((index + 1) < argc)) {
			serial_filter = argv[++index];
		} else if ((strcmp(argv[index], "--timeout-ms") == 0) && ((index + 1) < argc)) {
			usb_timeout_ms = (unsigned int)strtoul(argv[++index], NULL, 0);
		} else if ((strcmp(argv[index], "-h") == 0) || (strcmp(argv[index], "--help") == 0)) {
			usage(argv[0]);
			return 0;
		} else {
			usage(argv[0]);
			return 1;
		}
	}

	status = libusb_init(NULL);
	if (status != 0) {
		fprintf(stderr, "uac-control: libusb_init failed: %s\n", usb_error(status));
		return 1;
	}

	handle = open_matching_device(opened_serial, sizeof(opened_serial));
	if (handle == NULL) {
		fprintf(stderr, "uac-control: failed to open %04x:%04x serial %s\n",
		    vendor_id, product_id, serial_filter ? serial_filter : "<any>");
		libusb_exit(NULL);
		return 1;
	}

	printf("uac-control: opened %04x:%04x serial %s\n",
	    vendor_id, product_id, opened_serial[0] ? opened_serial : "<none>");
	status = verify_controls(handle);

	libusb_close(handle);
	libusb_exit(NULL);
	return status;
}
