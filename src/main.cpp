#include <Arduino.h>

#define PBX_BAUD_RATE 2000000L
#define DEBUG_BAUD_RATE 115200
#define REPLAY_MAGIC "SERT"
#define PBX_MAGIC "UPXL"
#define SCALE_FACTOR 1.5

#ifndef LED_BUILTIN // for ESP32
#define LED_BUILTIN 2
#endif

// #define DEBUG
#ifdef DEBUG
char debug_msg[512];
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINT(x) Serial.print(x)
// #define DEBUG_PRINTLN(x)
// #define DEBUG_PRINT(x)
#else
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINT(x)
#endif

unsigned long start_time;
unsigned long delta_time = 0;

char *replay_ptr;
char *last_wait;

struct ReplayHeader
{
    char magic[4];
    unsigned long time;
    unsigned long time2;
    uint32_t length;
};

struct ReplayHeader header;

extern const uint8_t replay_buffer_start[] asm("_binary_data_recording_sert_start");
extern const uint8_t replay_buffer_end[] asm("_binary_data_recording_sert_end");

char* replay_buffer = (char *)replay_buffer_start;
const long replay_size = (replay_buffer_end - replay_buffer_start);

#ifdef DEBUG
void hexdump(char *buff, int count)
{
    char *ptr = buff;
    int index = 0;
    while (index < count)
    {
        sprintf(debug_msg, "%02x", *(ptr + index));
        DEBUG_PRINT(debug_msg);
        index++;
    }
    DEBUG_PRINT(" ( '");
    index = 0;
    unsigned char current;
    while (index < count)
    {
        current = (unsigned char)*(ptr + index);
        if (0x20 <= current && current < 0x7f)
        {
            sprintf(debug_msg, "%c", *(ptr + index));
            DEBUG_PRINT(debug_msg);
        }
        else
        {
            DEBUG_PRINT(".");
        }
        index++;
    }
    sprintf(debug_msg, "', %d bytes )", count);
    DEBUG_PRINTLN(debug_msg);
}

void ptrdump(void *ptr) {
    sprintf(debug_msg, "%08p", ptr);
    DEBUG_PRINTLN(debug_msg);
}

void dump_header(ReplayHeader header)
{
    DEBUG_PRINTLN("ReplayHeader:");
    DEBUG_PRINT("-> header.magic: ");
    hexdump(&(header.magic[0]), sizeof(header.magic));
    sprintf(debug_msg, "-> header.time: %lu", header.time);
    DEBUG_PRINTLN(debug_msg);
    sprintf(debug_msg, "-> header.time2: %lu", header.time2);
    DEBUG_PRINTLN(debug_msg);
    sprintf(debug_msg, "-> header.length: %u", header.length);
    DEBUG_PRINTLN(debug_msg);
    hexdump((char *)&header, sizeof(header));
}

void dump_replay_ptr()
{
    sprintf(debug_msg, "replay_ptr: %p (replay_buffer + %d)", replay_ptr, (replay_ptr - &(replay_buffer[0])));
    DEBUG_PRINTLN(debug_msg);
}
#endif

long int scaled_millis() {
    return (long int)(millis() / SCALE_FACTOR);
}

void reset()
{
    replay_ptr = &(replay_buffer[0]);
    last_wait = replay_ptr;
    start_time = scaled_millis();
#ifdef DEBUG
    DEBUG_PRINTLN("resetting");
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
    digitalWrite(LED_BUILTIN, HIGH);
#endif
}

void setup()
{
    Serial.begin(DEBUG_BAUD_RATE);
    Serial2.begin(PBX_BAUD_RATE,  SERIAL_8N1);


#ifdef DEBUG
    delay(1000);
    pinMode(LED_BUILTIN, OUTPUT);
    DEBUG_PRINTLN("Serial initialised");
    sprintf(debug_msg, "sizeof ReplayHeader: %d", sizeof(ReplayHeader));
    DEBUG_PRINTLN(debug_msg);
    sprintf(debug_msg, "-> sizeof header.magic: %d", sizeof(header.magic));
    DEBUG_PRINTLN(debug_msg);
    sprintf(debug_msg, "-> sizeof header.time: %d", sizeof(header.time));
    DEBUG_PRINTLN(debug_msg);
    sprintf(debug_msg, "-> sizeof header.time2: %d", sizeof(header.time));
    DEBUG_PRINTLN(debug_msg);
    sprintf(debug_msg, "-> sizeof header.length: %d", sizeof(header.length));
    DEBUG_PRINTLN(debug_msg);
    sprintf(debug_msg, "sizeof replay_buffer: %d", sizeof(replay_buffer));
    DEBUG_PRINTLN(debug_msg);
    DEBUG_PRINT("ptr replay_buffer_start @");
    ptrdump((void *)replay_buffer_start);
    DEBUG_PRINT("ptr replay_buffer_end @");
    ptrdump((void *)replay_buffer_end);
    sprintf(debug_msg, "replay_buffer_len: %d", (replay_buffer_end - replay_buffer_start));
    DEBUG_PRINT("replay_buffer_len: ");
    // hexdump((char *)replay_buffer_start, (replay_buffer_end - replay_buffer_start));
#endif

    reset();
}

void loop()
{
#ifdef DEBUG
    DEBUG_PRINTLN("looping");
    dump_replay_ptr();
#endif
    // check if we have reached the end of replay_buffer
    if (replay_ptr - &(replay_buffer[0]) >= replay_size)
    {
        DEBUG_PRINTLN("reached end of buffer");
        reset();
        return;
    }

    // read header
    memcpy(&header, replay_ptr, sizeof(ReplayHeader));
    replay_ptr += sizeof(ReplayHeader);
#ifdef DEBUG
    dump_header(header);
    dump_replay_ptr();
#endif

    // check that magic string at start of replay header is what we expect
    if (!(strncmp(header.magic, REPLAY_MAGIC, 4) == 0))
    {
        DEBUG_PRINTLN("header not start with magic.");
        reset();
        return;
    }

    // wait until enough time has passed
    do
    {
        delta_time = (scaled_millis() - start_time);
#ifdef DEBUG
        if (last_wait != replay_ptr)
        {
            sprintf(debug_msg, "waiting, delta_time: %lu < header.time: %lu\n", delta_time, header.time);
            DEBUG_PRINTLN(debug_msg);
            last_wait = replay_ptr;
        }
#endif
        delay(1);
    } while (delta_time < header.time);
    DEBUG_PRINTLN("enough time has passed.");

    // Write the data
#ifdef DEBUG
    DEBUG_PRINT("writing: ");
    hexdump(replay_ptr, header.length);
#endif
    int bytes_written = Serial2.write((const uint8_t *)replay_ptr, (size_t)header.length);
    if (bytes_written != header.length)
    {
#ifdef DEBUG
        sprintf(debug_msg, "did not write %d bytes, instead wrote %d.\n", header.length, bytes_written);
        DEBUG_PRINTLN(debug_msg);
#endif
        // Serial2.drain();
        // reset();
        // return;
    }
    replay_ptr += header.length;
#ifdef DEBUG
    // sprintf(debug_msg, "delta_time: %lu header.time: %lu\n", delta_time, header.time);
    // Serial.write(debug_msg);
    dump_replay_ptr();
#endif
}
