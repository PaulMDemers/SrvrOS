#ifndef SRVROS_LPSS_SPI_H
#define SRVROS_LPSS_SPI_H

#include <stdbool.h>
#include <stdint.h>

bool lpss_spi_init(void);
bool lpss_spi_available(void);
void lpss_spi_print_status(void);
void lpss_spi_print_registers(void);

#endif
