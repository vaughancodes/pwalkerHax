#include "ir.h"
#include "ui.h"
#include "updates.h"
#include <stdio.h>
#include <stdlib.h>
#include <3ds.h>

int main(int argc, char* argv[])
{
	enum operation op;
	s32 prio;

	svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);

	gfxInitDefault();

	ui_init();
	ir_init();
	ir_enable();

	ui_draw();
	threadCreate((ThreadFunc) updates_check, (void *) VER, 1024, prio - 1, -2, true);
	
	u8 buf[64];
	
	while (aptMainLoop()) {
		op = ui_update();
		if (op == OP_EXIT)
			break;
		else if (op == OP_UPDATE)
			ui_draw();		

        u32 n = ir_rx_read(buf, sizeof(buf));
        if (n) {
            for (u32 i = 0; i < n; i++) {
                printf("%02X ", buf[i]);
            }
            printf("\n");

            // Also show XOR-decoded view
            for (u32 i = 0; i < n; i++) {
                printf("%02X ", (u8)(buf[i] ^ 0xAA));
            }
            printf("\n");
        }

        // small sleep so printing doesn’t starve everything
        svcSleepThread(1 * 1000 * 1000); // 1ms
	}

    ir_rx_end();
    ir_disable();
	ui_exit();
	gfxExit();
	return 0;
}
