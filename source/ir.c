#include "ir.h"
#include "i2c.h"
#include <stdbool.h>
#include <stdio.h>
#include <3ds/types.h>

void ir_flush_fifo(void)
{
    for (int k = 0; k < 32; k++) {
        u8 lvl = I2C_read(REG_RXLVL);
        if (!lvl) break;
        u8 tmp[64];
        if (lvl > sizeof(tmp)) lvl = sizeof(tmp);
        I2C_readArray(REG_FIFO, tmp, lvl);
    }
}

void ir_configure_div10_now(void)
{
    // Hard stop
    I2C_write(REG_EFCR, 0x06);     // disable TX/RX
    I2C_write(REG_FCR,  0x00);     // disable FIFO
    I2C_write(REG_IER,  0x00);     // disable sleep
    I2C_write(REG_IOSTATE, 0x00);  // sane IO state

    svcSleepThread(2 * 1000 * 1000); // 2ms

    // Force 8N1 with DLAB clear
    I2C_write(REG_LCR, 0x03);

    // Program divisor = 10 (DLAB=1)
    I2C_write(REG_LCR, 0x03 | BIT(7));
    I2C_write(REG_DLL, 10);
    I2C_write(REG_DLH, 0);
    I2C_write(REG_LCR, 0x03);      // DLAB clear, 8N1

    // Re-arm FIFO + RX
    I2C_write(REG_FCR,  0x07);     // reset+enable FIFO
    I2C_write(REG_EFCR, 0x04);     // enable RX

    svcSleepThread(2 * 1000 * 1000); // 2ms
    ir_flush_fifo();

    // Do NOT change divisor, but re-write it once more to force latch on picky units
    I2C_write(REG_LCR, 0x03 | BIT(7));
    I2C_write(REG_DLL, 10);
    I2C_write(REG_DLH, 0);
    I2C_write(REG_LCR, 0x03);

    I2C_write(REG_FCR,  0x07);
    I2C_write(REG_EFCR, 0x04);
    ir_flush_fifo();
}

bool ir_init()
{
	//static bool inited = false;

	//if (inited)
	//	return false;
	//inited = true;

	I2C_init();
	ir_configure_div10_now()

	return true;
}

void ir_enable(void)
{
	// Disable sleep mode
	I2C_write(REG_IER, 0);
	// IOState must be 0
	I2C_write(REG_IOSTATE, 0);
}

void ir_disable(void)
{
	// Enable sleep mode
	I2C_write(REG_IER, BIT(4));
	I2C_write(REG_IOSTATE, BIT(0));
}

void ir_send_data(void *data, u32 size)
{
	u8 *ptr8 = (u8 *) data, txlvl;
	u32 sent;

	// Reset and enable TX FIFO
	I2C_write(REG_FCR, 0x05);
	// Enable transmitter
	I2C_write(REG_EFCR, 0x02);

	do {
		txlvl = I2C_read(REG_TXLVL);
		sent = size > txlvl ? txlvl : size;
		I2C_writeArray(REG_FIFO, ptr8, sent);
		ptr8 += sent;
		size -= sent;
	} while (size);

	// Wait until THR and TSR are empty
	while (!(I2C_read(REG_LSR) & BIT(6)));

	// Disable transmitter and receiver
	I2C_write(REG_EFCR, 0x06);
	// Disable FIFO
	I2C_write(REG_FCR, 0);
}

u32 ir_recv_data(void *data, u32 size)
{
	u8 *ptr8 = (u8 *) data, rxlvl;
	u16 i, timeout = RX_TIMEOUT;
	u32 tc = 0;
	bool loop = true;

	// Reset and enable RX FIFO
	I2C_write(REG_FCR, 0x03);
	// Enable receiver
	I2C_write(REG_EFCR, 0x04);

	do {
		i = 0;
		// 10 cycles ~ 1ms (?)
		while (!(rxlvl = I2C_read(REG_RXLVL)) && i < timeout)
			i++;
		if (i == timeout)
			break;
		timeout = RX_MAX_WAIT;

		if (tc + rxlvl > size) {
			rxlvl = size - tc;
			loop = false;
		}
		I2C_readArray(REG_FIFO, ptr8 + tc, rxlvl);
		tc += rxlvl;
	} while (loop && tc < 136);

	// Disable transmitter and receiver
	I2C_write(REG_EFCR, 0x06);
	// Disable FIFO
	I2C_write(REG_FCR, 0);

	return tc;
}

void ir_rx_begin(void)
{
    // Reset and enable RX FIFO
    I2C_write(REG_FCR, 0x03);
    // Enable receiver
    I2C_write(REG_EFCR, 0x04);
}

u8 ir_rx_available(void)
{
    return I2C_read(REG_RXLVL);
}

u32 ir_rx_read(void *data, u32 max)
{
    u8 lvl = I2C_read(REG_RXLVL);
    if (!lvl) return 0;

    if (lvl > max) lvl = (u8)max;
    I2C_readArray(REG_FIFO, (u8 *)data, lvl);
    return lvl;
}

void ir_rx_end(void)
{
    // Disable transmitter and receiver
    I2C_write(REG_EFCR, 0x06);
    // Disable FIFO
    I2C_write(REG_FCR, 0);
}