// make && gcc -g -ggdb -o test test.c -ldiscferret -lusb-1.0 -L. && ./test
#include <stdio.h>
#include <malloc.h>
#include "src/discferret.h"

int main(void)
{
	printf("init: %d\n", discferret_init());

	printf("find null: %d\n", discferret_find_devices(NULL));

	DISCFERRET_DEVICE *x;
	int i, j;
	printf("find: %d\n", i = discferret_find_devices(&x));
	if (i > 0) {
		for (j=0; j<i; j++) {
			printf("\tvid %04X, pid %04X", x[j].vid, x[j].pid);
			if (x[j].manufacturer[0]) printf(", mfg '%s'", x[j].manufacturer);
			if (x[j].productname[0]) printf(", prod '%s'", x[j].productname);
			if (x[j].serialnumber[0]) printf(", s/n '%s'", x[j].serialnumber);
			printf("\n");
		}
	}

	printf("openfirst null: %d\n", discferret_open_first(NULL));

	DISCFERRET_DEVICE_HANDLE *devh;
	printf("openfirst valid: %d\n", discferret_open_first(&devh));
	printf("close: %d\n", discferret_close(devh));

	printf("open serial FRED: %d\n", discferret_open("FRED", &devh));
	printf("close: %d\n", discferret_close(devh));

	printf("open serial TARKA: %d\n", discferret_open("TARKA", &devh));

	// do some tests here
	DISCFERRET_DEVICE_INFO info;
	printf("getinfo: %d\n", discferret_get_info(devh, &info));
	printf("\tfw ver:  %04X\n", info.firmware_ver);
	printf("\thw rev:  %s\n", info.hardware_rev);
	printf("\tmctype:  %04X\n", info.microcode_type);
	printf("\tmc ver:  %04X\n", info.microcode_ver);
	printf("\tmfg:     %s\n", info.manufacturer);
	printf("\tproduct: %s\n", info.productname);
	printf("\tserial#: %s\n", info.serialnumber);
	printf("\n");

	// reset fpga
	printf("poll fpga status: %d\n", discferret_fpga_get_status(devh));
	printf("fpga init: %d\n", discferret_fpga_load_begin(devh));
	printf("poll fpga status: %d\n", discferret_fpga_get_status(devh));

	// load fpga
	FILE *fp = fopen("microcode.rbf", "rb");
	fseek(fp, 0, SEEK_END);
	size_t len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	unsigned char *rbfdata = malloc(len);
	fread(rbfdata, 1, len, fp);
	printf("load fpga mcode: %d\n", discferret_fpga_load_rbf(devh, rbfdata, len));
	printf("poll fpga status: %d\n", discferret_fpga_get_status(devh));
	free(rbfdata);

	// get ram addr
	printf("get ram addr: %d\n", discferret_ram_addr_get(devh));
	printf("set ram addr 1234: %d\n", discferret_ram_addr_set(devh, 1234));
	printf("get ram addr: %d\n", discferret_ram_addr_get(devh));
	printf("set ram addr 0: %d\n", discferret_ram_addr_set(devh, 0));
	printf("get ram addr: %d\n", discferret_ram_addr_get(devh));

	// register poke/peek
	printf("poke scratchpad DA: %d\n", discferret_reg_poke(devh, DISCFERRET_R_SCRATCHPAD, 0xDA));
	printf("peek scratchpad: %02X\n", discferret_reg_peek(devh, DISCFERRET_R_SCRATCHPAD));
	printf("peek invsp:      %02X\n", discferret_reg_peek(devh, DISCFERRET_R_INVERSE_SCRATCHPAD));

	printf("peek ticker 1: %02X\n", discferret_reg_peek(devh, DISCFERRET_R_CLOCK_TICKER));
	printf("peek ticker 2: %02X\n", discferret_reg_peek(devh, DISCFERRET_R_CLOCK_TICKER));

	printf("close: %d\n", discferret_close(devh));
	printf("done: %d\n", discferret_done());

	return 0;
}
