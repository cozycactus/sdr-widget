#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_DEVICE_NAME "Yoyodyne SDR-Widget"
#define PROBE_SECONDS 2.0
#define PCM24_SCALE 8388608.0f
#define MAX_CAPTURE_SAMPLES 1048576u
#define ALIGN_WINDOW_SAMPLES 128u
#define VERIFY_MIN_SAMPLES 4096u

typedef struct {
	uint32_t callbacks;
	uint64_t input_bytes;
	uint64_t output_bytes;
	uint64_t input_checksum;
	uint64_t output_checksum;
	uint64_t input_nonzero;
	uint64_t output_nonzero;
	uint32_t sample_lcg;
	int32_t *input_samples;
	int32_t *output_samples;
	uint32_t input_sample_count;
	uint32_t output_sample_count;
	uint32_t input_sample_overflow;
	uint32_t output_sample_overflow;
} io_state_t;

typedef struct {
	uint32_t passed;
	uint32_t aligned;
	uint32_t input_offset;
	uint32_t compared;
	uint32_t mismatches;
	uint32_t first_mismatch;
	int32_t first_expected;
	int32_t first_actual;
} verify_result_t;

static void update_checksum(uint64_t *checksum, uint64_t *nonzero,
	const uint8_t *data, UInt32 length)
{
	UInt32 index;

	for (index = 0u; index < length; index++) {
		*checksum = (*checksum * 33u) ^ data[index];
		if (data[index] != 0u) {
			(*nonzero)++;
		}
	}
}

static int32_t next_test_sample(io_state_t *state)
{
	int32_t sample;

	state->sample_lcg = (state->sample_lcg * 1664525u) + 1013904223u;
	sample = (int32_t)((state->sample_lcg >> 9) & 0x3fffffu) - 0x200000;
	if (sample == 0) {
		sample = 1;
	}

	return sample;
}

static float pcm24_to_float(int32_t sample)
{
	return (float)sample / PCM24_SCALE;
}

static int32_t float_to_pcm24(float sample)
{
	float scaled;

	if (sample >= 0.99999988f) {
		return 8388607;
	}
	if (sample <= -1.0f) {
		return -8388608;
	}

	scaled = sample * PCM24_SCALE;
	return (scaled >= 0.0f) ? (int32_t)(scaled + 0.5f) : (int32_t)(scaled - 0.5f);
}

static void append_input_sample(io_state_t *state, int32_t sample)
{
	if (state->input_sample_count >= MAX_CAPTURE_SAMPLES) {
		state->input_sample_overflow++;
		return;
	}

	state->input_samples[state->input_sample_count++] = sample;
}

static void append_output_sample(io_state_t *state, int32_t sample)
{
	if (state->output_sample_count >= MAX_CAPTURE_SAMPLES) {
		state->output_sample_overflow++;
		return;
	}

	state->output_samples[state->output_sample_count++] = sample;
}

static void capture_input_samples(io_state_t *state, const uint8_t *data, UInt32 length)
{
	const float *samples = (const float *)data;
	UInt32 count = length / sizeof(float);
	UInt32 index;

	for (index = 0u; index < count; index++) {
		append_input_sample(state, float_to_pcm24(samples[index]));
	}
}

static void fill_output_pattern(io_state_t *state, uint8_t *data, UInt32 length)
{
	float *samples = (float *)data;
	UInt32 count = length / sizeof(float);
	UInt32 index;
	int32_t sample;

	for (index = 0u; index < count; index++) {
		sample = next_test_sample(state);
		samples[index] = pcm24_to_float(sample);
		append_output_sample(state, sample);
	}
	update_checksum(&state->output_checksum, &state->output_nonzero, data, length);
}

static uint32_t samples_match(int32_t a, int32_t b)
{
	return a == b;
}

static uint32_t min_u32(uint32_t a, uint32_t b)
{
	return (a < b) ? a : b;
}

static uint32_t find_loopback_alignment(const io_state_t *state, uint32_t *input_offset)
{
	uint32_t window = min_u32(ALIGN_WINDOW_SAMPLES, state->output_sample_count);
	uint32_t offset;
	uint32_t index;

	if ((window == 0u) || (state->input_sample_count < window)) {
		return 0u;
	}

	for (offset = 0u; offset <= (state->input_sample_count - window); offset++) {
		for (index = 0u; index < window; index++) {
			if (!samples_match(state->input_samples[offset + index], state->output_samples[index])) {
				break;
			}
		}
		if (index == window) {
			*input_offset = offset;
			return 1u;
		}
	}

	return 0u;
}

static verify_result_t verify_loopback(const io_state_t *state)
{
	verify_result_t result;
	uint32_t index;

	memset(&result, 0, sizeof(result));
	result.first_mismatch = UINT32_MAX;
	if ((state->input_sample_overflow != 0u) || (state->output_sample_overflow != 0u)) {
		return result;
	}
	if (!find_loopback_alignment(state, &result.input_offset)) {
		return result;
	}

	result.aligned = 1u;
	result.compared = min_u32(state->output_sample_count, state->input_sample_count - result.input_offset);
	for (index = 0u; index < result.compared; index++) {
		int32_t expected = state->output_samples[index];
		int32_t actual = state->input_samples[result.input_offset + index];
		if (!samples_match(actual, expected)) {
			if (result.first_mismatch == UINT32_MAX) {
				result.first_mismatch = index;
				result.first_expected = expected;
				result.first_actual = actual;
			}
			result.mismatches++;
		}
	}

	result.passed = (result.compared >= VERIFY_MIN_SAMPLES) && (result.mismatches == 0u);
	return result;
}

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
			if (input_data->mBuffers[index].mData != NULL) {
				update_checksum(&state->input_checksum,
					&state->input_nonzero,
					(const uint8_t *)input_data->mBuffers[index].mData,
					input_data->mBuffers[index].mDataByteSize);
				capture_input_samples(state,
					(const uint8_t *)input_data->mBuffers[index].mData,
					input_data->mBuffers[index].mDataByteSize);
			}
		}
	}
	if (output_data != NULL) {
		for (index = 0u; index < output_data->mNumberBuffers; index++) {
			if (output_data->mBuffers[index].mData != NULL) {
				fill_output_pattern(state,
					(uint8_t *)output_data->mBuffers[index].mData,
					output_data->mBuffers[index].mDataByteSize);
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
	verify_result_t verify;
	OSStatus status;

	memset(&state, 0, sizeof(state));
	state.sample_lcg = 0x12345678u;
	state.input_samples = (int32_t *)calloc(MAX_CAPTURE_SAMPLES, sizeof(int32_t));
	state.output_samples = (int32_t *)calloc(MAX_CAPTURE_SAMPLES, sizeof(int32_t));
	if ((state.input_samples == NULL) || (state.output_samples == NULL)) {
		fprintf(stderr, "sample capture allocation failed\n");
		free(state.input_samples);
		free(state.output_samples);
		return 0;
	}

	status = AudioDeviceCreateIOProcID(device, io_callback, &state, &proc_id);
	if (status != noErr) {
		print_osstatus("AudioDeviceCreateIOProcID", status);
		free(state.input_samples);
		free(state.output_samples);
		return 0;
	}

	enable_io_proc_streams(device, proc_id, kAudioDevicePropertyScopeInput);
	enable_io_proc_streams(device, proc_id, kAudioDevicePropertyScopeOutput);
	status = AudioDeviceStart(device, proc_id);
	if (status != noErr) {
		print_osstatus("AudioDeviceStart", status);
		AudioDeviceDestroyIOProcID(device, proc_id);
		free(state.input_samples);
		free(state.output_samples);
		return 0;
	}

	CFRunLoopRunInMode(kCFRunLoopDefaultMode, PROBE_SECONDS, false);
	AudioDeviceStop(device, proc_id);
	AudioDeviceDestroyIOProcID(device, proc_id);
	verify = verify_loopback(&state);
	printf("started=1 callbacks=%u input_bytes=%llu output_bytes=%llu "
		"input_nonzero=%llu output_nonzero=%llu input_checksum=%llu output_checksum=%llu\n",
		state.callbacks,
		(unsigned long long)state.input_bytes,
		(unsigned long long)state.output_bytes,
		(unsigned long long)state.input_nonzero,
		(unsigned long long)state.output_nonzero,
		(unsigned long long)state.input_checksum,
		(unsigned long long)state.output_checksum);
	printf("verify=%s aligned=%u input_offset_samples=%u compared_samples=%u "
		"mismatches=%u first_mismatch=%u expected=%d actual=%d "
		"input_samples=%u output_samples=%u input_overflow=%u output_overflow=%u\n",
		verify.passed ? "pass" : "fail",
		verify.aligned,
		verify.input_offset,
		verify.compared,
		verify.mismatches,
		verify.first_mismatch,
		verify.first_expected,
		verify.first_actual,
		state.input_sample_count,
		state.output_sample_count,
		state.input_sample_overflow,
		state.output_sample_overflow);
	free(state.input_samples);
	free(state.output_samples);
	return verify.passed ? 1 : 0;
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
