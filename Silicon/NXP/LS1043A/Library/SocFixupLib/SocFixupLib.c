/** @file

 Copyright 2018 NXP

 This program and the accompanying materials
 are licensed and made available under the terms and conditions of the BSD License
 which accompanies this distribution.  The full text of the license may be found at
 http://opensource.org/licenses/bsd-license.php

 THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <libfdt.h>
#include <Chassis.h>
#include <PiDxe.h>
#include <Soc.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/SocFixupLib.h>

/**
  Fixup the device tree based on running SOC's properties.

  @param[in]  Dtb   The device tree to fixup.

  @return EFI_SUCCESS       Successfully fix up the device tree
  @return EFI_DEVICE_ERROR  Could not fixup the device tree
  @return EFI_NOT_FOUND     Could not found the device tree overlay file
  @retval EFI_CRC_ERROR     Device tree overlay file is not correct.
**/
EFI_STATUS
FdtSocFixup (
  IN  VOID  *Dtb
  )
{
  CCSR_GUR        *GurBase;
  CONST fdt32_t   *Prop;
  EFI_STATUS      Status;
  INT32           PropLen;
  INTN            FdtStatus;
  UINTN           DtbOverlaySize;
  UINTN           Svr;
  UINTN           Index;
  VOID            *DtbOverlay;

  GurBase = (VOID *)PcdGet64 (PcdGutsBaseAddr);
  ASSERT (GurBase != NULL);

  Svr = GurRead ( (UINTN)&GurBase->Svr);

  Index = 1;
  // Apply the device tree overlays one by one
  while (TRUE) {
    Status = GetSectionFromAnyFv (
               &gDtPlatformDefaultDtbFileGuid,
               EFI_SECTION_RAW,
               Index,
               &DtbOverlay,
               &DtbOverlaySize
               );
    // If no section found, that means all overlay files have been processed
    if (Status == EFI_NOT_FOUND) {
      Status = EFI_SUCCESS;
      break;
    }

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to get device tree overlay Index %d Status = %r\n", Index, Status));
      break;
    }

    if (fdt_check_header (DtbOverlay)) {
      DEBUG ((DEBUG_ERROR, "Invalid Device Tree Overlay Index %d\n", Index));
      Status = EFI_CRC_ERROR;
      break;
    }

    // Test if overlay is for soc revision
    if (!fdt_node_check_compatible (DtbOverlay, 0, "fsl,ls1043a-rev")) {
      // Get SOC revision from overlay and compare with SOC revision present read from SOC
      Prop = fdt_getprop (DtbOverlay, 0, "reg", &PropLen);
      if (Prop == NULL) {
        DEBUG ((DEBUG_ERROR, "Device Tree Overlay Index %d has no reg property\n", Index));
        Status = EFI_NOT_FOUND;
        break;
      }

      if (SVR_MINOR (Svr) == fdt32_to_cpu (*Prop)) {
        FdtStatus = fdt_overlay_apply (Dtb, DtbOverlay);
        if (FdtStatus) {
          DEBUG ((
            EFI_D_ERROR,
            "fdt_overlay_apply/failed to apply overlay file @ index %d: %a\n",
            Index, fdt_strerror (FdtStatus)
            ));
          Status = EFI_DEVICE_ERROR;
          break;
        }
      }
    }

    // Move onto next Overlay File
    Index++;
  }

  return Status;
}

/**
  Retrive the System Version from System Version Register (SVR)

  @return  0  could not read SVR register or invalid value in SVR register
  @return     SVR register value
**/
UINT32
SocGetSvr (
  )
{
  CCSR_GUR     *GurBase;

  GurBase = (VOID *)PcdGet64 (PcdGutsBaseAddr);
  ASSERT (GurBase != NULL);

  return GurRead ( (UINTN)&GurBase->Svr);
}
