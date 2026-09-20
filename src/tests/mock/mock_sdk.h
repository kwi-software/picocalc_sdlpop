#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef struct { volatile uint32_t dr, icr; } spi_hw_t;
extern spi_hw_t mock_spi;
#define spi1 (&mock_spi)
#define SPI_CPOL_0 0
#define SPI_CPHA_0 0
#define SPI_MSB_FIRST 0
#define DMA_SIZE_8 0
#define DMA_SIZE_16 1
#define GPIO_OUT 1
#define GPIO_FUNC_SPI 2
#define SPI_SSPICR_RORIC_BITS 1
typedef struct { unsigned size; bool inc; } dma_channel_config;
void gpio_put(unsigned, bool);
void gpio_init(unsigned);
void gpio_set_dir(unsigned, bool);
void gpio_set_function(unsigned, unsigned);
void sleep_ms(unsigned);
void busy_wait_us(unsigned);
void tight_loop_contents(void);
unsigned spi_init(spi_hw_t *, unsigned);
void spi_set_format(spi_hw_t *, unsigned, unsigned, unsigned, unsigned);
int spi_write_blocking(spi_hw_t *, const uint8_t *, size_t);
bool spi_is_readable(spi_hw_t *);
bool spi_is_busy(spi_hw_t *);
spi_hw_t *spi_get_hw(spi_hw_t *);
unsigned spi_get_dreq(spi_hw_t *, bool);
int dma_claim_unused_channel(bool);
dma_channel_config dma_channel_get_default_config(unsigned);
void channel_config_set_transfer_data_size(dma_channel_config *, unsigned);
void channel_config_set_read_increment(dma_channel_config *, bool);
void channel_config_set_write_increment(dma_channel_config *, bool);
void channel_config_set_dreq(dma_channel_config *, unsigned);
void dma_channel_configure(unsigned, const dma_channel_config *, volatile void *, const volatile void *, unsigned, bool);
void dma_start_channel_mask(unsigned);
void dma_channel_wait_for_finish_blocking(unsigned);
