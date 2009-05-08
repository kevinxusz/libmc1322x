#define GPIO_FUNC_SEL0  0x80000018 /* GPIO 15 - 0;  2 bit blocks */

#define BASE_UART1      0x80005000
#define UART1_CON       0x80005000
#define UART1_STAT      0x80005004
#define UART1_DATA      0x80005008
#define UR1CON          0x8000500c
#define UT1CON          0x80005010
#define UART1_CTS       0x80005014
#define UART1_BR        0x80005018

#define GPIO_PAD_DIR0   0x80000000
#define GPIO_DATA0      0x80000008

#include "embedded_types.h"
#include "nvm.h"
#include "maca.h"

#define reg(x) (*(volatile uint32_t *)(x))

#define DELAY 400000

void putc(uint8_t c);
void puts(uint8_t *s);
void put_hex(uint8_t x);
void put_hex16(uint16_t x);
void put_hex32(uint32_t x);
uint8_t getc();
void flushrx();

const uint8_t hex[16]={'0','1','2','3','4','5','6','7',
		 '8','9','a','b','c','d','e','f'};

#include "isr.h"

#define NBYTES 8
#define WRITE_ADDR 0x1e000
//#define WRITE_ADDR 0x0
#define WRITEVAL0 0x00000001 
#define WRITEVAL1 0x00000000

__attribute__ ((section ("startup")))
void main(void) {
	nvmType_t type=0;
	nvmErr_t err;
	volatile uint8_t c;
	volatile uint32_t buf[NBYTES/4];
	volatile uint32_t i;
	volatile uint32_t len=0;

	*(volatile uint32_t *)GPIO_PAD_DIR0 = 0x00000100;
	
	/* Restore UART regs. to default */
	/* in case there is still bootloader state leftover */

	reg(UART1_CON) = 0x0000c800; /* mask interrupts, 16 bit sample --- helps explain the baud rate */

	/* INC = 767; MOD = 9999 works: 115200 @ 24 MHz 16 bit sample */
	#define INC 767
	#define MOD 9999
	reg(UART1_BR) = INC<<16 | MOD; 

	/* see Section 11.5.1.2 Alternate Modes */
	/* you must enable the peripheral first BEFORE setting the function in GPIO_FUNC_SEL */
	/* From the datasheet: "The peripheral function will control operation of the pad IF */
	/* THE PERIPHERAL IS ENABLED. */
	reg(UART1_CON) = 0x00000003; /* enable receive and transmit */
	reg(GPIO_FUNC_SEL0) = ( (0x01 << (14*2)) | (0x01 << (15*2)) ); /* set GPIO15-14 to UART (UART1 TX and RX)*/

	vreg_init();

//	puts("CRM status: 0x");
//	put_hex32(reg(0x80003018));
//	puts("\n\r");

//	puts("Detecting internal nvm\n\r");

	err = nvm_detect(gNvmInternalInterface_c, &type);
		
/*
	puts("nvm_detect returned: 0x");
	put_hex(err);
	puts(" type is: 0x");
	put_hex32(type);
	puts("\n\r");
*/
	
	/* erase the flash */
//	err = nvm_erase(gNvmInternalInterface_c, type, 0x4fffffff); 
	err = nvm_erase(gNvmInternalInterface_c, 1, 0x4fffffff); 

/*
	puts("nvm_erase returned: 0x");
	put_hex(err);
	puts("\n\r");
*/

	/* say we are ready */
	len = 0;
	puts("ready");
	flushrx();

	/* read the length */
	for(i=0; i<4; i++) {
		c = getc();
		/* bail if the first byte of the length is zero */
		len += (c<<(i*8));
	}

//	puts("len: ");
//	put_hex32(len);
//	puts("\n\r");
	
	/* write the OKOK magic */
	((uint8_t *)buf)[0] = 'O'; ((uint8_t *)buf)[1] = 'K'; ((uint8_t *)buf)[2] = 'O'; ((uint8_t *)buf)[3] = 'K';
//	((uint8_t *)buf)[3] = 'x';
//	err = nvm_write(gNvmInternalInterface_c, type, (uint8_t *)buf, 0, 4);
	err = nvm_write(gNvmInternalInterface_c, 1, (uint8_t *)buf, 0, 4);
//	puts("nvm_write returned: 0x");
//	put_hex(err);
//	puts("\n\r");

	/* write the length */
	err = nvm_write(gNvmInternalInterface_c, 1, (uint8_t *)&len, 4, 4);

	/* read a byte, write a byte */
	/* byte at a time will make this work as a contiki process better */
	/* for OTAP */
	for(i=0; i<len; i++) {
		c = getc();	       
//		put_hex(c);
//		puts(": ");
//		err = nvm_write(gNvmInternalInterface_c, type, &c, 4+i, 1);
		err = nvm_write(gNvmInternalInterface_c, 1, &c, 8+i, 1);
//		if(err==0) { putc('.'); } else { putc('x'); }
//		puts("nvm_write returned: 0x");
//		put_hex(err);
//		puts("\n\r");
	}

	puts("flasher done\n\r");

	while(c=getc()) {putc(c);}

	while(1) {continue;};
}

void flushrx()
{
	volatile uint8_t c;
	while(reg(UR1CON) !=0) {
		c = reg(UART1_DATA);
	}
}

uint8_t getc() 
{
	volatile uint8_t c;
	while(reg(UR1CON) == 0);
	
	c = reg(UART1_DATA);
	return c;
}

void putc(uint8_t c) {
	while(reg(UT1CON)==31); /* wait for there to be room in the buffer */
	reg(UART1_DATA) = c;
}
	
void puts(uint8_t *s) {
	while(s && *s!=0) {
		putc(*s++);
	}
}

void put_hex(uint8_t x)
{
        putc(hex[x >> 4]);
        putc(hex[x & 15]);
}

void put_hex16(uint16_t x)
{
        put_hex((x >> 8) & 0xFF);
        put_hex((x) & 0xFF);
}

void put_hex32(uint32_t x)
{
        put_hex((x >> 24) & 0xFF);
        put_hex((x >> 16) & 0xFF);
        put_hex((x >> 8) & 0xFF);
        put_hex((x) & 0xFF);
}
