.. _frdm_mcxe247_wiring:

FRDM-MCXE247 Hardware Wiring (User Truth)
#########################################

This document provides the definitive physical wiring for the MIDI Synthesizer 
on the FRDM-MCXE247 board J1 header, as verified against **UM12286 Figure 13**.

J1 Header Pinout
****************

The following physical pin assignments must be used on the J1 header:

+------------------+-----------------------+------------------------+
| Signal           | Physical J1 Pin       | MCU Pin                |
+==================+=======================+========================+
| Bit Clock (BCLK) | **J1 Pin 9**          | PTA12 (SAI0_BCLK)      |
+------------------+-----------------------+------------------------+
| Frame Sync (LRCK)| **J1 Pin 7**          | PTA11 (SAI0_SYNC)      |
+------------------+-----------------------+------------------------+
| Data Out (TX_DAT)| **J1 Pin 5**          | PTE1 (SAI0_D1)         |
+------------------+-----------------------+------------------------+
| Data In (RX_DAT) | **J1 Pin 15**         | PTA14 (SAI0_D3)        |
+------------------+-----------------------+------------------------+

Configuration Details
*********************

The synthesizer is configured to use **SAI0**. 

*   **TX Data Pin**: physically located on **SAI0_D1** (PTE1).
*   **Devicetree Setting**: ``nxp,tx-channel`` is set to ``<2>`` (bitmask for 
    Channel 1) to match the physical Data Out pin.

MIDI Connectivity
*****************

MIDI input utilizes the standard Arduino Header pins (LPUART2):

*   **MIDI RX**: Arduino D0 (J1 Pin 14 / PTD17)
*   **MIDI TX**: Arduino D1 (J1 Pin 16 / PTE12)

@author Jan-Willem Smaal <usenet@gispen.org>
