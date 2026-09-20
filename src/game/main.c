/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "common.h"
#include "pop_port.h"
int main(void) {
  PC_Init();
  static char program[] = "prince";
  static char *argv[] = {program, NULL};
  g_argc = 1;
  g_argv = argv;
  pop_main();
  for (;;)
    PC_Delay(1000);
}
