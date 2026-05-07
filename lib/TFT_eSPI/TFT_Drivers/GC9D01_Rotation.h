
// This is the command sequence that rotates the GC9D01 driver coordinate frame.
// MADCTL bit semantics on GC9D01 are identical to GC9A01 (MY=0x80, MX=0x40,
// MV=0x20, BGR=0x08), so this matches GC9A01_Rotation.h.

  rotation = m % 4;

  writecommand(TFT_MADCTL);
  switch (rotation) {
    case 0: // Portrait
      writedata(TFT_MAD_BGR);
      _width  = _init_width;
      _height = _init_height;
      break;
    case 1: // Landscape (Portrait + 90)
      writedata(TFT_MAD_MX | TFT_MAD_MV | TFT_MAD_BGR);
      _width  = _init_height;
      _height = _init_width;
      break;
    case 2: // Inverter portrait
      writedata(TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_BGR);
      _width  = _init_width;
      _height = _init_height;
      break;
    case 3: // Inverted landscape
      writedata(TFT_MAD_MV | TFT_MAD_MY | TFT_MAD_BGR);
      _width  = _init_height;
      _height = _init_width;
      break;
  }
