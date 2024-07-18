/*
 *   Copyright 2024 M Hightower
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */
#ifndef SPI_FLASH_VENDOR_MYSTERY_D8
/*
 Mystery Flash Chip ID 0xD8

 Most of my NodeMCU v1.0 DEV boards have this undocumented part :(

 First point, if 0xD8 is a Manufacturer ID it has a parity error. Therefore, it
 will not appear in any list of JEDEC Manufacturer Banks.

////////////////////////////////////////////////////////////////////////////////
                                          SFDP Header dword 1
                      +------------------ SFDP Signature, 'SFDP'
                      |
                      |                   SFDP Header dword 2
                      |           +------ Access Protocol
                      |          | +----- Number of Parameter Headers (NPH) 0-based
                      |          | | +--- Major Revision Number
                      +-+-+-+    | | | +- Minor Revision Number
                      | | | |    | | | |
 SFDP:     0x00    0x50444653 0xFF010106
                                  ^   ^-- GD25Q32C does not have these values

                                          SFDP Parameter 1
                      +------------------ Parameter Table Length, 1-based (16 dwords)
                      | +---------------- Parameter Table Major Revision Number
                      | | +-------------- Parameter Table Minor Revision Number
                      | | | +------------ Parameter ID LSB:
                      | | | |
                      | | | |    +------- Parameter ID MSB:
                      | | | |    | +-+-+- Parameter Table Pointer (PTP)
                      | | | |    | | | |
                      | | | |    | | | |
                      | | | |    | | | |
           0x08    0x10010600 0xFF000030

                                          SFDP Parameter 2
                      +------------------ Parameter Table Length, 1-based
                      | +---------------- Parameter Table Major Revision Number
                      | | +-------------- Parameter Table Minor Revision Number
                      | | | +------------ Parameter ID LSB: GigaDevice pdf says - GigaDevice Manufacturer ID
                      | | | |
                      | | | |    +------- Parameter ID MSB: GigaDevice pdf says cannot be changed.
                      | | | |    | +-+-+- Parameter Table Pointer (PTP)
                      | | | |    | | | |
                      | | | |    | | | |
                      | | | |    | | | |
 SFDP:     0x10    0x030100C8 0xFF000090
           0x18    0xFFFFFFFF 0xFFFFFFFF
 ...
                   JEDEC Basic Flash Parameter Table
 SFDP:     0x30    0xFFF120E5 0x01FFFFFF 0x6B08EB44 0xBB423B08
 SFDP:     0x40    0xFFFFFFEE 0xFF00FFFF 0xFF00FFFF 0x520F200C
 SFDP:     0x50    0xFF00D810                                   GD25Q32C only has 9 entries and these match
           0x54               0xFEBD4A24 0x42152782 0x331662EC
 SFDP:     0x60    0x757A757A 0x5CD5B304 0x00640600 0x00001008  GD25Q32E has 16 entries
 ...
 SFDP:     0x90    0x27003600 0x6477F99E 0xFFFFCBFC 0xFFFFFFFF
      3.6 Volts MAX
      2.7 Volts MIN
      HW Reset# pin                  - not support
      HW Hold# pin                   - support
      Deep Power Down Mode           - support
      SW Reset                       - support
      SW Reset Op code               - Should be issue Reset Enable(66h) before Reset CMD, 99h.
      Program Suspend/Resume         - support
      Erase Suspend/Resume           - support
      Wrap-Around Read mode          - support
      Wrap-Around Read mode Op code  - 77h
      Wrap-Around Read data length   - 64H: 8B&16B&32B&64B ?? I think these are for the various modes
      Individual block lock          - not support
      " bit (Volatile/Nonvolatile)   - not support
      Individual block lock Op code  - 0xFF
      Individual block lock Volatile - protect
        protect bit default protect
        status
      Secured OTP                    - support
      Read Lock                      - not Support
      Permanent Lock                 - not support* - this is differnet from the CD25Q32C
      Unued 15:14                    - 11b

 deviceId:    0x001640d8,    1458392, 'unknown'

 Consult JESD216F-02.pdf for SFDP details:

 15th DWORD param data not present on CD25Q32C; however, it is on the CD25Q32E.

 110b: 15th DWORD 22:20
 QE is BIT1 of the SR2.
 Read 1 data byte from SR1 with Read Status instruction 05h.
 Read 1 data byte from SR2 with Read Status instruction 35h,
 Read 1 data byte from SR3 with Read Status instruction 15h.
 Write one data byte to SR2 with Write Status instruction 31h,
 QE is enabled via Write SR2 with 1 data byte with BIT1 set.
 QE is disabled via Write SR2 with 1 data byte with BIT1 cleared.

 xxx_1xxxb: 16th DWORD 6:0
 Non-Volatile/Volatile SR1 powers-up to last value written to the non-volatile
 SR1. For non-volatile SR1, use instruction 06h to enable write to non-volatile
 status register.
 To enable override with volatile status register after power-up, use
 instruction 50h to enable write before issuing instruction 06h for write SR1.

 x1_xxxxb: 16th DWORD 13:8
 Supports software reset instructions. Send two one-byte SPI bus commands, reset
 enable instruction 66h, and then reset instruction 99h.

 The ESP8266 BootROM tries to reprogramme the QE at boot, based on the
 information in the bin header file. This action fails with this device. Leaving
 the QE bit unchanged.  This could be used as a feature to allow access to the
 GPIO9 and GPIO10 pins w/o reprogramming the Flash Status Register at every boot
 cycle. By setting the non-volatile QE bit once, at the next boot cycle the
 BootROM tries and fails to reset the QE bit. Evidence of failure are present
 after boot, by way of the WSEN bit still being set,

 Has SR1, SR2, and SR3
 No support for legacy 16-bit SR1 Write
 There appear to be Driver strength bits in SR3, defaults to 75% (01)
 This mystery flash supports a 128-bit Unique ID, but only the first 64 bits are
 set the last 64 are 0xFF... unprogrammed.
 The 2nd Parameter table hints at the MFG being "GigaDevice Semiconductor".
 I get the feeling these parts were never finalized. Also the MFG ID 0xD8 is
 returned by the SPI command 9fh which is 0xC8 with one bit picked, 0x10. Maybe
 with some unknown manufactuing tools an easy burn to 0 when the parts are
 ready. Similar to 0xFFs last 64 bits of the unique ID.

 Summary
 -------

 Whatever the situation is for this device, it does not behave as expected if
 it were a GigaDevice Part. So lets define what we know.

 No support for 16-bit write SR1

 Register bits - This is what I assume:

    S23   S22   S21   S20   S19   S18   S17   S16   - SR3
         DRV1  DRV0

    S15   S14   S13   S12   S11   S10    S9    S8   - SR2
                                         QE

     S7    S6    S5    S4    S3    S2    S1    s0   - SR1
                                        WEL   WIP

There is a strong correlation between GigaDevice datasheet and this SFDP, I think
it is reasonable to use the GigaDevice datasheet for strength values.

DRV1 and DRV0 are assume to be driver strength values. Use the table as defined
by Winbond and GigaDevice for driver strength percentage value.

All status registers are accessed as 8-bit only.
Avoid using any bits not listed.

Assume Unique ID has only 64-bits or don't use.

Software reset 66h, 99h cleared the non-volatile QE bit in the SR2

*/

#define SPI_FLASH_VENDOR_MYSTERY_D8 0xD8u
constexpr uint32_t kMysteryId_D8 = SPI_FLASH_VENDOR_MYSTERY_D8;
#endif // #ifndef SPI_FLASH_VENDOR_MYSTERY_D8.h
