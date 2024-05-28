
void testGpio9Gpio10() {
  ETS_PRINTF("  Test: QE=%c, GPIO pins 9 and 10 as INPUT\n", is_QE() ? '1' : '0');
  pinMode(9, INPUT);
  pinMode(10, INPUT);
  ETS_PRINTF("  Result: GPIO_9(%u) GPIO_10(%u)\n", digitalRead(9), digitalRead(10));
  delay(10);
  ETS_PRINTF("  Test: QE=%c, GPIO pins 9 and 10 as OUTPUT set LOW\n", is_QE() ? '1' : '0');
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  digitalWrite(9, LOW);
  digitalWrite(10, LOW);
  delay(10);
  ETS_PRINTF("  Passed\n"); // No WDT Reset - then it passed.
}

void cmdLoop(Print& oStream, int key) {
  switch (key) {
    case 'v':
      oStream.println(F("\nRunning GPIO9 and GPIO10 verification test"));
      oStream.println(F("Test GPIO9 and GPIO10 with 'pinMode()' for INPUT and OUTPUT operation"));
      testGpio9Gpio10();
      break;

    case 't':
      oStream.println(F("Test call to reclaim_GPIO_9_10()"));
      reclaim_GPIO_9_10();
      break;
    //
    case '3':
      {
        uint32_t status = 0;
        int ok = spi0_flash_read_status_registers_3B(&status);
        if (0 == ok) {
          ETS_PRINTF("\nspi0_flash_read_status_registers_3B(0x%06X)\n", status);
          if (kQEBit16 == (status & kQEBit16)) ETS_PRINTF("  QE=1\n");
          if (kWELBit  == (status & kWELBit))  ETS_PRINTF("  WEL=1\n");
        } else {
          ETS_PRINTF("\nspi0_flash_read_status_registers_3B() failed!\n");
        }
      }
      break;

    case 'R':
      oStream.println(F("Restart ..."));
      oStream.flush();
      ESP.restart();
      break;

    case 'd':
      {
        int ok = spi0_flash_write_disable();
        oStream.printf("\n%d = spi0_flash_write_disable()%s\n", ok, (0 == ok) ? "" : " - failed!\n");
        uint32_t status = 0;
        ok = spi0_flash_read_status_registers_3B(&status);
        oStream.printf("\n%d = spi0_flash_read_status_registers_3B(0x%06X)%s\n", ok, status, (0 == ok) ? "" : " - failed!\n");
      }
      break;

    case '?':
      oStream.println(F("\r\nHot key help:"));
      oStream.println(F("  3 - SPI Flash Read Status Registers, 3 Bytes"));
      oStream.println(F("  t - Test call to 'reclaim_GPIO_9_10()'"));
      oStream.println(F("  v - Test GPIO9 and GPIO10 with 'pinMode()' for INPUT and OUTPUT operation"));
      oStream.println(F("  d - Clear WEL bit, write enable"));
      oStream.println(F("  R - Restart"));
      oStream.println(F("  ? - This help message\r\n"));
      break;

    default:
      break;
  }
  oStream.println();
}

void serialClientLoop(void) {
  if (0 < Serial.available()) {
    char hotKey = Serial.read();
    cmdLoop(Serial, hotKey);
  }
}
