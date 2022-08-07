#include <conio.h>
#include <stdio.h>
#include <cbm.h>

#define SMC 0x42
#define ECHOREG 8
#define DEBUGREG 9

extern int __fastcall__ cx16_k_i2c_readbyte(char offset, char device);
extern char __fastcall__ cx16_k_i2c_writebyte(char value, char offset, char device);


void main() {
  static char y, i;
  static unsigned int attempts;
  static int v;
  static char vera_irqstate;

  static char key = 0;

  vera_irqstate = VERA.irq_enable;

#ifndef NOKBD
  printf("disable vera vblank irq? ");
  while (kbhit()) cgetc(); // clear input buffer
  key = 2;
  while(key==2) {
    while(!kbhit()) {}
    key = cgetc();
    switch (key) {
      case 'y':
      case 'Y':
        key = 0;
        VERA.irq_enable = 0;
        VERA.irq_flags = 0x03; // ack any pending VERA IRQs.
        printf("Y\n");
        break;
      case 'n':
      case 'N':
        printf("N\n");
        key = 1;
        break;
      default:
        key = 2;
    }
  }
#endif

  i=255;
  attempts=0xffff;
  y = wherey();
  if (y>=57) { printf("\n\n\n\n"); y=55; }
  do {
    ++i;
    gotoxy(0,y);
    cprintf("a:%5u i:$%02x r:$  ",attempts,i);
    gotox(wherex()-2);
    if (!cx16_k_i2c_writebyte(i,ECHOREG,SMC)) {
        cprintf("\n\rwrite error\n\r");
        break;
    }
    v = cx16_k_i2c_readbyte(ECHOREG,SMC);
    if (v<0) {
      cprintf("\n\rread error\n\r");
      break;
    }
    cprintf("%02x",v);
    if ((char)v != i) {
      cprintf("\n\rmismatch\n\r");
      cx16_k_i2c_writebyte(1,DEBUGREG,SMC); // tell SMC to debug output the echo_byte
      break;
    }
    --attempts;
    if(key) {
      if (kbhit()) attempts=0;
    }
  } while (attempts > 0);
  if (attempts == 0) {
    cprintf("\n\rsuccess.\n\r");
  }
  printf("done\n");
  VERA.irq_enable = vera_irqstate;
}
