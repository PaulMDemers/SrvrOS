#pragma once
/* Only the isolated read-enabled image groups trusted preparation code. */
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
#define AIR61_SAMPLE_CODE __attribute__((section(".text.air61_sample")))
#else
#define AIR61_SAMPLE_CODE
#endif
