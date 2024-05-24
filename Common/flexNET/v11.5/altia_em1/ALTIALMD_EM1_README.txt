============================================================================
Altia FLEXlm Linux ALTIALMD supporting "em1" network interface versus "eth0"
Created: 2021 June 6 by Altia, Inc.
============================================================================

Summary of the issue that is resolved by this package
=====================================================
Some newer versions of Linux on Intel x86/x64 (as of 2021, maybe also back a
couple more years) now use "em1" as the network interface name instead of
"eth0".  The Altia FLEXlm Linux ALTIALMD daemon executable expects to see
"eth0" as the network interface name.  When it does not, it sends messages
like the following to its debug output with <HOSTID_NUM> set to the hex
host id number of the server:

   (ALTIALMD) SERVER line says <HOSTID_NUM>, hostid is (Can't get hostid of type 2 [])
   (ALTIALMD) Invalid hostid on SERVER line
   (ALTIALMD) Disabling X licenses from feature altia_design
   ...
   (ALTIALMD) No valid hostids, exiting

As a result of these messages, all features in the license file are disabled
which causes all checkouts from the license server to fail.

Prerequisites to this package:
==============================
  The full Altia FLEXlm Linux package must already be installed
  and configured to use a license file.

  If it is not installed, download these simple instructions:

      https://altia.box.com/s/4bc5e091cd5ca024084b
      
  Ignore the tar archive download link in the simple instructions and
  use this link instead:

      https://altia.box.com/s/67fae9033fdf1343e2dd

  Additional detailed instructions if required:

      https://altia.box.com/s/ovjv44ld1yuj7lhfyxll
      Password: bigband      

Files in this package:
======================
ALTIALMD
    Modified Altia FLEXlm Linux ALTIALMD executable to check
    for network interface named "em1" instead of "eth0".
    1. The Altia daemon MUST be name ALTIALMD otherwise
       starting lmgrd will fail.
    2. This ALTIALMD only supports checking for a network
       interface named "em1".  The original ALTIALMD must
       be used for a computer with a network interface
       named "eth0".

ALTIALMD_EM1_NEW
    Copy of new ALTIALMD that detects network interface "em1".

ALTIALMD_ETH0_ORIG
    Copy of original 2003/2004 ALTIALMD that detects "eth0".

Instructions:
=============
1.  Unpack altiaFLEXLMlinux_ALTIALMD_EM1.tar on Linux with this command:

        tar xvf altiaFLEXLMlinux_ALTIALMD_EM1.tar

2.  Copy ALTIALMD to the same folder as the existing Altia FLEXlm Linux lmgrd
    executable and start lmgrd as usual.

3.  If it is necessary to go back to the original ALTIALMD at a later date,
    copy ALTIALMD_ETH0_ORIG to ALTIALMD in the same folder as the Altia FLEXlm
    Linux lmgrd executable and start lmgrd as usual.
