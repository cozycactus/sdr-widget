#!/usr/bin/env python3
import argparse
import sys
import wave


ALIGN_WINDOW_SAMPLES = 128
PATTERN_ALIGN_MAX_SAMPLES = 65536
TONE_ALIGN_MAX_SAMPLES = 4096
VERIFY_MIN_SAMPLES = 4096
VERIFY_HASH_OFFSET_BASIS = 1469598103934665603
VERIFY_HASH_PRIME = 1099511628211
TONE_PERIOD_SAMPLES = 96
SINE_PERIOD_SAMPLES = 96
SINE_ALIGN_MAX_SAMPLES = 4096
MELODY_ALIGN_MAX_SAMPLES = 1048576
MELODY_NOTE_FRAMES_48K = 9600
MELODY_NOTE_FRAMES_44K = 8820
SINE_SAMPLES_16 = (
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
)
MELODY_NOTES_48K = (
    34292, 43205, 51380, 68584, 64734, 51380,
    43205, 38491, 34292, 38491, 43205, 51380,
    57672, 51380, 43205, 34292, 38491, 43205,
    51380, 57672, 51380, 43205, 38491, 34292,
)
MELODY_NOTES_44K = (
    37324, 47026, 55923, 74649, 70459, 55923,
    47026, 41895, 37324, 41895, 47026, 55923,
    62772, 55923, 47026, 37324, 41895, 47026,
    55923, 62772, 55923, 47026, 41895, 37324,
)


def hash_sample(value, sample):
    sample &= 0xffffffff
    for index in range(4):
        value ^= (sample >> (index * 8)) & 0xff
        value = (value * VERIFY_HASH_PRIME) & 0xffffffffffffffff
    return value


def read_wav_samples(path):
    with wave.open(path, "rb") as wav_file:
        channels = wav_file.getnchannels()
        sample_width = wav_file.getsampwidth()
        rate = wav_file.getframerate()
        frame_count = wav_file.getnframes()
        data = wav_file.readframes(frame_count)

    if channels <= 0:
        raise ValueError("WAV has no channels")
    if sample_width not in (2, 3):
        raise ValueError(f"unsupported WAV sample width: {sample_width} bytes")

    samples = []
    step = sample_width
    for offset in range(0, len(data) - (len(data) % step), step):
        if sample_width == 2:
            samples.append(int.from_bytes(data[offset:offset + 2], "little", signed=True))
        else:
            value = data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16)
            if value & 0x800000:
                value -= 0x1000000
            samples.append(value)

    return {
        "channels": channels,
        "rate": rate,
        "bits": sample_width * 8,
        "frames": frame_count,
        "samples": samples,
    }


def next_pattern_sample(state, bits):
    value_bits = bits - 2
    midpoint = 1 << (value_bits - 1)
    mask = (1 << value_bits) - 1
    shift = 17 if bits == 16 else 9
    state[0] = ((state[0] * 1664525) + 1013904223) & 0xffffffff
    sample = ((state[0] >> shift) & mask) - midpoint
    return 1 if sample == 0 else sample


def fill_expected_pattern(count, bits):
    state = [0x12345678]
    return [next_pattern_sample(state, bits) for _ in range(count)]


def expected_tone_sample(index, bits):
    positive = (index % TONE_PERIOD_SAMPLES) < (TONE_PERIOD_SAMPLES // 2)
    if bits == 16:
        return 0x2000 if positive else -0x2000
    return 0x200000 if positive else -0x200000


def fill_expected_tone(count, bits):
    return [expected_tone_sample(index, bits) for index in range(count)]


def expected_sine_sample(index, bits):
    sample = SINE_SAMPLES_16[(index // 2) % SINE_PERIOD_SAMPLES]
    return sample if bits == 16 else sample * 256


def fill_expected_sine(count, bits):
    return [expected_sine_sample(index, bits) for index in range(count)]


def div_trunc(value, divisor):
    if value >= 0:
        return value // divisor
    return -((-value) // divisor)


def melody_note_frames(rate):
    if rate == 44100:
        return MELODY_NOTE_FRAMES_44K
    return MELODY_NOTE_FRAMES_48K


def melody_note_table(rate):
    if rate == 44100:
        return MELODY_NOTES_44K
    return MELODY_NOTES_48K


def melody_apply_envelope(sample, frame, frames):
    attack = frames // 20
    release = frames // 8
    gain = 256

    if attack > 0 and frame < attack:
        gain = (frame * 256) // attack
    elif release > 0 and frame >= frames - release:
        gain = ((frames - frame) * 256) // release

    return div_trunc(sample * gain, 256)


def fill_expected_melody(count, bits, rate):
    frames = melody_note_frames(rate)
    notes = melody_note_table(rate)
    note = 0
    frame = 0
    phase = 0
    phase_period = SINE_PERIOD_SAMPLES << 16
    frame_sample = 0
    expected = []

    for index in range(count):
        if index % 2 == 0:
            if frame >= frames:
                frame = 0
                phase = 0
                note = (note + 1) % len(notes)

            sample = SINE_SAMPLES_16[phase >> 16]
            sample = melody_apply_envelope(sample, frame, frames)
            phase += notes[note]
            if phase >= phase_period:
                phase -= phase_period
            frame += 1
            frame_sample = sample if bits == 16 else sample * 256
        expected.append(frame_sample)

    return expected


def first_nonzero_offset(samples):
    for index, sample in enumerate(samples):
        if sample != 0:
            return index
    return None


def find_expected_alignment(samples, expected):
    start = first_nonzero_offset(samples)
    if start is None:
        return None

    window = min(ALIGN_WINDOW_SAMPLES, len(samples) - start)
    if window == 0 or len(expected) < window:
        return None

    for offset in range(0, len(expected) - window + 1):
        if samples[start:start + window] == expected[offset:offset + window]:
            return start, offset
    return None


def find_verified_alignment(samples, expected):
    start = first_nonzero_offset(samples)
    if start is None:
        return None

    window = min(ALIGN_WINDOW_SAMPLES, len(samples) - start)
    if window == 0 or len(expected) < window:
        return None

    for offset in range(0, len(expected) - window + 1):
        if samples[start:start + window] != expected[offset:offset + window]:
            continue
        compared = min(len(samples) - start, len(expected) - offset)
        if compared < VERIFY_MIN_SAMPLES:
            continue
        if samples[start:start + compared] == expected[offset:offset + compared]:
            return start, offset
    return None


def find_loopback_alignment(samples, expected):
    window = min(ALIGN_WINDOW_SAMPLES, len(expected))
    if window == 0 or len(samples) < window:
        return None

    for offset in range(0, len(samples) - window + 1):
        if samples[offset:offset + window] == expected[:window]:
            return offset, 0
    return None


def verify_expected(samples, expected):
    return verify_with_alignment(samples, expected, find_expected_alignment(samples, expected))


def verify_exactly_aligned(samples, expected):
    return verify_with_alignment(samples, expected, find_verified_alignment(samples, expected))


def verify_loopback(samples, expected):
    return verify_with_alignment(samples, expected, find_loopback_alignment(samples, expected))


def verify_with_alignment(samples, expected, alignment):
    result = {
        "passed": False,
        "aligned": False,
        "input_offset": 0,
        "expected_offset": 0,
        "compared": 0,
        "mismatches": 0,
        "expected_hash": VERIFY_HASH_OFFSET_BASIS,
        "actual_hash": VERIFY_HASH_OFFSET_BASIS,
        "first_mismatch": 0xffffffff,
        "first_expected": 0,
        "first_actual": 0,
    }

    if alignment is None:
        return result

    result["aligned"] = True
    result["input_offset"], result["expected_offset"] = alignment
    result["compared"] = min(
        len(samples) - result["input_offset"],
        len(expected) - result["expected_offset"],
    )

    for index in range(result["compared"]):
        expected_sample = expected[result["expected_offset"] + index]
        actual_sample = samples[result["input_offset"] + index]
        result["expected_hash"] = hash_sample(result["expected_hash"], expected_sample)
        result["actual_hash"] = hash_sample(result["actual_hash"], actual_sample)
        if actual_sample != expected_sample:
            if result["first_mismatch"] == 0xffffffff:
                result["first_mismatch"] = index
                result["first_expected"] = expected_sample
                result["first_actual"] = actual_sample
            result["mismatches"] += 1

    result["passed"] = (
        result["compared"] >= VERIFY_MIN_SAMPLES and
        result["mismatches"] == 0
    )
    return result


def verify_silence(samples):
    result = {
        "passed": False,
        "aligned": True,
        "input_offset": 0,
        "expected_offset": 0,
        "compared": len(samples),
        "mismatches": 0,
        "expected_hash": VERIFY_HASH_OFFSET_BASIS,
        "actual_hash": VERIFY_HASH_OFFSET_BASIS,
        "first_mismatch": 0xffffffff,
        "first_expected": 0,
        "first_actual": 0,
    }

    for index, actual_sample in enumerate(samples):
        result["expected_hash"] = hash_sample(result["expected_hash"], 0)
        result["actual_hash"] = hash_sample(result["actual_hash"], actual_sample)
        if actual_sample != 0:
            if result["first_mismatch"] == 0xffffffff:
                result["first_mismatch"] = index
                result["first_actual"] = actual_sample
            result["mismatches"] += 1

    result["passed"] = (
        result["compared"] >= VERIFY_MIN_SAMPLES and
        result["mismatches"] == 0
    )
    return result


def expected_for_source(source, sample_count, bits, rate):
    if source == "pattern":
        return fill_expected_pattern(sample_count + PATTERN_ALIGN_MAX_SAMPLES, bits)
    if source == "tone":
        return fill_expected_tone(sample_count + TONE_ALIGN_MAX_SAMPLES, bits)
    if source == "sine":
        return fill_expected_sine(sample_count + SINE_ALIGN_MAX_SAMPLES, bits)
    if source == "melody":
        if rate not in (44100, 48000):
            return None
        return fill_expected_melody(sample_count + MELODY_ALIGN_MAX_SAMPLES, bits, rate)
    return None


def main():
    parser = argparse.ArgumentParser(
        description="Verify a SAME70 WAV capture from disk."
    )
    parser.add_argument("wav")
    parser.add_argument("--source", choices=("loop", "pattern", "tone", "sine", "silence", "melody"), required=True)
    parser.add_argument("--expected-wav")
    parser.add_argument("--rate", type=int)
    parser.add_argument("--bits", type=int)
    args = parser.parse_args()

    try:
        wav = read_wav_samples(args.wav)
    except (OSError, EOFError, wave.Error, ValueError) as exc:
        print(f"audio-wav-verify: failed to read WAV: {exc}", file=sys.stderr)
        return 1

    if args.rate is not None and wav["rate"] != args.rate:
        print(f"audio-wav-verify: rate is {wav['rate']}, expected {args.rate}", file=sys.stderr)
        return 1
    if args.bits is not None and wav["bits"] != args.bits:
        print(f"audio-wav-verify: bits is {wav['bits']}, expected {args.bits}", file=sys.stderr)
        return 1
    if wav["bits"] not in (16, 24):
        print(f"audio-wav-verify: unsupported bits: {wav['bits']}", file=sys.stderr)
        return 1

    samples = wav["samples"]
    if args.source == "silence":
        result = verify_silence(samples)
    elif args.source == "loop":
        if args.expected_wav is None:
            print("audio-wav-verify: --source loop requires --expected-wav", file=sys.stderr)
            return 1
        try:
            expected_wav = read_wav_samples(args.expected_wav)
        except (OSError, EOFError, wave.Error, ValueError) as exc:
            print(f"audio-wav-verify: failed to read expected WAV: {exc}", file=sys.stderr)
            return 1
        for key in ("rate", "bits", "channels"):
            if wav[key] != expected_wav[key]:
                print(
                    f"audio-wav-verify: {key} mismatch: input={wav[key]} "
                    f"expected={expected_wav[key]}",
                    file=sys.stderr,
                )
                return 1
        result = verify_loopback(samples, expected_wav["samples"])
    else:
        expected = expected_for_source(args.source, len(samples), wav["bits"], wav["rate"])
        if expected is None:
            print(f"audio-wav-verify: unsupported source: {args.source}", file=sys.stderr)
            return 1
        if args.source == "melody":
            result = verify_exactly_aligned(samples, expected)
        else:
            result = verify_expected(samples, expected)

    status = "pass" if result["passed"] else "fail"
    print(
        f"wav_verify={status} source={args.source} rate={wav['rate']} bits={wav['bits']} "
        f"channels={wav['channels']} frames={wav['frames']} samples={len(samples)} "
        f"aligned={1 if result['aligned'] else 0} "
        f"input_offset_samples={result['input_offset']} "
        f"expected_offset_samples={result['expected_offset']} "
        f"compared_samples={result['compared']} mismatches={result['mismatches']} "
        f"expected_hash=0x{result['expected_hash']:016x} "
        f"actual_hash=0x{result['actual_hash']:016x} "
        f"first_mismatch={result['first_mismatch']} "
        f"expected={result['first_expected']} actual={result['first_actual']}"
    )
    if result["passed"]:
        print("audio-wav-verify: pass")
        return 0

    print("audio-wav-verify: fail", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
