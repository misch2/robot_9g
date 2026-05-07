
// This is the command sequence that initialises the GC9D01 driver.
//
// Translated from moononournation/Arduino_GFX
// (src/display/Arduino_GC9D01.h, GC9D01_init_operations[] array,
// commit on master 2024-09). Independently corroborated by the
// commented-out 160x160 block in webfraggle/TFT_eSPI@GC9D01N at
// TFT_Drivers/GC9D01N_Init.h lines 326-559 — bytes match register-
// for-register. GC9D01 is NOT register-compatible with GC9A01; the
// inner-register unlock pattern, gamma curves, gate driver tables,
// and the GC9D01-specific dual-gate-mode (0xBF) and 32-byte 0x6E
// waveform table all differ.

{
  // Inner-register access enable + unlock pattern: write 0xFF to every
  // register from 0x80 through 0x8F. Without this the panel-config
  // registers below are write-protected.
  writecommand(0xFE);
  writecommand(0xEF);

  writecommand(0x80); writedata(0xFF);
  writecommand(0x81); writedata(0xFF);
  writecommand(0x82); writedata(0xFF);
  writecommand(0x83); writedata(0xFF);
  writecommand(0x84); writedata(0xFF);
  writecommand(0x85); writedata(0xFF);
  writecommand(0x86); writedata(0xFF);
  writecommand(0x87); writedata(0xFF);
  writecommand(0x88); writedata(0xFF);
  writecommand(0x89); writedata(0xFF);
  writecommand(0x8A); writedata(0xFF);
  writecommand(0x8B); writedata(0xFF);
  writecommand(0x8C); writedata(0xFF);
  writecommand(0x8D); writedata(0xFF);
  writecommand(0x8E); writedata(0xFF);
  writecommand(0x8F); writedata(0xFF);

  // COLMOD = 16bpp / RGB565 (TFT_eSPI's pixel format)
  writecommand(0x3A); writedata(0x05);

  // Inversion mode (GC9D01-specific register, not in GC9A01)
  writecommand(0xEC); writedata(0x01);

  writecommand(0x74);
  writedata(0x02); writedata(0x0E); writedata(0x00); writedata(0x00);
  writedata(0x00); writedata(0x00); writedata(0x00);

  writecommand(0x98); writedata(0x3E);
  writecommand(0x99); writedata(0x3E);

  // Blanking porch control
  writecommand(0xB5); writedata(0x0D); writedata(0x0D);

  // Gate driver tables
  writecommand(0x60);
  writedata(0x38); writedata(0x0F); writedata(0x79); writedata(0x67);

  writecommand(0x61);
  writedata(0x38); writedata(0x11); writedata(0x79); writedata(0x67);

  writecommand(0x64);
  writedata(0x38); writedata(0x17); writedata(0x71); writedata(0x5F);
  writedata(0x79); writedata(0x67);

  writecommand(0x65);
  writedata(0x38); writedata(0x13); writedata(0x71); writedata(0x5B);
  writedata(0x79); writedata(0x67);

  writecommand(0x6A); writedata(0x00); writedata(0x00);

  writecommand(0x6C);
  writedata(0x22); writedata(0x02); writedata(0x22); writedata(0x02);
  writedata(0x22); writedata(0x22); writedata(0x50);

  // 32-byte timing/waveform table — GC9D01-specific, completely absent
  // from GC9A01.
  writecommand(0x6E);
  writedata(0x03); writedata(0x03); writedata(0x01); writedata(0x01);
  writedata(0x00); writedata(0x00); writedata(0x0F); writedata(0x0F);
  writedata(0x0D); writedata(0x0D); writedata(0x0B); writedata(0x0B);
  writedata(0x09); writedata(0x09); writedata(0x00); writedata(0x00);
  writedata(0x00); writedata(0x00); writedata(0x0A); writedata(0x0A);
  writedata(0x0C); writedata(0x0C); writedata(0x0E); writedata(0x0E);
  writedata(0x10); writedata(0x10); writedata(0x00); writedata(0x00);
  writedata(0x02); writedata(0x02); writedata(0x04); writedata(0x04);

  // Dual signal gate mode — GC9D01-specific
  writecommand(0xBF); writedata(0x01);

  writecommand(0xF9); writedata(0x40);
  writecommand(0x9B); writedata(0x3B);

  writecommand(0x93);
  writedata(0x33); writedata(0x7F); writedata(0x00);

  writecommand(0x7E); writedata(0x30);

  writecommand(0x70);
  writedata(0x0D); writedata(0x02); writedata(0x08); writedata(0x0D);
  writedata(0x02); writedata(0x08);

  writecommand(0x71);
  writedata(0x0D); writedata(0x02); writedata(0x08);

  writecommand(0x91); writedata(0x0E); writedata(0x09);

  // Power control
  writecommand(0xC3); writedata(0x19);
  writecommand(0xC4); writedata(0x19);
  writecommand(0xC9); writedata(0x3C);

  // Gamma curves (4 sets — completely different bytes from GC9A01)
  writecommand(0xF0);
  writedata(0x53); writedata(0x15); writedata(0x0A); writedata(0x04);
  writedata(0x00); writedata(0x3E);

  writecommand(0xF2);
  writedata(0x53); writedata(0x15); writedata(0x0A); writedata(0x04);
  writedata(0x00); writedata(0x3A);

  writecommand(0xF1);
  writedata(0x56); writedata(0xA8); writedata(0x7F); writedata(0x33);
  writedata(0x34); writedata(0x5F);

  writecommand(0xF3);
  writedata(0x52); writedata(0xA4); writedata(0x7F); writedata(0x33);
  writedata(0x34); writedata(0xDF);

  // MADCTL — portrait, RGB. setRotation() will overwrite this; included
  // here for parity with the Arduino_GFX init.
  writecommand(0x36); writedata(0x00);

  // Sleep out + display on
  writecommand(0x11); // SLPOUT
  delay(120);
  writecommand(0x29); // DISPON
  delay(20);
}
