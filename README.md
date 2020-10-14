# ReSerial

Replay Serial messages from an ESP32

## SERT File format

Serial messages are read from a file called `recording.sert` with a custom binary format that has the following header:

```c
struct ReplayHeader
{
    char magic[4];
    unsigned long time;
    unsigned long time2;
    uint32_t length;
};
```

Where:

- `magic` is `"SERT"`
- `time` is a `uint64_t` that is the number of milliseconds from the start of the recording that the message was spread out over two `uint32_t`s to save space from boundary aligning. `time2` is currently ignored.
- `length` is the number of bytes of the actual serial message, which is the distance from the end of this header to the start of the next.

## Usage

Open with platformio and hit upload
