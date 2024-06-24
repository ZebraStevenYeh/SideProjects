#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/IoLib.h>

#include <Protocol/ShellParameters.h>

UINTN   Argc;
CHAR16  **Argv;

#define MaxBusNum                       256
#define MaxDevNum                       32
#define MaxFunNum                       8

#define PCI_MMIO_ADDRESS(Bus, Dev, Fun, Reg) (Bus<<20)+(Dev<<15)+(Fun<<12)+Reg
#define INTEL_PCIE_MMIO_BASE_ADDR            0xE0000000
#define AMD_PCIE_MMIO_BASE_ADDR              0xF0000000

//Note: The gImageHandle is the image handle for this app.


/**

  This function parse application ARG.

  @return Status
**/
EFI_STATUS
GetArg (
  VOID
  )
{
  EFI_STATUS                     Status;
  EFI_SHELL_PARAMETERS_PROTOCOL  *ShellParameters;

  Status = gBS->HandleProtocol (
                  gImageHandle,
                  &gEfiShellParametersProtocolGuid,
                  (VOID **)&ShellParameters
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Argc = ShellParameters->Argc;
  Argv = ShellParameters->Argv;

  return EFI_SUCCESS;
}

/**
  Print APP usage.
**/
VOID
PrintUsage (
  VOID
  )
{
  Print (L"PcieCamTool:  usage\n");
  Print (L"  PcieCamTool -h\n");
  Print (L"  PcieCamTool -x\n");
  Print (L"  PcieCamTool -s Bus:Dev.Fun\n");
  Print (L"  PcieCamTool\n");
  Print (L"Parameter:\n");
  Print (L"  -h -help: show instructions\n");
  Print (L"  -s: specify a pcie device with the format of Bus:Dev.Fun.\n");
  Print (L"  -x: show pcie configuration space\n");
}

BOOLEAN
IsDecimalDigitCharacter (
  CHAR16                    Char
) {
  return (BOOLEAN) (Char >= L'0' && Char <= L'9');
}

CHAR16
Char16ToUpper (
  CHAR16                    Char
  )
{
  if (Char >= L'a' && Char <= L'z') {
    return (CHAR16) (Char - (L'a' - L'A'));
  }

  return Char;
}

INT16
HexStrToUint16 (
  CHAR16      FirstChara,
  CHAR16      SecondChara
  )
{
  UINT16      Data;

  if(FirstChara == SecondChara) {
    if (IsDecimalDigitCharacter(FirstChara)) {
      Data = FirstChara - L'0';
    } else {
      if ((Char16ToUpper(SecondChara) - L'A') > 5) return -1;
      Data = Char16ToUpper(FirstChara) - L'A' + 10;
    }
  } else {
    if (IsDecimalDigitCharacter(FirstChara)) {
      Data = FirstChara - L'0';
    } else {
      if ((Char16ToUpper(FirstChara) - L'A') > 5) return -1;
      Data = Char16ToUpper(FirstChara) - L'A' + 10;
    }

    if (IsDecimalDigitCharacter(SecondChara)) {
      Data = (Data << 4) + (SecondChara - L'0');
    } else {
      if ((Char16ToUpper(SecondChara) - L'A') > 5) return -1;
      Data = (Data << 4) + (Char16ToUpper(SecondChara) - L'A' + 10);
    }
  }

  return Data;
}

/**
  Print input parameters.
**/
EFI_STATUS
ParseParameters (
  CHAR16      *InputStr,
  INT16       *Bus,
  INT16       *Dev,
  INT16       *Fun 
  )
{
  UINT16      Index;
  UINT16      SemicolonPosition;
  UINT16      DotPisition;
  UINT16      InputStrLen;

  if (InputStr == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; InputStr[Index] != L'\0'; Index ++) {
    if ((InputStr[Index] == L' ') || (InputStr[Index] == L'\t')) return EFI_INVALID_PARAMETER;
    if (InputStr[Index] == L':') SemicolonPosition = Index;
    if (InputStr[Index] == L'.') DotPisition       = Index;
  }

  InputStrLen = Index;

  if ((SemicolonPosition > 2) || (DotPisition - SemicolonPosition > 3) || (InputStrLen - DotPisition > 2)) {
    return EFI_INVALID_PARAMETER; 
  }

  *Bus = HexStrToUint16(InputStr[0], InputStr[SemicolonPosition - 1]);
  *Dev = HexStrToUint16(InputStr[SemicolonPosition + 1], InputStr[DotPisition - 1]);
  *Fun = HexStrToUint16(InputStr[DotPisition + 1], InputStr[InputStrLen - 1]);

  if (*Bus == -1 || *Dev == -1 || *Fun == -1) return EFI_INVALID_PARAMETER;

  Print (L"Debug InputStrLen = %d, Bus = %x, Dev = %x, Fun = %x\n", InputStrLen, *Bus, *Dev, *Fun);

  return EFI_SUCCESS;
}

VOID
ShowPciConfigSpace (
  UINTN        PciMmioAddr
) {
  UINT16       Index;
  UINT8        Data;

  for (Index = 0; Index <= 0xFFF; Index ++) {
    Data = MmioRead8 (PciMmioAddr + Index);
    Print (L"%02x ", Data);
    if ((Index + 1) % 16 == 0 ) Print (L"\n");
    if (Index == 0x100 && MmioRead32 (PciMmioAddr + Index) == 0) break;
  }
}

/**
  The user Entry Point for Application. The user code starts with this function
  as the real entry point for the application.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINT16               BusIndex;
  UINT16               DevIndex;
  UINT16               FunIndex;
  INT16                Bus;
  INT16                Dev;
  INT16                Fun;
  UINTN                PcieMmioAddr;
  UINT16               VId;
  UINT16               DId;
  EFI_STATUS           Status;
  UINT16               Index;
  BOOLEAN              ShowRegs;

  PcieMmioAddr         = 0;
  VId                  = 0;
  DId                  = 0;
  Index                = 0;
  ShowRegs             = FALSE;

  Status = GetArg();
  if (EFI_ERROR (Status)) {
    Print (L"Get patameters failed!\n");
    return Status;
  }

// Parse input parameters.
  for (Index = 0; Index < Argc; Index ++) {
    if (StrCmp(Argv[Index], L"-h") == 0 || StrCmp(Argv[Index], L"-help") == 0) {
      PrintUsage();
      return EFI_SUCCESS;
    }
  }

  for (Index = 0; Index < Argc; Index ++) {
    if (StrCmp(Argv[Index], L"-x") == 0 || StrCmp(Argv[Index], L"-X") == 0) {
      ShowRegs = TRUE;
    }
  }

  for (Index = 0; Index < Argc - 1; Index ++) {
    if (StrCmp(Argv[Index], L"-s") == 0 || StrCmp(Argv[Index], L"-S") == 0) {
      Status = ParseParameters(Argv[Index + 1], &Bus, &Dev, &Fun);
      if (EFI_ERROR(Status)) {
        Print (L"Invalid Pci address.\n");
        return EFI_INVALID_PARAMETER;
      }

      VId = MmioRead16 (AMD_PCIE_MMIO_BASE_ADDR + PCI_MMIO_ADDRESS(Bus, Dev, Fun, 0));
      if (VId == 0xffff) {
        Print (L"Invalid Pci address.\n");
        return EFI_INVALID_PARAMETER;
      }

      DId = MmioRead16 (AMD_PCIE_MMIO_BASE_ADDR + PCI_MMIO_ADDRESS(Bus, Dev, Fun, 2));
      Print (L"%02x:%02x.%x  VId = 0x%04x, DId = 0x%04x\n", Bus, Dev, Fun, VId, DId);
      if (ShowRegs) {
        PcieMmioAddr = AMD_PCIE_MMIO_BASE_ADDR + PCI_MMIO_ADDRESS(Bus, Dev, Fun, 0);
        ShowPciConfigSpace (PcieMmioAddr);
        Print (L"\n");
      }

      return EFI_SUCCESS;
    }
  }

  for (BusIndex = 0; BusIndex < MaxBusNum; BusIndex ++) {
    for (DevIndex = 0; DevIndex < MaxDevNum; DevIndex ++) {
      for (FunIndex = 0; FunIndex < MaxFunNum; FunIndex ++) {

        VId = MmioRead16 (AMD_PCIE_MMIO_BASE_ADDR + PCI_MMIO_ADDRESS(BusIndex, DevIndex, FunIndex, 0));
        if ((VId == 0xffff) && (FunIndex == 0)) {
          break;
        } else if (VId == 0xffff) {
          continue;
        }

        DId = MmioRead16 (AMD_PCIE_MMIO_BASE_ADDR + PCI_MMIO_ADDRESS(BusIndex, DevIndex, FunIndex, 2));
        Print (L"%02x:%02x.%x  VId = 0x%04x, DId = 0x%04x\n", BusIndex, DevIndex, FunIndex, VId, DId);
        if (ShowRegs) {
          ShowPciConfigSpace(PcieMmioAddr);
          Print (L"\n");
        }
      }
    }
  }

  return EFI_SUCCESS;
}