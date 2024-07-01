#include "stdio.h"
#include "sys/mman.h"
#include "string.h"
#include "stdbool.h"
#include "fcntl.h"
#include "unistd.h"

#define MaxBusNum                            256
#define MaxDevNum                            32
#define MaxFunNum                            8

#define PCI_MMIO_ADDRESS(Bus, Dev, Fun, Reg) (Bus<<20)+(Dev<<15)+(Fun<<12)+Reg
#define INTEL_PCIE_MMIO_BASE_ADDR            0xE0000000
#define AMD_PCIE_MMIO_BASE_ADDR              0xF0000000

#define BOUNDRY_46BIT                        0x3FFFFFFFFFFF // 64TB
#define MAX_MEM_ADDR                         ((unsigned long long)BOUNDRY_46BIT)

/**
  Print usage.
**/
void
PrintUsage (
  void
  )
{
  printf("PcieEcamTool:  usage\n");
  printf("  PcieEcamTool -h\n");
  printf("  PcieEcamTool -x\n");
  printf("  PcieEcamTool -s Bus:Dev.Fun\n");
  printf("  PcieEcamTool\n");
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

  InputStrLen = Index;

  if ((SemicolonPosition > 2) || (DotPisition - SemicolonPosition > 3) || (InputStrLen - DotPisition > 2)) {
    return -1; 
  }

  *Bus = HexStrToUint16(InputStr[0], InputStr[SemicolonPosition - 1]);
  *Dev = HexStrToUint16(InputStr[SemicolonPosition + 1], InputStr[DotPisition - 1]);
  *Fun = HexStrToUint16(InputStr[DotPisition + 1], InputStr[InputStrLen - 1]);

  if (*Bus == -1 || *Dev == -1 || *Fun == -1) return -1;

  printf ("InputStrLen = %d, Bus = %x, Dev = %x, Fun = %x\n", InputStrLen, *Bus, *Dev, *Fun);

  return 0;
}

// This functino calculate a page aligned base and offset
int
AddrToMemPageBaseOffset (
  unsigned long          Addr,
  unsigned long          Length,
  size_t                 *Map_Size,
  unsigned long          *Base,
  unsigned long          *Offset
) {
  size_t                 SystemPageSize = sysconf(_SC_PAGE_SIZE);

  *Base   = Addr & (~(SystemPageSize -1));
  *Offset = Addr & (SystemPageSize -1);

  // *Map_Size is supposed to be at least the size of *Offset + Length, and should be the integer multiples of SystemPageSize;
  if ((*Offset + Length) > SystemPageSize) {
    *Map_Size = SystemPageSize * 2;
  } else {
    *Map_Size = SystemPageSize;
  }

  return 0;
}

int
MmioRead (
  unsigned long           PcieMmioAddr,
  unsigned int            Length,
  unsigned long           *Data
) {
  int                     Fd;
  volatile unsigned char  *Ptr_Mem;
  size_t                  Map_Size;
  unsigned long           Base;
  unsigned long           Offset;
  int                     ReturnValue;

  Fd                      = -1;
  Ptr_Mem                 = NULL;

  if (Data == NULL || (Length > sizeof(unsigned long long))) {
    return -1;
  }

  if (PcieMmioAddr > MAX_MEM_ADDR) {
    printf("ERROR: Memory address is greater than MAX_MEM_ADDR(0x%llx) supported.\n", MAX_MEM_ADDR);
    return -1;
  }

  // transform mapped physical address on mem page.
  ReturnValue = AddrToMemPageBaseOffset (PcieMmioAddr, Length, &Map_Size, &Base, &Offset);
  if (ReturnValue != 0) {
    return -1;
  }

  // Open mem device
  Fd = open("/dev/mem", O_RDWR);
  if (Fd < 0) {
    printf("ERROR: Failed to open %s.\n", "/dev/mem");
    ReturnValue = -1;

    goto Error;
  }

  Ptr_Mem = (volatile unsigned char*)mmap(NULL, Map_Size, PROT_READ, MAP_SHARED, Fd, Base);
  if (Ptr_Mem == (void *)-1) {
    printf("ERROR: Memory mapped failed.\n");
    ReturnValue = -1;
    goto Error;
  }

  if (Length == sizeof(unsigned char)) {
    *Data = *(volatile unsigned char*)(Ptr_Mem + Offset);
  } else if (Length == sizeof(unsigned short)) {
    *Data = *(volatile unsigned short*)(Ptr_Mem + Offset);
  } else if (Length == sizeof(unsigned int)) {
    *Data = *(volatile unsigned int*)(Ptr_Mem + Offset);
  } else if (Length == sizeof(unsigned long long)) {
    *Data = *(volatile unsigned long long*)(Ptr_Mem + Offset);
  }

Error:
  if (Fd >= 0) {
    close (Fd);
  }

  if (Ptr_Mem != NULL) {
    munmap ((void *)Ptr_Mem, Map_Size);
  }

  return ReturnValue;
}

unsigned char
MmioRead8 (
  unsigned long    Addr
) {
  unsigned long   Data;

  Data = 0;
  MmioRead (Addr, sizeof(unsigned char), &Data);

  return (unsigned char)Data;
}

unsigned short
MmioRead16 (
  unsigned long    Addr
) {
  unsigned long   Data;

  Data = 0;
  MmioRead (Addr, sizeof(unsigned short), &Data);

  return (unsigned short)Data;
}

unsigned int
MmioRead32 (
  unsigned long    Addr
) {
  unsigned long   Data;

  Data = 0;
  MmioRead (Addr, sizeof(unsigned int), &Data);

  return (unsigned int)Data;
}

void
ShowPciConfigSpace (
  unsigned long    PcieMmioAddr
) {
  unsigned short   Index;
  unsigned char    Data;

  for (Index = 0; Index <= 0xFFF; Index ++) {
    Data = MmioRead8 (PcieMmioAddr + Index);
    printf ("%02x ", Data);
    if ((Index + 1) % 16 == 0 ) printf ("\n");
    if (Index == 0x100 && MmioRead32 (PcieMmioAddr + Index) == 0) break;
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
  unsigned long PcieMmioAddr;
  unsigned int  VId;
  unsigned int  DId;
  int           ReturnValue;
  unsigned int  Index;
  bool          ShowRegs;

  PcieMmioAddr  = 0;
  VId           = 0;
  DId           = 0;
  Index         = 0;
  ShowRegs      = false;

  printf("Scanning Pcie devices by ECAM(MMIO) method:\n\n");

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

      PcieMmioAddr = AMD_PCIE_MMIO_BASE_ADDR + PCI_MMIO_ADDRESS(Bus, Dev, Fun, 0);
      VId = MmioRead16 (PcieMmioAddr);
      if (VId == 0xffff) {
        printf("Invalid Pci address.\n");
        return -1;
      }

      DId = MmioRead16 (PcieMmioAddr + 2);
      printf("%02x:%02x.%x  VId = 0x%04x, DId = 0x%04x\n", Bus, Dev, Fun, VId, DId);
      if (ShowRegs) {
        ShowPciConfigSpace(PcieMmioAddr);
        printf("\n");
      }
    return 0;
    }
  }

  for (BusIndex = 0; BusIndex < MaxBusNum; BusIndex ++) {
    for (DevIndex = 0; DevIndex < MaxDevNum; DevIndex ++) {
      for (FunIndex = 0; FunIndex < MaxFunNum; FunIndex ++) {

        PcieMmioAddr = AMD_PCIE_MMIO_BASE_ADDR + PCI_MMIO_ADDRESS(BusIndex, DevIndex, FunIndex, 0);
        VId = MmioRead16 (PcieMmioAddr);
        if ((VId == 0xffff) && (FunIndex == 0)) {
          break;
        } else if (VId == 0xffff) {
          continue;
        }

        DId = MmioRead16 (PcieMmioAddr + 2);
        printf ("%02x:%02x.%x  VId = 0x%04x, DId = 0x%04x\n", BusIndex, DevIndex, FunIndex, VId, DId);
        if (ShowRegs) {
          ShowPciConfigSpace(PcieMmioAddr);
          printf ("\n");
        }
      }
    }
  }

  return 0;
}