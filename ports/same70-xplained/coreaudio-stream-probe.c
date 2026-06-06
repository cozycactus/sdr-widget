#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define DEFAULT_DEVICE_NAME "Yoyodyne SDR-Widget"
#define PROBE_SECONDS 2.0

typedef struct {
	uint32_t callbacks;
	uint64_t input_bytes;
	uint64_t output_bytes;
} io_state_t;

static void print_osstatus(const char *label, OSStatus status)
{
	fprintf(stderr, "%s failed: %d (0x%08x)\n", label, (int)status, (unsigned int)status);
}

static uint32_t channel_count(AudioDeviceID device, AudioObjectPropertyScope scope)
{
	AudioObjectPropertyAddress address = {
		kAudioDevicePropertyStreamConfiguration,
		scope,
		kAudioObjectPropertyElementMain
	};
	UInt32 size = 0u;
	AudioBufferList *buffers = NULL;
	uint32_t channels = 0u;
	OSStatus status;
	UInt32 index;

	status = AudioObjectGetPropertyDataSize(device, &address, 0u, NULL, &size);
	if ((status != noErr) || (size == 0u)) {
		return 0u;
	}

	buffers = (AudioBufferList *)calloc(1u, size);
	if (buffers == NULL) {
		return 0u;
	}

	status = AudioObjectGetPropertyData(device, &address, 0u, NULL, &size, buffers);
	if (status == noErr) {
		for (index = 0u; index < buffers->mNumberBuffers; index++) {
			channels += buffers->mBuffers[index].mNumberChannels;
		}
	}

	free(buffers);
	return channels;
}

static uint32_t stream_buffer_count(AudioDeviceID device, AudioObjectPropertyScope scope)
{
	AudioObjectPropertyAddress address = {
		kAudioDevicePropertyStreamConfiguration,
		scope,
		kAudioObjectPropertyElementMain
	};
	UInt32 size = 0u;
	AudioBufferList *buffers = NULL;
	uint32_t count = 0u;
	OSStatus status;

	status = AudioObjectGetPropertyDataSize(device, &address, 0u, NULL, &size);
	if ((status != noErr) || (size == 0u)) {
		return 0u;
	}

	buffers = (AudioBufferList *)calloc(1u, size);
	if (buffers == NULL) {
		return 0u;
	}

	status = AudioObjectGetPropertyData(device, &address, 0u, NULL, &size, buffers);
	if (status == noErr) {
		count = buffers->mNumberBuffers;
	}

	free(buffers);
	return count;
}

static int cfstring_get(CFStringRef text, char *buffer, size_t size)
{
	if ((text == NULL) || (buffer == NULL) || (size == 0u)) {
		return 0;
	}

	return CFStringGetCString(text, buffer, size, kCFStringEncodingUTF8);
}

static int find_device(const char *needle, AudioDeviceID *device)
{
	AudioObjectPropertyAddress address = {
		kAudioHardwarePropertyDevices,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMain
	};
	AudioObjectPropertyAddress name_address = {
		kAudioObjectPropertyName,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMain
	};
	UInt32 size = 0u;
	AudioDeviceID *devices = NULL;
	UInt32 count;
	UInt32 index;
	OSStatus status;

	status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0u, NULL, &size);
	if ((status != noErr) || (size == 0u)) {
		print_osstatus("AudioObjectGetPropertyDataSize(devices)", status);
		return 0;
	}

	devices = (AudioDeviceID *)calloc(1u, size);
	if (devices == NULL) {
		fprintf(stderr, "allocation failed\n");
		return 0;
	}

	status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0u, NULL, &size, devices);
	if (status != noErr) {
		print_osstatus("AudioObjectGetPropertyData(devices)", status);
		free(devices);
		return 0;
	}

	count = size / sizeof(AudioDeviceID);
	for (index = 0u; index < count; index++) {
		CFStringRef name = NULL;
		char name_text[256];
		UInt32 prop_size = sizeof(name);

		status = AudioObjectGetPropertyData(devices[index], &name_address, 0u, NULL, &prop_size, &name);
		if (status != noErr) {
			continue;
		}

		if (cfstring_get(name, name_text, sizeof(name_text)) && (strstr(name_text, needle) != NULL)) {
				*device = devices[index];
			printf("device=%s input_channels=%u output_channels=%u input_streams=%u output_streams=%u\n",
				name_text,
				channel_count(devices[index], kAudioDevicePropertyScopeInput),
				channel_count(devices[index], kAudioDevicePropertyScopeOutput),
				stream_buffer_count(devices[index], kAudioDevicePropertyScopeInput),
				stream_buffer_count(devices[index], kAudioDevicePropertyScopeOutput));
			CFRelease(name);
			free(devices);
			return 1;
		}

		CFRelease(name);
	}

	free(devices);
	return 0;
}

static OSStatus io_callback(AudioObjectID device, const AudioTimeStamp *now,
	const AudioBufferList *input_data, const AudioTimeStamp *input_time,
	AudioBufferList *output_data, const AudioTimeStamp *output_time,
	void *client_data)
{
	io_state_t *state = (io_state_t *)client_data;
	UInt32 index;

	(void)device;
	(void)now;
	(void)input_time;
	(void)output_time;

	state->callbacks++;
	if (input_data != NULL) {
		for (index = 0u; index < input_data->mNumberBuffers; index++) {
			state->input_bytes += input_data->mBuffers[index].mDataByteSize;
		}
	}
	if (output_data != NULL) {
		for (index = 0u; index < output_data->mNumberBuffers; index++) {
			if (output_data->mBuffers[index].mData != NULL) {
				memset(output_data->mBuffers[index].mData, 0, output_data->mBuffers[index].mDataByteSize);
				state->output_bytes += output_data->mBuffers[index].mDataByteSize;
			}
		}
	}

	return noErr;
}

static void enable_io_proc_streams(AudioDeviceID device, AudioDeviceIOProcID proc_id,
	AudioObjectPropertyScope scope);

static int run_hal_probe(AudioDeviceID device)
{
	AudioDeviceIOProcID proc_id = NULL;
	io_state_t state;
	OSStatus status;

	memset(&state, 0, sizeof(state));
	status = AudioDeviceCreateIOProcID(device, io_callback, &state, &proc_id);
	if (status != noErr) {
		print_osstatus("AudioDeviceCreateIOProcID", status);
		return 0;
	}

	enable_io_proc_streams(device, proc_id, kAudioDevicePropertyScopeInput);
	enable_io_proc_streams(device, proc_id, kAudioDevicePropertyScopeOutput);
	status = AudioDeviceStart(device, proc_id);
	if (status != noErr) {
		print_osstatus("AudioDeviceStart", status);
		AudioDeviceDestroyIOProcID(device, proc_id);
		return 0;
	}

	CFRunLoopRunInMode(kCFRunLoopDefaultMode, PROBE_SECONDS, false);
	AudioDeviceStop(device, proc_id);
	AudioDeviceDestroyIOProcID(device, proc_id);
	printf("callbacks=%u input_bytes=%llu output_bytes=%llu\n",
		state.callbacks,
		(unsigned long long)state.input_bytes,
		(unsigned long long)state.output_bytes);
	return 1;
}

static void enable_io_proc_streams(AudioDeviceID device, AudioDeviceIOProcID proc_id,
	AudioObjectPropertyScope scope)
{
	AudioObjectPropertyAddress address = {
		kAudioDevicePropertyIOProcStreamUsage,
		scope,
		kAudioObjectPropertyElementMain
	};
	uint32_t streams = stream_buffer_count(device, scope);
	size_t size;
	AudioHardwareIOProcStreamUsage *usage;
	uint32_t index;
	OSStatus status;

	if (streams == 0u) {
		return;
	}

	size = offsetof(AudioHardwareIOProcStreamUsage, mStreamIsOn) + (streams * sizeof(UInt32));
	usage = (AudioHardwareIOProcStreamUsage *)calloc(1u, size);
	if (usage == NULL) {
		fprintf(stderr, "stream usage allocation failed\n");
		return;
	}

	usage->mIOProc = (void *)proc_id;
	usage->mNumberStreams = streams;
	for (index = 0u; index < streams; index++) {
		usage->mStreamIsOn[index] = 1u;
	}

	status = AudioObjectSetPropertyData(device, &address, 0u, NULL, (UInt32)size, usage);
	if (status != noErr) {
		print_osstatus((scope == kAudioDevicePropertyScopeInput) ?
			"AudioObjectSetPropertyData(input stream usage)" :
			"AudioObjectSetPropertyData(output stream usage)", status);
	}

	free(usage);
}

int main(int argc, char **argv)
{
	const char *device_name = DEFAULT_DEVICE_NAME;
	AudioDeviceID device = kAudioObjectUnknown;

	setvbuf(stdout, NULL, _IOLBF, 0);
	if (argc > 1) {
		device_name = argv[1];
	}

	if (!find_device(device_name, &device)) {
		fprintf(stderr, "device containing \"%s\" not found\n", device_name);
		return 1;
	}

	return run_hal_probe(device) ? 0 : 1;
}
