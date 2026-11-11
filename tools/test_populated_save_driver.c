/* Disposable file backend and host-clock control for the populated replay. */
#include "test_reserved_append_driver.c"
void profile_set_realtime(uint64_t value) {
    assert(value <= INT64_MAX);
    mock_realtime = (int64_t)value;
}
