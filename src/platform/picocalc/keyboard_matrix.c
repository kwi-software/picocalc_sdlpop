/* Physical PicoCalc layout from clockworkpi/PicoCalc keyboard.ino.
   Active-low matrix bits. All modifier transitions precede ordinary keys. */
#include "keyboard_matrix.h"
#include "pop_port.h"
void pc_keyboard_matrix_feed(const uint8_t columns[8], uint8_t arrows) {
  static const uint8_t buttons[8] = {0xa1, 0xa5, 0xa2, 0xa3,
                                     '0',  '9',  ']',  '['};
  static const uint8_t keys[7][8] = {
      {0x85, 0x84, 0x83, 0x82, 0x81, '`', '3', '2'},
      {8, 0xd4, 0xc1, 9, 0xb1, '4', 'e', 'w'},
      {'p', '=', '-', 92, '/', 'r', 's', '1'},
      {13, '8', '7', '6', '5', 'f', 'x', 'q'},
      {'.', 'i', 'u', 'y', 't', 'v', ';', 'a'},
      {'l', 'k', 'j', 'h', 'g', 'c', 39, 'z'},
      {'o', ',', 'm', 'n', 'b', 'd', ' ', 0}};
  static const uint8_t directions[4] = {0xb7, 0xb5, 0xb6, 0xb4};
  for (unsigned c = 0; c < 8; c++)
    PC_InputFeed(buttons[c], columns[c] & 128 ? 3 : 1);
  for (unsigned y = 0; y < 7; y++)
    for (unsigned c = 0; c < 8; c++)
      if (keys[y][c])
        PC_InputFeed(keys[y][c], columns[c] & (1u << y) ? 3 : 1);
  for (unsigned i = 0; i < 4; i++)
    PC_InputFeed(directions[i], arrows & (1u << i) ? 3 : 1);
}
