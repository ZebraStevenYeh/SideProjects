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

//
// Declare I/O Ports used to perform PCI Confguration Cycles
//
#define PCI_CONFIGURATION_ADDRESS_PORT  0xCF8
#define PCI_CONFIGURATION_DATA_PORT     0xCFC


#define PCI_IO_ADDRESS(Bus, Dev, Fun, Reg) 0x80000000+(Bus<<16)+(Dev<<11)+(Fun<<8)+Reg


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
  UINT32       PciIoAddr
) {
  UINT16       Index;
  UINT8        Data;

  // io access can only read the first 256 bytes
  for (Index = 0; Index <= 0xFF; Index ++) {
    IoWrite32 (PCI_CONFIGURATION_ADDRESS_PORT, (PciIoAddr + Index));
    Data = IoRead8 (PCI_CONFIGURATION_DATA_PORT + ((PciIoAddr + Index) & 3));

    Print (L"%02x ", Data);
    if ((Index + 1) % 16 == 0 ) Print (L"\n");
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
  UINT32               PciIoAddr;
  UINT16               VId;
  UINT16               DId;
  EFI_STATUS           Status;
  UINT16               Index;
  BOOLEAN              ShowRegs;

  PciIoAddr            = 0;
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

      PciIoAddr = PCI_IO_ADDRESS(Bus, Dev, Fun, 0);
      IoWrite32 (PCI_CONFIGURATION_ADDRESS_PORT, PciIoAddr);
      VId = IoRead16 (PCI_CONFIGURATION_DATA_PORT);
      if (VId == 0xffff) {
        Print (L"Invalid Pci address.\n");
        return EFI_INVALID_PARAMETER;
      }

      DId = IoRead16 (PCI_CONFIGURATION_DATA_PORT + 2);
      Print (L"%02x:%02x.%x  VId = 0x%04x, DId = 0x%04x\n", Bus, Dev, Fun, VId, DId);
      if (ShowRegs) {
        ShowPciConfigSpace(PciIoAddr);
        Print (L"\n");
      }

      return EFI_SUCCESS;
    }
  }

  for (BusIndex = 0; BusIndex < MaxBusNum; BusIndex ++) {
    for (DevIndex = 0; DevIndex < MaxDevNum; DevIndex ++) {
      for (FunIndex = 0; FunIndex < MaxFunNum; FunIndex ++) {

        PciIoAddr = PCI_IO_ADDRESS(BusIndex, DevIndex, FunIndex, 0);
        IoWrite32 (PCI_CONFIGURATION_ADDRESS_PORT, PciIoAddr);
        VId = IoRead16 (PCI_CONFIGURATION_DATA_PORT);
        if ((VId == 0xffff) && (FunIndex == 0)) {
          break;
        } else if (VId == 0xffff) {
          continue;
        }

        DId = IoRead16 (PCI_CONFIGURATION_DATA_PORT + 2);
        Print (L"%02x:%02x.%x  VId = 0x%04x, DId = 0x%04x\n", BusIndex, DevIndex, FunIndex, VId, DId);
        if (ShowRegs) {
          ShowPciConfigSpace(PciIoAddr);
          Print (L"\n");
        }
      }
    }
  }

  return EFI_SUCCESS;
}