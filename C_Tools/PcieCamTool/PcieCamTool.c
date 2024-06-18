#include "stdio.h"
#include "sys/io.h"
#include "string.h"
#include "stdbool.h"

#define MaxBusNum                       255
#define MaxDevNum                       31
#define MaxFunNum                       7

//
// Declare I/O Ports used to perform PCI Confguration Cycles
//
#define PCI_CONFIGURATION_ADDRESS_PORT  0xCF8
#define PCI_CONFIGURATION_DATA_PORT     0xCFC


#define PCI_IO_ADDRESS(Bus, Dev, Fun, Reg) 0x80000000+(Bus<<16)+(Dev<<11)+(Fun<<8)+Reg


/**
  Print usage.
**/
void
PrintUsage (
  void
  )
{
  printf("PcieCamTool:  usage\n");
  printf("  PcieCamTool -h\n");
  printf("  PcieCamTool -x\n");
  printf("  PcieCamTool -s Bus:Dev.Fun\n");
  printf("  PcieCamTool\n");
  printf("Parameter:\n");
  printf("  -h -help: show instructions\n");
  printf("  -s: specify a pcie device with the format of Bus:Dev.Fun.\n");
  printf("  -x: show pcie configuration space\n");
}

bool
IsDecimalDigitCharacter (
  char                    Char
) {
  return (bool) (Char >= '0' && Char <= '9');
}

char
Char16ToUpper (
  char                    Char
  )
{
  if (Char >= 'a' && Char <= 'z') {
    return (char) (Char - ('a' - 'A'));
  }

  return Char;
}

int
HexStrToUint16 (
  char              FirstChara,
  char              SecondChara
  )
{
  unsigned int      Data;

  if(FirstChara == SecondChara) {
    if (IsDecimalDigitCharacter(FirstChara)) {
      Data = FirstChara - '0';
    } else {
      if ((Char16ToUpper(SecondChara) - 'A') > 5) return -1;
      Data = Char16ToUpper(FirstChara) - 'A' + 10;
    }
  } else {
    if (IsDecimalDigitCharacter(FirstChara)) {
      Data = FirstChara - L'0';
    } else {
      if ((Char16ToUpper(FirstChara) - 'A') > 5) return -1;
      Data = Char16ToUpper(FirstChara) - 'A' + 10;
    }

    if (IsDecimalDigitCharacter(SecondChara)) {
      Data = (Data << 4) + (SecondChara - '0');
    } else {
      if ((Char16ToUpper(SecondChara) - 'A') > 5) return -1;
      Data = (Data << 4) + (Char16ToUpper(SecondChara) - 'A' + 10);
    }
  }

  return Data;
}

/**
  Print input parameters.
**/
int
ParseParameters (
  char                *InputStr,
  int                 *Bus,
  int                 *Dev,
  int                 *Fun 
  )
{
  unsigned short      Index;
  unsigned short      SemicolonPosition;
  unsigned short      DotPisition;
  unsigned short      InputStrLen;

  if (InputStr == NULL) {
    return -1;
  }

  for (Index = 0; InputStr[Index] != '\0'; Index ++) {
    if ((InputStr[Index] == ' ') || (InputStr[Index] == '\t')) return -1;
    if (InputStr[Index] == ':') SemicolonPosition = Index;
    if (InputStr[Index] == '.') DotPisition       = Index;
  }

  InputStrLen = Index - 1;

  if ((SemicolonPosition > 2) || (DotPisition - SemicolonPosition > 3) || (InputStrLen - DotPisition > 1)) {
    return -1; 
  }

  *Bus = HexStrToUint16(InputStr[0], InputStr[SemicolonPosition - 1]);
  *Dev = HexStrToUint16(InputStr[SemicolonPosition + 1], InputStr[DotPisition - 1]);
  *Fun = HexStrToUint16(InputStr[DotPisition + 1], InputStr[InputStrLen - 1]);

  if (*Bus == -1 || *Dev == -1 || *Fun == -1) return -1;

  printf ("Debug InputStrLen = %d, Bus = %x, Dev = %x, Fun = %x\n", InputStrLen, *Bus, *Dev, *Fun);

  return 0;
}

void
ShowPciConfigSpace (
  unsigned long    PciIoAddr
) {
  unsigned short   Index;
  unsigned short   Data;

  // io access can only read the first 256 bytes
  for (Index = 0; Index <= 0xFF; Index ++) {
    outl ((PciIoAddr + Index), PCI_CONFIGURATION_ADDRESS_PORT);
    Data = inl (PCI_CONFIGURATION_DATA_PORT + ((PciIoAddr + Index) & 3));

    printf("%02x ", Data);
    if ((Index + 1) % 16 == 0 ) printf("\n");
  }
}

int main (
    int           Argc,
    char          *Argv[]
) {
    unsigned int  BusIndex;
    unsigned int  DevIndex;
    unsigned int  FunIndex;
    int           Bus;
    int           Dev;
    int           Fun;
    unsigned long PciIoAddr;
    unsigned int  VId;
    unsigned int  DId;
    int           ReturnValue;
    unsigned int  Index;
    bool          ShowRegs;


    PciIoAddr     = 0;
    VId           = 0;
    DId           = 0;
    Index         = 0;
    ShowRegs      = false;

    printf("Scanning Pcie devices by CAM(IO port) method:\n\n");

    // get permission to access io ports
    // int ioperm(unsigned long from, unsigned long num, int turn_on);
    if (ioperm(PCI_CONFIGURATION_ADDRESS_PORT, 1, 1) || ioperm(PCI_CONFIGURATION_DATA_PORT, 4, 1)) {
      printf("Get io port accessing permission failed.\n");
      return -1;
    }

    // Parse input parameters.
    for (Index = 0; Index < Argc; Index ++) {
      if (strcmp(Argv[Index], "-h") == 0 || strcmp(Argv[Index], "-help") == 0) {
        PrintUsage();
        return 0;
      }
    }

    for (Index = 0; Index < Argc; Index ++) {
      if (strcmp(Argv[Index], "-x") == 0 || strcmp(Argv[Index], "-X") == 0) {
        ShowRegs = true;
      }
    }

    for (Index = 0; Index < Argc - 1; Index ++) {
      if (strcmp(Argv[Index], "-s") == 0 || strcmp(Argv[Index], "-S") == 0) {
        ReturnValue = ParseParameters(Argv[Index + 1], &Bus, &Dev, &Fun);
        if (ReturnValue == -1) {
          printf("Invalid Pci address.\n");
          return -1;
        }

        PciIoAddr = PCI_IO_ADDRESS(Bus, Dev, Fun, 0);
        outl (PciIoAddr, PCI_CONFIGURATION_ADDRESS_PORT);
        VId = inl(PCI_CONFIGURATION_DATA_PORT);
        if (VId == 0xffff) {
          printf("Invalid Pci address.\n");
          return -1;
        }

      DId = inl(PCI_CONFIGURATION_DATA_PORT + 2);
      printf("%02x:%02x.%x  VId = 0x%04x, DId = 0x%04x\n", Bus, Dev, Fun, VId, DId);
      if (ShowRegs) {
        ShowPciConfigSpace(PciIoAddr);
        printf("\n");
      }

      return 0;
    }
  }

    for (BusIndex = 0; BusIndex < MaxBusNum; BusIndex ++) {
      for (DevIndex = 0; DevIndex < MaxDevNum; DevIndex ++) {
        for (FunIndex = 0; FunIndex < MaxFunNum; FunIndex ++) {

          PciIoAddr = PCI_IO_ADDRESS(BusIndex, DevIndex, FunIndex, 0);
          outl (PciIoAddr, PCI_CONFIGURATION_ADDRESS_PORT);
          VId = inl(PCI_CONFIGURATION_DATA_PORT);
          if ((VId == 0xffff) && (FunIndex == 0)) {
            break;
          } else if (VId == 0xffff) {
            continue;
          }

          DId = inl(PCI_CONFIGURATION_DATA_PORT + 2); 

          printf ("%02x:%02x.%x  VId = 0x%04x, DId = 0x%04x\n", BusIndex, DevIndex, FunIndex, VId, DId);
          if (ShowRegs) {
            ShowPciConfigSpace(PciIoAddr);
            printf ("\n");
          }
        }
      }
    }

    return 0;
}