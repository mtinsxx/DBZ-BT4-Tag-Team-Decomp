#include "bt4_sdk_cri.h"
#include "bt4_guest_abi.h"
#include "bt4_sdk_fields.h"
#include <assert.h>
#include <pthread.h>
#include <stddef.h>
#include <string.h>

void *Port_Window;
uint32_t D_00321820[2], D_00321828[2], D_00321830[2];
uint32_t D_002E4440, D_002E4444;
char D_002E4488[128];
/* Synthetic message resources occupy the same symbol contracts, without
   shipping any string/data resource from the owner's executable. */
const char D_002F7160[]="destroy-null", D_002F7170[]=" missing", D_002F7190[]="destroy-unused", D_002F71A0[]=" unused";
const char D_002F71C0[]="uuid-null", D_002F71D0[]="uuid-unused", D_002F71E0[]="callback-null", D_002F71F0[]="callback-unused";
const char D_002F7200[]="reset-null", D_002F7210[]="reset-unused", D_002F7220[]="get-null", D_002F7230[]="get-unused";
const char D_002F7240[]="put-null", D_002F7250[]="put-unused", D_002F7260[]="unget-null", D_002F7270[]="unget-unused";
const char D_002F7280[]="can-get-null", D_002F7290[]="can-get-unused";
const char D_002F72D0[]=" missing", D_002F7330[]="ring-destroy-null", D_002F7340[]="ring-destroy-unused", D_002F7350[]=" unused";
const char D_002F7370[]="ring-reset-null", D_002F7380[]="ring-reset-unused", D_002F7390[]="ring-get-null", D_002F73A0[]="ring-get-unused";
const char D_002F73B0[]="ring-get-size", D_002F73C0[]=" invalid-size", D_002F73D8[]="ring-put-null", D_002F73E8[]="ring-put-unused";
const char D_002F73F8[]="ring-unget-null", D_002F7408[]="ring-unget-unused", D_002F7418[]="ring-unget-size", D_002F7428[]="ring-can-null", D_002F7438[]="ring-can-unused";
const char D_002F7468[]="queue-destroy-null", D_002F7478[]=" missing", D_002F7498[]="queue-destroy-unused", D_002F74A8[]=" unused";
const char D_002F74C8[]="queue-uuid-null", D_002F74D8[]="queue-uuid-unused", D_002F74E8[]="queue-callback-null", D_002F74F8[]="queue-callback-unused";
const char D_002F7508[]="queue-reset-null", D_002F7518[]="queue-reset-unused", D_002F7528[]="queue-size-null", D_002F7538[]="queue-size-unused";
const char D_002F7548[]="queue-get-null", D_002F7558[]="queue-get-unused", D_002F7568[]="queue-put-null", D_002F7578[]="queue-put-unused";
const char D_002F7588[]="queue-unget-null", D_002F7598[]="queue-unget-unused", D_002F75A8[]="queue-can-null", D_002F75B8[]="queue-can-unused";
uint32_t D_002D3FE4, D_002D3FE8, D_002D97F4, D_002D97F8, D_002E6F5C, D_002E6F60;

static Bt4SjMem object;
static Bt4SjRbf ring;
static Bt4SjUni queue;
static Bt4SjUniNode pool[4];
_Alignas(16) static unsigned char ring_data[64];
static int errors, logs, enters, leaves, marker;
static int32_t last_code;

static void object_error(void *context, int32_t code) {
    assert(context == &marker);
    errors++;
    last_code = code;
    /* An error callback may re-enter the same object's interface. */
    assert(func_00282758(&object) == object.uuid);
}
static void integer_error(uint32_t context, int32_t code) {
    assert(context == 0xF1234567u);
    errors++;
    last_code = code;
}
static void logger(void *context, const char *message) {
    assert(context == &marker && message == D_002E4488);
    logs++;
}
static void integer_logger(uint32_t context, uint32_t message) {
    assert(context == 0xE1234567u && message == Bt4Android_EncodePointer(D_002E4488));
    logs++;
}
static void enter(void *context) { assert(context == &marker); enters++; }
static void leave(void *context) { assert(context == &marker); leaves++; }

static void reset(void) {
    memset(&object, 0, sizeof(object));
    object.used = 1;
    object.uuid = 0xFABCDE01u;
    object.data = 0x8FFF0000u;
    object.capacity = 100;
    func_00282898(&object);
}
typedef struct Consumer { unsigned bytes; } Consumer;
static void *consume(void *opaque) {
    Consumer *consumer = opaque;
    unsigned i;
    for (i = 0; i < 1500; ++i) {
        Bt4SjChunk chunk;
        func_00282978(&object, 1, 1, &chunk);
        consumer->bytes += (unsigned)chunk.size;
    }
    return NULL;
}

static unsigned char *pointer(uint32_t address) {
    return (unsigned char *)((uintptr_t)Port_Window + address);
}
static void test_ring(void) {
    Bt4SjChunk produced, consumed, before;
    int32_t count;
    unsigned i;
    int before_errors;
    memset(&ring, 0, sizeof(ring));
    memset(ring_data, 0xAA, sizeof(ring_data));
    ring.used = 1;
    ring.uuid = 0xF1234567u;
    ring.data = Bt4Android_EncodePointer(ring_data);
    ring.capacity = 16;
    ring.extra = 4;
    func_00283338(&ring, Bt4Android_EncodePointer((void *)integer_error), 0xF1234567u);
    func_00283348(&ring);
    assert(ring.writable == 16 && ring.readable == 0 && func_00283330(&ring) == ring.uuid);
    assert(func_00283A48(&ring, 0, 12, &count) == 1 && count == 12);
    func_00283448(&ring, 0, 12, &produced);
    assert(produced.data == ring.data && produced.size == 12 && ring.write_position == 12 && ring.writable == 4);
    for (i = 0; i < 12; ++i) pointer(produced.data)[i] = (unsigned char)i;
    func_00283648(&ring, 1, &produced);
    assert(ring.readable == 12 && ring.put_read == 12);
    assert(memcmp(ring_data, ring_data + 16, 4) == 0);
    func_00283448(&ring, 1, 10, &consumed);
    assert(consumed.size == 10 && ring.read_position == 10 && ring.readable == 2);
    for (i = 0; i < 10; ++i) assert(pointer(consumed.data)[i] == i);
    func_00283648(&ring, 0, &consumed);
    assert(ring.writable == 14 && ring.put_write == 10);
    func_00283448(&ring, 0, 10, &produced);
    assert(produced.data == ring.data + 12 && produced.size == 8 && ring.write_position == 4);
    for (i = 0; i < 8; ++i) pointer(produced.data)[i] = (unsigned char)(12 + i);
    func_00283648(&ring, 1, &produced);
    assert(ring.readable == 10 && ring.put_read == 20 && ring_data[0] == 16 && ring_data[3] == 19);
    func_00283448(&ring, 1, 10, &consumed);
    for (i = 0; i < 10; ++i) assert(pointer(consumed.data)[i] == 10 + i);
    func_00283648(&ring, 0, &consumed);
    assert(ring.writable == 16 && ring.readable == 0);
    func_00283448(&ring, 0, 3, &produced);
    func_00283818(&ring, 0, &produced);
    assert(ring.write_position == 4 && ring.writable == 16 && ring.got_write == 20);
    produced.data = ring.data + 7;
    produced.size = 2;
    before_errors = errors;
    func_00283818(&ring, 0, &produced);
    assert(errors == before_errors + 1 && ring.write_position == 4 && ring.writable == 16 && ring.got_write == 18);
    before = produced;
    func_00283448(&ring, 27, 2, &produced);
    assert(produced.size == 0 && produced.data == 0 && last_code == -3);
    produced = before;
    ring.capacity = 0;
    func_00283448(&ring, 0, 2, &produced);
    assert(memcmp(&produced, &before, sizeof(before)) == 0);
    assert(strcmp(D_002E4488, "ring-get-size invalid-size") == 0);
    ring.capacity = 16;
    assert(func_002833F8(&ring, 0) == ring.writable && func_002833F8(&ring, 1) == ring.readable);
    func_00283278(&ring);
    {
        Bt4SjRbf zero = {0};
        assert(memcmp(&ring, &zero, sizeof(ring)) == 0);
    }
    /* Guard the memory surrounding the ring/mirror storage. */
    for (i = 20; i < sizeof(ring_data); ++i) assert(ring_data[i] == 0xAA);
}

static void test_queue(void) {
    Bt4SjChunk chunk, output, before;
    unsigned i;
    int32_t amount;
    int before_errors;
    memset(&queue, 0, sizeof(queue));
    memset(pool, 0xA5, sizeof(pool));
    queue.used = -1;
    queue.uuid = 0xF0001234u;
    queue.pool = Bt4Android_EncodePointer(pool);
    queue.count = 4;
    func_00283FE0(&queue, Bt4Android_EncodePointer((void *)integer_error), 0xF1234567u);
    func_00284090(&queue);
    assert(func_00283F50(&queue) == queue.uuid && queue.free_head == queue.pool);
    for (i = 0; i < 4; ++i) {
        assert(pool[i].reserved == 0xA5A5A5A5u && pool[i].chunk.data == 0 && pool[i].chunk.size == 0);
        assert(pool[i].next == (i < 3 ? queue.pool + 16 * (i + 1) : 0));
        chunk.data = 0x80000000u + i * 100;
        chunk.size = 10 + (int32_t)i;
        func_00284448(&queue, (int32_t)i, &chunk);
        assert(func_00284198(&queue, (int32_t)i) == chunk.size);
    }
    assert(queue.free_head == 0);
    chunk.data = 123;
    chunk.size = 5;
    before_errors = errors;
    func_00284448(&queue, 0, &chunk);
    assert(errors == before_errors + 1 && func_00284198(&queue, 0) == 10);
    assert(chunk.data == 123 && chunk.size == 5); /* exhausted pool never consumes input */
    assert(func_00284730(&queue, 0, 5, &amount) == 0 && amount == 10);
    output.data = 456; output.size = 789;
    func_00284298(&queue, 0, 5, &output);
    assert(output.data == 0 && output.size == 0); /* indivisible head */
    queue.split = 1;
    assert(func_00284730(&queue, 0, 5, &amount) == 1 && amount == 10);
    func_00284298(&queue, 0, 5, &output);
    assert(output.data == 0x80000000u && output.size == 5 && func_00284198(&queue, 0) == 5);
    func_002845C0(&queue, 0, &output); /* merge prefix without a free node */
    assert(func_00284198(&queue, 0) == 10 && queue.free_head == 0);
    func_00284298(&queue, 0, 10, &output);
    assert(output.size == 10 && queue.heads[0] == 0 && queue.free_head != 0);
    assert(func_00284730(&queue, 0, 0, &amount) == 0 && amount == 0);
    func_002845C0(&queue, 0, &output);
    chunk.data = output.data + (uint32_t)output.size; chunk.size = 8;
    func_00284448(&queue, 0, &chunk); /* merge suffix, still no free node */
    assert(func_00284198(&queue, 0) == 18 && queue.free_head == 0);
    func_00284298(&queue, 0, -1, &output);
    assert(output.size == -1 && func_00284198(&queue, 0) == 19);
    before = chunk;
    before_errors = errors;
    func_00284448(&queue, -1, &chunk);
    assert(errors == before_errors + 1 && memcmp(&chunk, &before, sizeof(before)) == 0);
    func_00284298(&queue, 4, 1, &output);
    assert(output.data == 0 && output.size == 0);
    queue.used = 0;
    amount = 567;
    assert(func_00284730(&queue, 0, 10, &amount) == 0 && amount == 567);
    queue.used = 1;
    func_00284090(&queue);
    queue.count = 0;
    pool[0].next = 123;
    func_00284090(&queue);
    assert(queue.free_head == queue.pool && pool[0].next == 0);
    func_00283E98(&queue);
    {
        Bt4SjUni zero = {0};
        assert(memcmp(&queue, &zero, sizeof(queue)) == 0);
    }
}

int main(void) {
    Bt4SjChunk chunk, untouched;
    int32_t amount;
    Bt4AbiEntry entries[] = {
        {(void *)object_error, "vpi"}, {(void *)integer_error, "vii"},
        {(void *)logger, "vpp"}, {(void *)integer_logger, "vii"},
        {(void *)enter, "vp"}, {(void *)leave, "vp"}
    };
    Bt4AbiModule module = {NULL, entries, sizeof(entries)/sizeof(entries[0])};
    Port_Window = (void *)((uintptr_t)&object - 0x20000000u);
    Bt4Android_RegisterSignatures(&module);
    D_00321820[0] = Bt4Android_EncodePointer((void *)enter);
    D_00321828[0] = Bt4Android_EncodePointer((void *)leave);
    D_00321820[1] = D_00321828[1] = Bt4Android_EncodePointer(&marker);
    D_00321830[0] = Bt4Android_EncodePointer((void *)logger);
    D_00321830[1] = Bt4Android_EncodePointer(&marker);
    reset();
    assert(object.available == 100 && object.position == 0);
    assert(func_00282758(&object) == object.uuid);
    func_002827E8(&object, Bt4Android_EncodePointer((void *)object_error), Bt4Android_EncodePointer(&marker));
    assert(func_00282930(&object, 1) == 100 && func_00282930(&object, 0) == 0);
    assert(func_00282CF8(&object, 1, 120, &amount) == 0 && amount == 100);
    assert(func_00282CF8(&object, 1, 40, &amount) == 1 && amount == 40);
    func_00282978(&object, 1, 40, &chunk);
    assert(chunk.data == object.data && chunk.size == 40 && object.position == 40 && object.available == 60);
    func_00282BA8(&object, 1, &chunk);
    assert(object.position == 0 && object.available == 100 && errors == 0);
    untouched = chunk;
    func_00282AB8(&object, 1, &chunk);
    assert(memcmp(&chunk, &untouched, sizeof(chunk)) == 0);
    func_00282BA8(&object, 0, &chunk);
    assert(errors == 1 && last_code == -3);
    func_00282AB8(&object, 7, &chunk);
    assert(chunk.data == 0 && chunk.size == 0 && errors == 2);
    func_00282978(&object, 0, 40, &chunk);
    assert(chunk.data == 0 && chunk.size == 0 && object.available == 100);
    func_00282978(&object, 1, 120, &chunk);
    assert(chunk.size == 100 && object.available == 0 && object.position == 100);
    func_00282978(&object, 1, 1, &chunk);
    assert(chunk.size == 0 && chunk.data == object.data + 100);
    /* Signed request and wrapping address semantics are observable original behavior. */
    func_00282978(&object, 1, -1, &chunk);
    assert(chunk.size == -1 && object.available == 1 && object.position == 99);
    chunk.data = 123;
    chunk.size = 6;
    func_00282BA8(&object, 17, &chunk);
    assert(chunk.data == 0 && chunk.size == 0 && errors == 3);
    func_002827E8(&object, Bt4Android_EncodePointer((void *)integer_error), 0xF1234567u);
    assert(func_00282930(&object, -1) == 0 && errors == 4);
    chunk.data = 0x12345678u;
    chunk.size = 123;
    untouched = chunk;
    func_00282978(NULL, 1, 5, &chunk);
    assert(memcmp(&chunk, &untouched, sizeof(chunk)) == 0 && logs == 1);
    assert(strcmp(D_002E4488, "get-null missing") == 0);
    D_00321830[0] = Bt4Android_EncodePointer((void *)integer_logger);
    D_00321830[1] = 0xE1234567u;
    object.used = 0;
    amount = 123;
    assert(func_00282CF8(&object, 1, 5, &amount) == 0 && amount == 123 && logs == 2);
    assert(strcmp(D_002E4488, "can-get-unused unused") == 0);
    reset();
    func_002826A0(&object);
    {
        Bt4SjMem zero = {0};
        assert(memcmp(&object, &zero, sizeof(object)) == 0);
    }
    assert(enters == leaves && D_002E4440 == 0 && D_002E4444 == 0);
    test_ring();
    test_queue();
    assert(enters == leaves && D_002E4440 == 0);
    /* The Android adapter must also serialize independent native threads. */
    D_00321820[0] = D_00321828[0] = 0;
    reset();
    object.capacity = object.available = 2000;
    {
        Consumer a = {0}, b = {0};
        pthread_t first, second;
        assert(pthread_create(&first, NULL, consume, &a) == 0);
        assert(pthread_create(&second, NULL, consume, &b) == 0);
        assert(pthread_join(first, NULL) == 0 && pthread_join(second, NULL) == 0);
        assert(a.bytes + b.bytes == 2000 && object.available == 0 && object.position == 2000);
    }
    return 0;
}
