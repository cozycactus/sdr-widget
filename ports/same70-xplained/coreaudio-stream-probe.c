#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_DEVICE_NAME "Yoyodyne SDR-Widget"
#define DEFAULT_PROBE_SECONDS 2.0
#define MAX_PROBE_SECONDS 120.0
#define DEFAULT_PROBE_RUNS 1u
#define MAX_PROBE_RUNS 100u
#define VERIFY_MODE_LOOPBACK 0u
#define VERIFY_MODE_INPUT    1u
#define VERIFY_MODE_PATTERN  2u
#define VERIFY_MODE_SILENCE  3u
#define VERIFY_MODE_TONE     4u
#define VERIFY_MODE_SINE     5u
#define VERIFY_MODE_MELODY   6u
#define DEFAULT_SAMPLE_RATE_HZ 48000u
#define DEFAULT_SAMPLE_BITS 24u
#define CAPTURE_SAMPLES_PER_SECOND 128000u
#define CAPTURE_SAMPLE_MARGIN 65536u
#define ALIGN_WINDOW_SAMPLES 128u
#define PATTERN_ALIGN_MAX_SAMPLES 65536u
#define VERIFY_MIN_SAMPLES 4096u
#define VERIFY_HASH_OFFSET_BASIS 1469598103934665603ull
#define VERIFY_HASH_PRIME 1099511628211ull
#define TONE_PERIOD_SAMPLES 96u
#define TONE_ALIGN_MAX_SAMPLES 4096u
#define SINE_PERIOD_SAMPLES 96u
#define SINE_ALIGN_MAX_SAMPLES 4096u
#define MELODY_ALIGN_MAX_SAMPLES 1048576u
#define MELODY_NOTE_COUNT 24u
#define MELODY_PHASE_SHIFT 16u
#define MELODY_PHASE_PERIOD (SINE_PERIOD_SAMPLES << MELODY_PHASE_SHIFT)
#define MELODY_NOTE_FRAMES_48K 9600u
#define MELODY_NOTE_FRAMES_44K 8820u

typedef struct {
	uint32_t callbacks;
	uint64_t input_bytes;
	uint64_t output_bytes;
	uint64_t input_checksum;
	uint64_t output_checksum;
	uint64_t input_nonzero;
	uint64_t output_nonzero;
	uint32_t sample_lcg;
	uint32_t silent_output;
	uint32_t sample_bits;
	int32_t *input_samples;
	int32_t *output_samples;
	uint32_t capture_sample_capacity;
	uint32_t input_sample_count;
	uint32_t output_sample_count;
	uint32_t input_sample_overflow;
	uint32_t output_sample_overflow;
} io_state_t;

typedef struct {
	uint32_t passed;
	uint32_t aligned;
	uint32_t input_offset;
	uint32_t expected_offset;
	uint32_t compared;
	uint32_t mismatches;
	uint64_t expected_hash;
	uint64_t actual_hash;
	uint32_t first_mismatch;
	int32_t first_expected;
	int32_t first_actual;
} verify_result_t;

typedef struct {
	uint32_t passed;
	uint32_t failed;
	uint64_t compared;
	uint64_t mismatches;
} probe_summary_t;

static const int32_t sine_samples_16[SINE_PERIOD_SAMPLES] = {
	0, 536, 1069, 1598, 2120, 2633, 3135, 3623,
	4096, 4551, 4987, 5401, 5793, 6159, 6499, 6811,
	7094, 7347, 7568, 7757, 7913, 8035, 8122, 8174,
	8192, 8174, 8122, 8035, 7913, 7757, 7568, 7347,
	7094, 6811, 6499, 6159, 5793, 5401, 4987, 4551,
	4096, 3623, 3135, 2633, 2120, 1598, 1069, 536,
	0, -536, -1069, -1598, -2120, -2633, -3135, -3623,
	-4096, -4551, -4987, -5401, -5793, -6159, -6499, -6811,
	-7094, -7347, -7568, -7757, -7913, -8035, -8122, -8174,
	-8192, -8174, -8122, -8035, -7913, -7757, -7568, -7347,
	-7094, -6811, -6499, -6159, -5793, -5401, -4987, -4551,
	-4096, -3623, -3135, -2633, -2120, -1598, -1069, -536,
};

static const uint32_t melody_notes_48k[MELODY_NOTE_COUNT] = {
	34292u, 43205u, 51380u, 68584u, 64734u, 51380u,
	43205u, 38491u, 34292u, 38491u, 43205u, 51380u,
	57672u, 51380u, 43205u, 34292u, 38491u, 43205u,
	51380u, 57672u, 51380u, 43205u, 38491u, 34292u,
};

static const uint32_t melody_notes_44k[MELODY_NOTE_COUNT] = {
	37324u, 47026u, 55923u, 74649u, 70459u, 55923u,
	47026u, 41895u, 37324u, 41895u, 47026u, 55923u,
	62772u, 55923u, 47026u, 37324u, 41895u, 47026u,
	55923u, 62772u, 55923u, 47026u, 41895u, 37324u,
};

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

static int32_t next_lcg_sample(uint32_t *lcg, uint32_t sample_bits)
{
	uint32_t value_bits = sample_bits - 2u;
	uint32_t midpoint = 1u << (value_bits - 1u);
	uint32_t mask = (1u << value_bits) - 1u;
	uint32_t shift = (sample_bits == 16u) ? 17u : 9u;
	int32_t sample;

	*lcg = (*lcg * 1664525u) + 1013904223u;
	sample = (int32_t)((*lcg >> shift) & mask) - (int32_t)midpoint;
	if (sample == 0) {
		sample = 1;
	}

	return sample;
}

static int32_t next_test_sample(io_state_t *state)
{
	return next_lcg_sample(&state->sample_lcg, state->sample_bits);
}

static float pcm_scale(uint32_t bits)
{
	return (float)(1u << (bits - 1u));
}

static float pcm_to_float(int32_t sample, uint32_t bits)
{
	return (float)sample / pcm_scale(bits);
}

static int32_t float_to_pcm(float sample, uint32_t bits)
{
	float scaled;
	int32_t min_sample = -(int32_t)(1u << (bits - 1u));
	int32_t max_sample = (int32_t)((1u << (bits - 1u)) - 1u);

	if (sample >= ((float)max_sample / pcm_scale(bits))) {
		return max_sample;
	}
	if (sample <= -1.0f) {
		return min_sample;
	}

	scaled = sample * pcm_scale(bits);
	return (scaled >= 0.0f) ? (int32_t)(scaled + 0.5f) : (int32_t)(scaled - 0.5f);
}

static void append_input_sample(io_state_t *state, int32_t sample)
{
	if (state->input_sample_count >= state->capture_sample_capacity) {
		state->input_sample_overflow++;
		return;
	}

	state->input_samples[state->input_sample_count++] = sample;
}

static void append_output_sample(io_state_t *state, int32_t sample)
{
	if (state->output_sample_count >= state->capture_sample_capacity) {
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
		append_input_sample(state, float_to_pcm(samples[index], state->sample_bits));
	}
}

static void fill_output_buffer(io_state_t *state, uint8_t *data, UInt32 length)
{
	float *samples = (float *)data;
	UInt32 count = length / sizeof(float);
	UInt32 index;
	int32_t sample;

	for (index = 0u; index < count; index++) {
		sample = state->silent_output ? 0 : next_test_sample(state);
		samples[index] = pcm_to_float(sample, state->sample_bits);
		append_output_sample(state, sample);
	}
	update_checksum(&state->output_checksum, &state->output_nonzero, data, length);
}

static uint32_t samples_match(int32_t a, int32_t b)
{
	return a == b;
}

static uint64_t hash_sample(uint64_t hash, int32_t sample)
{
	uint32_t value = (uint32_t)sample;
	uint32_t index;

	for (index = 0u; index < sizeof(value); index++) {
		hash ^= (uint8_t)(value >> (index * 8u));
		hash *= VERIFY_HASH_PRIME;
	}

	return hash;
}

static int write_bytes(FILE *file, const void *data, size_t length)
{
	return fwrite(data, 1u, length, file) == length;
}

static int write_le16(FILE *file, uint32_t value)
{
	uint8_t bytes[2];

	bytes[0] = (uint8_t)(value & 0xffu);
	bytes[1] = (uint8_t)((value >> 8) & 0xffu);
	return write_bytes(file, bytes, sizeof(bytes));
}

static int write_le32(FILE *file, uint32_t value)
{
	uint8_t bytes[4];

	bytes[0] = (uint8_t)(value & 0xffu);
	bytes[1] = (uint8_t)((value >> 8) & 0xffu);
	bytes[2] = (uint8_t)((value >> 16) & 0xffu);
	bytes[3] = (uint8_t)((value >> 24) & 0xffu);
	return write_bytes(file, bytes, sizeof(bytes));
}

static int write_samples_wav(const char *label, const char *path,
	const int32_t *samples, uint32_t available_samples,
	uint32_t sample_rate_hz, uint32_t sample_bits, uint32_t channels)
{
	FILE *file;
	uint32_t bytes_per_sample;
	uint32_t sample_count;
	uint32_t data_size;
	uint32_t block_align;
	uint32_t byte_rate;
	uint32_t index;
	int ok;

	if (path == NULL) {
		return 1;
	}
	if ((sample_bits != 16u) && (sample_bits != 24u)) {
		fprintf(stderr, "cannot dump %u-bit samples as WAV\n", sample_bits);
		return 0;
	}
	if (channels == 0u) {
		channels = 1u;
	}

	bytes_per_sample = sample_bits / 8u;
	sample_count = available_samples - (available_samples % channels);
	data_size = sample_count * bytes_per_sample;
	block_align = channels * bytes_per_sample;
	byte_rate = sample_rate_hz * block_align;

	file = fopen(path, "wb");
	if (file == NULL) {
		fprintf(stderr, "failed to open WAV dump: %s\n", path);
		return 0;
	}

	ok =
		write_bytes(file, "RIFF", 4u) &&
		write_le32(file, 36u + data_size) &&
		write_bytes(file, "WAVE", 4u) &&
		write_bytes(file, "fmt ", 4u) &&
		write_le32(file, 16u) &&
		write_le16(file, 1u) &&
		write_le16(file, channels) &&
		write_le32(file, sample_rate_hz) &&
		write_le32(file, byte_rate) &&
		write_le16(file, block_align) &&
		write_le16(file, sample_bits) &&
		write_bytes(file, "data", 4u) &&
		write_le32(file, data_size);

	for (index = 0u; ok && (index < sample_count); index++) {
		uint32_t value = (uint32_t)samples[index];

		if (sample_bits == 16u) {
			ok = write_le16(file, value & 0xffffu);
		} else {
			ok =
				fputc((int)(value & 0xffu), file) != EOF &&
				fputc((int)((value >> 8) & 0xffu), file) != EOF &&
				fputc((int)((value >> 16) & 0xffu), file) != EOF;
		}
	}

	if (fclose(file) != 0) {
		ok = 0;
	}
	if (!ok) {
		fprintf(stderr, "failed to write WAV dump: %s\n", path);
		return 0;
	}

	printf("%s=%s samples=%u channels=%u rate=%u bits=%u bytes=%u\n",
		label, path, sample_count, channels, sample_rate_hz, sample_bits, data_size);
	return 1;
}

static int write_input_wav(const char *path, const io_state_t *state,
	uint32_t sample_rate_hz, uint32_t sample_bits, uint32_t channels)
{
	return write_samples_wav("dump_input_wav", path, state->input_samples,
		state->input_sample_count, sample_rate_hz, sample_bits, channels);
}

static int write_output_wav(const char *path, const io_state_t *state,
	uint32_t sample_rate_hz, uint32_t sample_bits, uint32_t channels)
{
	return write_samples_wav("dump_output_wav", path, state->output_samples,
		state->output_sample_count, sample_rate_hz, sample_bits, channels);
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

static uint32_t first_nonzero_input_offset(const io_state_t *state, uint32_t *input_offset)
{
	uint32_t index;

	for (index = 0u; index < state->input_sample_count; index++) {
		if (state->input_samples[index] != 0) {
			*input_offset = index;
			return 1u;
		}
	}

	return 0u;
}

static void fill_expected_pattern(int32_t *samples, uint32_t count, uint32_t sample_bits)
{
	uint32_t lcg = 0x12345678u;
	uint32_t index;

	for (index = 0u; index < count; index++) {
		samples[index] = next_lcg_sample(&lcg, sample_bits);
	}
}

static int32_t expected_tone_sample(uint32_t index, uint32_t sample_bits)
{
	uint32_t positive = (index % TONE_PERIOD_SAMPLES) < (TONE_PERIOD_SAMPLES / 2u);

	if (sample_bits == 16u) {
		return positive ? 0x2000 : -0x2000;
	}

	return positive ? 0x200000 : -0x200000;
}

static void fill_expected_tone(int32_t *samples, uint32_t count, uint32_t sample_bits)
{
	uint32_t index;

	for (index = 0u; index < count; index++) {
		samples[index] = expected_tone_sample(index, sample_bits);
	}
}

static int32_t expected_sine_sample(uint32_t index, uint32_t sample_bits)
{
	int32_t sample = sine_samples_16[(index / 2u) % SINE_PERIOD_SAMPLES];

	return (sample_bits == 16u) ? sample : (sample * 256);
}

static void fill_expected_sine(int32_t *samples, uint32_t count, uint32_t sample_bits)
{
	uint32_t index;

	for (index = 0u; index < count; index++) {
		samples[index] = expected_sine_sample(index, sample_bits);
	}
}

static uint32_t melody_note_frames(uint32_t sample_rate_hz)
{
	return (sample_rate_hz == 44100u) ? MELODY_NOTE_FRAMES_44K : MELODY_NOTE_FRAMES_48K;
}

static const uint32_t *melody_note_table(uint32_t sample_rate_hz)
{
	return (sample_rate_hz == 44100u) ? melody_notes_44k : melody_notes_48k;
}

static int32_t melody_apply_envelope(int32_t sample, uint32_t frame, uint32_t frames)
{
	uint32_t attack = frames / 20u;
	uint32_t release = frames / 8u;
	uint32_t gain = 256u;

	if ((attack > 0u) && (frame < attack)) {
		gain = (frame * 256u) / attack;
	} else if ((release > 0u) && (frame >= (frames - release))) {
		gain = ((frames - frame) * 256u) / release;
	}

	return (sample * (int32_t)gain) / 256;
}

static void fill_expected_melody(int32_t *samples, uint32_t count, uint32_t sample_bits,
	uint32_t sample_rate_hz)
{
	const uint32_t *notes = melody_note_table(sample_rate_hz);
	uint32_t note_frames = melody_note_frames(sample_rate_hz);
	uint32_t note = 0u;
	uint32_t frame = 0u;
	uint32_t phase = 0u;
	uint32_t index;
	int32_t frame_sample = 0;

	for (index = 0u; index < count; index++) {
		if ((index % 2u) == 0u) {
			int32_t sample;

			if (frame >= note_frames) {
				frame = 0u;
				phase = 0u;
				note++;
				if (note >= MELODY_NOTE_COUNT) {
					note = 0u;
				}
			}

			sample = sine_samples_16[phase >> MELODY_PHASE_SHIFT];
			sample = melody_apply_envelope(sample, frame, note_frames);
			phase += notes[note];
			if (phase >= MELODY_PHASE_PERIOD) {
				phase -= MELODY_PHASE_PERIOD;
			}
			frame++;
			frame_sample = (sample_bits == 16u) ? sample : (sample * 256);
		}
		samples[index] = frame_sample;
	}
}

static uint32_t find_pattern_alignment(const io_state_t *state, const int32_t *expected,
	uint32_t expected_count, uint32_t *input_offset, uint32_t *expected_offset)
{
	uint32_t window;
	uint32_t start;
	uint32_t offset;
	uint32_t index;

	if (!first_nonzero_input_offset(state, &start)) {
		return 0u;
	}
	window = min_u32(ALIGN_WINDOW_SAMPLES, state->input_sample_count - start);
	if ((window == 0u) || (expected_count < window)) {
		return 0u;
	}

	for (offset = 0u; offset <= (expected_count - window); offset++) {
		for (index = 0u; index < window; index++) {
			if (!samples_match(state->input_samples[start + index], expected[offset + index])) {
				break;
			}
		}
		if (index == window) {
			*input_offset = start;
			*expected_offset = offset;
			return 1u;
		}
	}

	return 0u;
}

static uint32_t find_verified_alignment(const io_state_t *state, const int32_t *expected,
	uint32_t expected_count, uint32_t *input_offset, uint32_t *expected_offset)
{
	uint32_t window;
	uint32_t start;
	uint32_t offset;
	uint32_t index;

	if (!first_nonzero_input_offset(state, &start)) {
		return 0u;
	}
	window = min_u32(ALIGN_WINDOW_SAMPLES, state->input_sample_count - start);
	if ((window == 0u) || (expected_count < window)) {
		return 0u;
	}

	for (offset = 0u; offset <= (expected_count - window); offset++) {
		uint32_t compared;

		for (index = 0u; index < window; index++) {
			if (!samples_match(state->input_samples[start + index], expected[offset + index])) {
				break;
			}
		}
		if (index != window) {
			continue;
		}
		compared = min_u32(state->input_sample_count - start, expected_count - offset);
		if (compared < VERIFY_MIN_SAMPLES) {
			continue;
		}
		for (index = 0u; index < compared; index++) {
			if (!samples_match(state->input_samples[start + index], expected[offset + index])) {
				break;
			}
		}
		if (index == compared) {
			*input_offset = start;
			*expected_offset = offset;
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
	result.expected_offset = 0u;
	result.compared = min_u32(state->output_sample_count, state->input_sample_count - result.input_offset);
	result.expected_hash = VERIFY_HASH_OFFSET_BASIS;
	result.actual_hash = VERIFY_HASH_OFFSET_BASIS;
	for (index = 0u; index < result.compared; index++) {
		int32_t expected = state->output_samples[index];
		int32_t actual = state->input_samples[result.input_offset + index];
		result.expected_hash = hash_sample(result.expected_hash, expected);
		result.actual_hash = hash_sample(result.actual_hash, actual);
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

static verify_result_t verify_input_pattern(const io_state_t *state)
{
	verify_result_t result;
	int32_t *expected;
	uint32_t expected_count;
	uint32_t index;

	memset(&result, 0, sizeof(result));
	result.first_mismatch = UINT32_MAX;
	if ((state->input_sample_overflow != 0u) || (state->input_sample_count < VERIFY_MIN_SAMPLES)) {
		return result;
	}

	expected_count = state->input_sample_count + PATTERN_ALIGN_MAX_SAMPLES;
	expected = (int32_t *)calloc(expected_count, sizeof(int32_t));
	if (expected == NULL) {
		return result;
	}
	fill_expected_pattern(expected, expected_count, state->sample_bits);
	if (!find_pattern_alignment(state, expected, expected_count,
	    &result.input_offset, &result.expected_offset)) {
		free(expected);
		return result;
	}

	result.aligned = 1u;
	result.compared = min_u32(
		state->input_sample_count - result.input_offset,
		expected_count - result.expected_offset);
	result.expected_hash = VERIFY_HASH_OFFSET_BASIS;
	result.actual_hash = VERIFY_HASH_OFFSET_BASIS;
	for (index = 0u; index < result.compared; index++) {
		int32_t expected_sample = expected[result.expected_offset + index];
		int32_t actual = state->input_samples[result.input_offset + index];
		result.expected_hash = hash_sample(result.expected_hash, expected_sample);
		result.actual_hash = hash_sample(result.actual_hash, actual);
		if (!samples_match(actual, expected_sample)) {
			if (result.first_mismatch == UINT32_MAX) {
				result.first_mismatch = index;
				result.first_expected = expected_sample;
				result.first_actual = actual;
			}
			result.mismatches++;
		}
	}

	result.passed = (result.compared >= VERIFY_MIN_SAMPLES) && (result.mismatches == 0u);
	free(expected);
	return result;
}

static verify_result_t verify_input_tone(const io_state_t *state)
{
	verify_result_t result;
	int32_t *expected;
	uint32_t expected_count;
	uint32_t index;

	memset(&result, 0, sizeof(result));
	result.first_mismatch = UINT32_MAX;
	if ((state->input_sample_overflow != 0u) || (state->input_sample_count < VERIFY_MIN_SAMPLES)) {
		return result;
	}

	expected_count = state->input_sample_count + TONE_ALIGN_MAX_SAMPLES;
	expected = (int32_t *)calloc(expected_count, sizeof(int32_t));
	if (expected == NULL) {
		return result;
	}
	fill_expected_tone(expected, expected_count, state->sample_bits);
	if (!find_pattern_alignment(state, expected, expected_count,
	    &result.input_offset, &result.expected_offset)) {
		free(expected);
		return result;
	}

	result.aligned = 1u;
	result.compared = min_u32(
		state->input_sample_count - result.input_offset,
		expected_count - result.expected_offset);
	result.expected_hash = VERIFY_HASH_OFFSET_BASIS;
	result.actual_hash = VERIFY_HASH_OFFSET_BASIS;
	for (index = 0u; index < result.compared; index++) {
		int32_t expected_sample = expected[result.expected_offset + index];
		int32_t actual = state->input_samples[result.input_offset + index];
		result.expected_hash = hash_sample(result.expected_hash, expected_sample);
		result.actual_hash = hash_sample(result.actual_hash, actual);
		if (!samples_match(actual, expected_sample)) {
			if (result.first_mismatch == UINT32_MAX) {
				result.first_mismatch = index;
				result.first_expected = expected_sample;
				result.first_actual = actual;
			}
			result.mismatches++;
		}
	}

	result.passed = (result.compared >= VERIFY_MIN_SAMPLES) && (result.mismatches == 0u);
	free(expected);
	return result;
}

static verify_result_t verify_input_sine(const io_state_t *state)
{
	verify_result_t result;
	int32_t *expected;
	uint32_t expected_count;
	uint32_t index;

	memset(&result, 0, sizeof(result));
	result.first_mismatch = UINT32_MAX;
	if ((state->input_sample_overflow != 0u) || (state->input_sample_count < VERIFY_MIN_SAMPLES)) {
		return result;
	}

	expected_count = state->input_sample_count + SINE_ALIGN_MAX_SAMPLES;
	expected = (int32_t *)calloc(expected_count, sizeof(int32_t));
	if (expected == NULL) {
		return result;
	}
	fill_expected_sine(expected, expected_count, state->sample_bits);
	if (!find_pattern_alignment(state, expected, expected_count,
	    &result.input_offset, &result.expected_offset)) {
		free(expected);
		return result;
	}

	result.aligned = 1u;
	result.compared = min_u32(
		state->input_sample_count - result.input_offset,
		expected_count - result.expected_offset);
	result.expected_hash = VERIFY_HASH_OFFSET_BASIS;
	result.actual_hash = VERIFY_HASH_OFFSET_BASIS;
	for (index = 0u; index < result.compared; index++) {
		int32_t expected_sample = expected[result.expected_offset + index];
		int32_t actual = state->input_samples[result.input_offset + index];
		result.expected_hash = hash_sample(result.expected_hash, expected_sample);
		result.actual_hash = hash_sample(result.actual_hash, actual);
		if (!samples_match(actual, expected_sample)) {
			if (result.first_mismatch == UINT32_MAX) {
				result.first_mismatch = index;
				result.first_expected = expected_sample;
				result.first_actual = actual;
			}
			result.mismatches++;
		}
	}

	result.passed = (result.compared >= VERIFY_MIN_SAMPLES) && (result.mismatches == 0u);
	free(expected);
	return result;
}

static verify_result_t verify_input_melody(const io_state_t *state, uint32_t sample_rate_hz)
{
	verify_result_t result;
	int32_t *expected;
	uint32_t expected_count;
	uint32_t index;

	memset(&result, 0, sizeof(result));
	result.first_mismatch = UINT32_MAX;
	if ((state->input_sample_overflow != 0u) || (state->input_sample_count < VERIFY_MIN_SAMPLES)) {
		return result;
	}

	expected_count = state->input_sample_count + MELODY_ALIGN_MAX_SAMPLES;
	expected = (int32_t *)calloc(expected_count, sizeof(int32_t));
	if (expected == NULL) {
		return result;
	}
	fill_expected_melody(expected, expected_count, state->sample_bits, sample_rate_hz);
	if (!find_verified_alignment(state, expected, expected_count,
	    &result.input_offset, &result.expected_offset)) {
		free(expected);
		return result;
	}

	result.aligned = 1u;
	result.compared = min_u32(
		state->input_sample_count - result.input_offset,
		expected_count - result.expected_offset);
	result.expected_hash = VERIFY_HASH_OFFSET_BASIS;
	result.actual_hash = VERIFY_HASH_OFFSET_BASIS;
	for (index = 0u; index < result.compared; index++) {
		int32_t expected_sample = expected[result.expected_offset + index];
		int32_t actual = state->input_samples[result.input_offset + index];
		result.expected_hash = hash_sample(result.expected_hash, expected_sample);
		result.actual_hash = hash_sample(result.actual_hash, actual);
		if (!samples_match(actual, expected_sample)) {
			if (result.first_mismatch == UINT32_MAX) {
				result.first_mismatch = index;
				result.first_expected = expected_sample;
				result.first_actual = actual;
			}
			result.mismatches++;
		}
	}

	result.passed = (result.compared >= VERIFY_MIN_SAMPLES) && (result.mismatches == 0u);
	free(expected);
	return result;
}

static verify_result_t verify_input_activity(const io_state_t *state)
{
	verify_result_t result;
	uint32_t index;

	memset(&result, 0, sizeof(result));
	result.first_mismatch = UINT32_MAX;
	result.compared = state->input_sample_count;
	result.actual_hash = VERIFY_HASH_OFFSET_BASIS;
	for (index = 0u; index < result.compared; index++) {
		result.actual_hash = hash_sample(result.actual_hash, state->input_samples[index]);
	}
	result.passed =
		(state->input_sample_overflow == 0u) &&
		(state->input_sample_count >= VERIFY_MIN_SAMPLES) &&
		(state->input_nonzero != 0u);
	return result;
}

static verify_result_t verify_input_silence(const io_state_t *state)
{
	verify_result_t result;
	uint32_t index;

	memset(&result, 0, sizeof(result));
	result.aligned = 1u;
	result.first_mismatch = UINT32_MAX;
	result.compared = state->input_sample_count;
	result.expected_hash = VERIFY_HASH_OFFSET_BASIS;
	result.actual_hash = VERIFY_HASH_OFFSET_BASIS;
	for (index = 0u; index < result.compared; index++) {
		int32_t actual = state->input_samples[index];
		result.expected_hash = hash_sample(result.expected_hash, 0);
		result.actual_hash = hash_sample(result.actual_hash, actual);
		if (actual != 0) {
			if (result.first_mismatch == UINT32_MAX) {
				result.first_mismatch = index;
				result.first_actual = actual;
			}
			result.mismatches++;
		}
	}
	result.passed =
		(state->input_sample_overflow == 0u) &&
		(state->input_sample_count >= VERIFY_MIN_SAMPLES) &&
		(result.mismatches == 0u);
	return result;
}

static void print_osstatus(const char *label, OSStatus status)
{
	fprintf(stderr, "%s failed: %d (0x%08x)\n", label, (int)status, (unsigned int)status);
}

static uint32_t property_is_settable(AudioDeviceID device, const AudioObjectPropertyAddress *address)
{
	Boolean settable = false;
	OSStatus status;

	if (!AudioObjectHasProperty(device, address)) {
		return 0u;
	}
	status = AudioObjectIsPropertySettable(device, address, &settable);
	return (status == noErr) && settable;
}

static void set_output_mute(AudioDeviceID device, AudioObjectPropertyElement element, UInt32 muted)
{
	AudioObjectPropertyAddress address = {
		kAudioDevicePropertyMute,
		kAudioDevicePropertyScopeOutput,
		element
	};
	OSStatus status;

	if (!property_is_settable(device, &address)) {
		return;
	}
	status = AudioObjectSetPropertyData(device, &address, 0u, NULL, sizeof(muted), &muted);
	if (status != noErr) {
		print_osstatus("AudioObjectSetPropertyData(output mute)", status);
	}
}

static void set_output_volume(AudioDeviceID device, AudioObjectPropertyElement element, Float32 volume)
{
	AudioObjectPropertyAddress address = {
		kAudioDevicePropertyVolumeScalar,
		kAudioDevicePropertyScopeOutput,
		element
	};
	OSStatus status;

	if (!property_is_settable(device, &address)) {
		return;
	}
	status = AudioObjectSetPropertyData(device, &address, 0u, NULL, sizeof(volume), &volume);
	if (status != noErr) {
		print_osstatus("AudioObjectSetPropertyData(output volume)", status);
	}
}

static void configure_output_unity(AudioDeviceID device, uint32_t output_channels)
{
	uint32_t channel;

	set_output_mute(device, kAudioObjectPropertyElementMain, 0u);
	set_output_volume(device, kAudioObjectPropertyElementMain, 1.0f);
	for (channel = 1u; channel <= output_channels; channel++) {
		set_output_mute(device, channel, 0u);
		set_output_volume(device, channel, 1.0f);
	}
}

static uint32_t set_hog_mode(AudioDeviceID device, pid_t owner)
{
	AudioObjectPropertyAddress address = {
		kAudioDevicePropertyHogMode,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMain
	};
	pid_t value = owner;
	OSStatus status;

	if (!property_is_settable(device, &address)) {
		return 0u;
	}
	status = AudioObjectSetPropertyData(device, &address, 0u, NULL, sizeof(value), &value);
	if (status != noErr) {
		print_osstatus("AudioObjectSetPropertyData(hog mode)", status);
		return 0u;
	}
	return 1u;
}

static uint32_t acquire_hog_mode(AudioDeviceID device)
{
	return set_hog_mode(device, getpid());
}

static void release_hog_mode(AudioDeviceID device)
{
	(void)set_hog_mode(device, (pid_t)-1);
}

static void print_usage(const char *program)
{
	fprintf(stderr, "usage: %s [--seconds N] [--runs N] [--rate 44100|48000] [--bits 16|24] [--verify loopback|input|pattern|tone|sine|melody|silence] [--silent-output] [--dump-input-wav FILE] [--dump-output-wav FILE] [--device NAME]\n", program);
	fprintf(stderr, "       %s [DEVICE_NAME]\n", program);
}

static int parse_seconds(const char *text, double *seconds)
{
	char *end = NULL;
	double value;

	errno = 0;
	value = strtod(text, &end);
	if ((errno != 0) || (end == text) || (*end != '\0') ||
	    (value <= 0.0) || (value > MAX_PROBE_SECONDS)) {
		return 0;
	}

	*seconds = value;
	return 1;
}

static int parse_u32_range(const char *text, uint32_t min_value, uint32_t max_value, uint32_t *value)
{
	char *end = NULL;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(text, &end, 10);
	if ((errno != 0) || (end == text) || (*end != '\0') ||
	    (parsed < (unsigned long)min_value) || (parsed > (unsigned long)max_value)) {
		return 0;
	}

	*value = (uint32_t)parsed;
	return 1;
}

static int parse_sample_rate(const char *text, uint32_t *rate_hz)
{
	uint32_t value;

	if (!parse_u32_range(text, 1u, 384000u, &value)) {
		return 0;
	}
	if ((value != 44100u) && (value != 48000u)) {
		return 0;
	}

	*rate_hz = value;
	return 1;
}

static int parse_sample_bits(const char *text, uint32_t *bits)
{
	uint32_t value;

	if (!parse_u32_range(text, 1u, 32u, &value)) {
		return 0;
	}
	if ((value != 16u) && (value != 24u)) {
		return 0;
	}

	*bits = value;
	return 1;
}

static int parse_verify_mode(const char *text, uint32_t *mode)
{
	if (strcmp(text, "loopback") == 0) {
		*mode = VERIFY_MODE_LOOPBACK;
		return 1;
	}
	if (strcmp(text, "input") == 0) {
		*mode = VERIFY_MODE_INPUT;
		return 1;
	}
	if (strcmp(text, "pattern") == 0) {
		*mode = VERIFY_MODE_PATTERN;
		return 1;
	}
	if (strcmp(text, "silence") == 0) {
		*mode = VERIFY_MODE_SILENCE;
		return 1;
	}
	if (strcmp(text, "tone") == 0) {
		*mode = VERIFY_MODE_TONE;
		return 1;
	}
	if (strcmp(text, "sine") == 0) {
		*mode = VERIFY_MODE_SINE;
		return 1;
	}
	if (strcmp(text, "melody") == 0) {
		*mode = VERIFY_MODE_MELODY;
		return 1;
	}

	return 0;
}

static const char *verify_mode_name(uint32_t mode)
{
	if (mode == VERIFY_MODE_PATTERN) {
		return "pattern";
	}
	if (mode == VERIFY_MODE_SILENCE) {
		return "silence";
	}
	if (mode == VERIFY_MODE_TONE) {
		return "tone";
	}
	if (mode == VERIFY_MODE_SINE) {
		return "sine";
	}
	if (mode == VERIFY_MODE_MELODY) {
		return "melody";
	}

	return (mode == VERIFY_MODE_INPUT) ? "input" : "loopback";
}

static uint32_t capture_capacity_for_duration(double seconds)
{
	double capacity = (seconds * (double)CAPTURE_SAMPLES_PER_SECOND) + (double)CAPTURE_SAMPLE_MARGIN;

	if (capacity > (double)UINT32_MAX) {
		return UINT32_MAX;
	}

	return (uint32_t)capacity;
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

static int set_nominal_sample_rate(AudioDeviceID device, uint32_t rate_hz)
{
	AudioObjectPropertyAddress address = {
		kAudioDevicePropertyNominalSampleRate,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMain
	};
	Float64 rate = (Float64)rate_hz;
	Float64 current = 0.0;
	UInt32 size = sizeof(rate);
	uint32_t attempt;
	OSStatus status;

	if (!AudioObjectHasProperty(device, &address)) {
		fprintf(stderr, "device has no nominal sample-rate property\n");
		return 0;
	}

	status = AudioObjectSetPropertyData(device, &address, 0u, NULL, size, &rate);
	if (status != noErr) {
		print_osstatus("AudioObjectSetPropertyData(nominal sample rate)", status);
		return 0;
	}

	for (attempt = 0u; attempt < 20u; attempt++) {
		size = sizeof(current);
		status = AudioObjectGetPropertyData(device, &address, 0u, NULL, &size, &current);
		if (status != noErr) {
			print_osstatus("AudioObjectGetPropertyData(nominal sample rate)", status);
			return 0;
		}
		if (((current + 0.5) >= (Float64)rate_hz) && ((current - 0.5) <= (Float64)rate_hz)) {
			printf("nominal_sample_rate=%.0f\n", current);
			return 1;
		}
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
	}

	printf("nominal_sample_rate=%.0f\n", current);
	return 0;
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
				fill_output_buffer(state,
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

static int run_hal_probe(AudioDeviceID device, double seconds, uint32_t sample_rate_hz,
	uint32_t sample_bits, uint32_t verify_mode, uint32_t input_channels,
	uint32_t output_channels, const char *dump_input_wav, const char *dump_output_wav,
	uint32_t run, uint32_t silent_output, probe_summary_t *summary)
{
	AudioDeviceIOProcID proc_id = NULL;
	io_state_t state;
	verify_result_t verify;
	OSStatus status;

	memset(&state, 0, sizeof(state));
	state.sample_lcg = 0x12345678u;
	state.sample_bits = sample_bits;
	state.silent_output = silent_output;
	state.capture_sample_capacity = capture_capacity_for_duration(seconds);
	state.input_samples = (int32_t *)calloc(state.capture_sample_capacity, sizeof(int32_t));
	state.output_samples = (int32_t *)calloc(state.capture_sample_capacity, sizeof(int32_t));
	if ((state.input_samples == NULL) || (state.output_samples == NULL)) {
		fprintf(stderr, "sample capture allocation failed\n");
		free(state.input_samples);
		free(state.output_samples);
		summary->failed++;
		return 0;
	}

	status = AudioDeviceCreateIOProcID(device, io_callback, &state, &proc_id);
	if (status != noErr) {
		print_osstatus("AudioDeviceCreateIOProcID", status);
		free(state.input_samples);
		free(state.output_samples);
		summary->failed++;
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
		summary->failed++;
		return 0;
	}

	CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, false);
	AudioDeviceStop(device, proc_id);
	AudioDeviceDestroyIOProcID(device, proc_id);
	if (verify_mode == VERIFY_MODE_PATTERN) {
		verify = verify_input_pattern(&state);
	} else if (verify_mode == VERIFY_MODE_TONE) {
		verify = verify_input_tone(&state);
	} else if (verify_mode == VERIFY_MODE_SINE) {
		verify = verify_input_sine(&state);
	} else if (verify_mode == VERIFY_MODE_MELODY) {
		verify = verify_input_melody(&state, sample_rate_hz);
	} else if (verify_mode == VERIFY_MODE_SILENCE) {
		verify = verify_input_silence(&state);
	} else if (verify_mode == VERIFY_MODE_INPUT) {
		verify = verify_input_activity(&state);
	} else {
		verify = verify_loopback(&state);
	}
	printf("run=%u started=1 seconds=%.3f rate=%u bits=%u callbacks=%u input_bytes=%llu output_bytes=%llu "
		"input_nonzero=%llu output_nonzero=%llu input_checksum=%llu output_checksum=%llu\n",
		run,
		seconds,
		sample_rate_hz,
		sample_bits,
		state.callbacks,
		(unsigned long long)state.input_bytes,
		(unsigned long long)state.output_bytes,
		(unsigned long long)state.input_nonzero,
		(unsigned long long)state.output_nonzero,
		(unsigned long long)state.input_checksum,
		(unsigned long long)state.output_checksum);
	if (!write_input_wav(dump_input_wav, &state, sample_rate_hz, sample_bits, input_channels)) {
		verify.passed = 0u;
	}
	if (!write_output_wav(dump_output_wav, &state, sample_rate_hz, sample_bits, output_channels)) {
		verify.passed = 0u;
	}
	printf("run=%u verify=%s mode=%s aligned=%u input_offset_samples=%u expected_offset_samples=%u compared_samples=%u "
		"mismatches=%u expected_hash=0x%016llx actual_hash=0x%016llx "
		"first_mismatch=%u expected=%d actual=%d "
		"input_samples=%u output_samples=%u input_overflow=%u output_overflow=%u\n",
		run,
		verify.passed ? "pass" : "fail",
		verify_mode_name(verify_mode),
		verify.aligned,
		verify.input_offset,
		verify.expected_offset,
		verify.compared,
		verify.mismatches,
		(unsigned long long)verify.expected_hash,
		(unsigned long long)verify.actual_hash,
		verify.first_mismatch,
		verify.first_expected,
		verify.first_actual,
		state.input_sample_count,
		state.output_sample_count,
		state.input_sample_overflow,
		state.output_sample_overflow);
	if (verify.passed) {
		summary->passed++;
	} else {
		summary->failed++;
	}
	summary->compared += verify.compared;
	summary->mismatches += verify.mismatches;
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
	const char *dump_input_wav = NULL;
	const char *dump_output_wav = NULL;
	double seconds = DEFAULT_PROBE_SECONDS;
	uint32_t runs = DEFAULT_PROBE_RUNS;
	uint32_t sample_rate_hz = DEFAULT_SAMPLE_RATE_HZ;
	uint32_t sample_bits = DEFAULT_SAMPLE_BITS;
	uint32_t rate_was_set = 0u;
	uint32_t bits_were_set = 0u;
	uint32_t verify_mode = VERIFY_MODE_LOOPBACK;
	uint32_t silent_output = 0u;
	AudioDeviceID device = kAudioObjectUnknown;
	probe_summary_t summary;
	uint32_t input_channels;
	uint32_t output_channels;
	uint32_t hog_acquired;
	int index;
	uint32_t run;

	setvbuf(stdout, NULL, _IOLBF, 0);
	for (index = 1; index < argc; index++) {
		if (strcmp(argv[index], "--seconds") == 0) {
			if (((index + 1) >= argc) || !parse_seconds(argv[index + 1], &seconds)) {
				print_usage(argv[0]);
				return 1;
			}
			index++;
		} else if (strcmp(argv[index], "--runs") == 0) {
			if (((index + 1) >= argc) ||
			    !parse_u32_range(argv[index + 1], 1u, MAX_PROBE_RUNS, &runs)) {
				print_usage(argv[0]);
				return 1;
			}
			index++;
		} else if (strcmp(argv[index], "--rate") == 0) {
			if (((index + 1) >= argc) || !parse_sample_rate(argv[index + 1], &sample_rate_hz)) {
				print_usage(argv[0]);
				return 1;
			}
			rate_was_set = 1u;
			index++;
		} else if (strcmp(argv[index], "--bits") == 0) {
			if (((index + 1) >= argc) || !parse_sample_bits(argv[index + 1], &sample_bits)) {
				print_usage(argv[0]);
				return 1;
			}
			bits_were_set = 1u;
			index++;
			} else if (strcmp(argv[index], "--verify") == 0) {
				if (((index + 1) >= argc) || !parse_verify_mode(argv[index + 1], &verify_mode)) {
					print_usage(argv[0]);
					return 1;
				}
				index++;
			} else if (strcmp(argv[index], "--silent-output") == 0) {
				silent_output = 1u;
			} else if (strcmp(argv[index], "--device") == 0) {
				if ((index + 1) >= argc) {
					print_usage(argv[0]);
				return 1;
			}
			device_name = argv[index + 1];
			index++;
		} else if (strcmp(argv[index], "--dump-input-wav") == 0) {
			if ((index + 1) >= argc) {
				print_usage(argv[0]);
				return 1;
			}
			dump_input_wav = argv[index + 1];
			index++;
		} else if (strcmp(argv[index], "--dump-output-wav") == 0) {
			if ((index + 1) >= argc) {
				print_usage(argv[0]);
				return 1;
			}
			dump_output_wav = argv[index + 1];
			index++;
		} else if ((strcmp(argv[index], "-h") == 0) || (strcmp(argv[index], "--help") == 0)) {
			print_usage(argv[0]);
			return 0;
		} else if (argv[index][0] == '-') {
			print_usage(argv[0]);
			return 1;
		} else {
			device_name = argv[index];
		}
	}

	if (rate_was_set && !bits_were_set) {
		sample_bits = (sample_rate_hz == 44100u) ? 16u : 24u;
	} else if (bits_were_set && !rate_was_set) {
		sample_rate_hz = (sample_bits == 16u) ? 44100u : 48000u;
	}
	if (((sample_rate_hz == 44100u) && (sample_bits != 16u)) ||
	    ((sample_rate_hz == 48000u) && (sample_bits != 24u))) {
		fprintf(stderr, "supported pairs are 44100/16 and 48000/24\n");
		return 1;
	}
	if (((dump_input_wav != NULL) || (dump_output_wav != NULL)) && (runs != 1u)) {
		fprintf(stderr, "--dump-*-wav requires --runs 1\n");
		return 1;
	}
	if ((verify_mode == VERIFY_MODE_LOOPBACK) && silent_output) {
		fprintf(stderr, "--silent-output is not valid with --verify loopback\n");
		return 1;
	}

	if (!find_device(device_name, &device)) {
		fprintf(stderr, "device containing \"%s\" not found\n", device_name);
		return 1;
	}
	if (!set_nominal_sample_rate(device, sample_rate_hz)) {
		fprintf(stderr, "failed to select %u Hz\n", sample_rate_hz);
		return 1;
	}
	input_channels = channel_count(device, kAudioDevicePropertyScopeInput);
	if (input_channels == 0u) {
		input_channels = 1u;
	}
	output_channels = channel_count(device, kAudioDevicePropertyScopeOutput);
	if (output_channels == 0u) {
		output_channels = 1u;
	}
	hog_acquired = acquire_hog_mode(device);
	printf("hog_mode=%u\n", hog_acquired);
	configure_output_unity(device, output_channels);

	memset(&summary, 0, sizeof(summary));
	for (run = 1u; run <= runs; run++) {
		(void)run_hal_probe(device, seconds, sample_rate_hz, sample_bits, verify_mode,
			input_channels, output_channels, dump_input_wav, dump_output_wav, run,
			silent_output, &summary);
	}
	printf("summary mode=%s rate=%u bits=%u runs=%u passed=%u failed=%u compared_samples=%llu mismatches=%llu\n",
		verify_mode_name(verify_mode),
		sample_rate_hz,
		sample_bits,
		runs,
		summary.passed,
		summary.failed,
		(unsigned long long)summary.compared,
		(unsigned long long)summary.mismatches);
	if (hog_acquired) {
		release_hog_mode(device);
	}
	return (summary.failed == 0u) ? 0 : 1;
}
