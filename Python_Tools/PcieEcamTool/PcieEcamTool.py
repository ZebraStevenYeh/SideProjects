import os
import mmap
import sys
import getopt

MaxBusNum                         =   256
MaxDevNum                         =   32
MaxFuncNum                        =   8

AMD_PCIE_MMIO_BASE_ADDR           =  0xF0000000
INTEL_PCIE_MMIO_BASE_ADDR         =  0xE0000000
MAX_MEM_ADDR                      =  0x3FFFFFFFFFFF  # 64TB

def PCI_MMIO_ADDRESS (Bus, Dev, Func, Reg):
    return (Bus<<20)+(Dev<<15)+(Func<<12)+Reg

def MmioRead (Addr, Len):
    MAP_MASK = mmap.PAGESIZE - 1
    Fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
    Ptr_Mem = mmap.mmap(Fd, mmap.PAGESIZE, mmap.MAP_SHARED, mmap.PROT_WRITE | mmap.PROT_READ, offset = (Addr & ~MAP_MASK))
    Ptr_Mem.seek(Addr & MAP_MASK)
    Data_Arr = []
    for i in range(0, Len):
        Val = Ptr_Mem.read_byte()
        Data_Arr.append(Val)
        #print(hex(Val))
    Ptr_Mem.close()
    os.close(Fd)

    return Data_Arr

def ShowPciConfigSpace (Addr):
    for Index in range(0, 0x1000):
        Data = MmioRead(Addr + Index, 1)
        print ("%02x " %(Data[0]), end="")
        if ((Index + 1) % 16 == 0):
            print("")
        if (Index == 0x100 and MmioRead(Addr + Index, 1) == 0):
            break
    
def HelpMessage ():
    print("PcieEcamTool:  usage")
    print("  PcieEcamTool -h")
    print("  PcieEcamTool -x")
    print("  PcieEcamTool -s Bus:Dev.Func")
    print("  PcieEcamTool")
    print("Parameter:")
    print("  -h -help: show instructions")
    print("  -s: specify a pcie device with the format of Bus:Dev.Func")
    print("  -x: show pcie configuration space")

def HexStringToInt (str):
    Val = str.lower()
    return int(Val, 16)

if __name__ == "__main__":

    InputPcieDev = ""
    ShowRegs = False
    print("\nPython tool for doing Pcie devices by ECAM(MMIO):\n")

    # Parse input parameters.
    opts,args = getopt.getopt(sys.argv[1:], "hHxXs:S:", )
    for opt, arg in opts:
        if opt in ("-h", "-H"):
            HelpMessage()
            sys.exit()
        elif opt in ("-x", "-X"):
            ShowRegs = True
        elif opt in ("-s", "-S"):
            InputPcieDev = arg

    if InputPcieDev != "":
        parts = InputPcieDev.split(":")
        Bus = HexStringToInt(parts[0])
        subparts = parts[1].split(".")
        Dev = HexStringToInt(subparts[0])
        Func = int(subparts[1])
        # print (Bus, Dev, Func)
        if Bus < 0 or Bus >= MaxBusNum or Dev < 0 or Dev >= MaxDevNum or Func < 0 or Func >= MaxFuncNum:
            print("Input parameter for Bus:Dev.Func is wrong.\n")
            HelpMessage()
            sys.exit()
        else:
            PcieMmioAddr = AMD_PCIE_MMIO_BASE_ADDR + PCI_MMIO_ADDRESS(Bus, Dev, Func, 0)
            VId = MmioRead (PcieMmioAddr, 2)
            if VId[0] == 0xff and VId[1] == 0xff:
                print("Invalid Pci address")
                sys.exit()
            DId = MmioRead (PcieMmioAddr + 2, 2)
            print("%02x:%02x.%x  VId = 0x%02x%02x, DId = 0x%02x%02x" %(Bus, Dev, Func, VId[1], VId[0], DId[1], DId[0]))
            if ShowRegs == True:
                ShowPciConfigSpace(PcieMmioAddr)
                print("\n")
    else:
        for Bus in range(0, MaxBusNum):
            for Dev in range(0, MaxDevNum):
                for Func in range(0, MaxFuncNum):
                    PcieMmioAddr = AMD_PCIE_MMIO_BASE_ADDR + PCI_MMIO_ADDRESS(Bus, Dev, Func, 0)
                    VId = MmioRead (PcieMmioAddr, 2)
                    if VId[0] == 0xff and VId[1] == 0xff:
                        continue
                    DId = MmioRead (PcieMmioAddr + 2, 2)
                    print("%02x:%02x.%x  VId = 0x%02x%02x, DId = 0x%02x%02x" %(Bus, Dev, Func, VId[1], VId[0], DId[1], DId[0]))
                    if ShowRegs == True:
                        ShowPciConfigSpace(PcieMmioAddr)
                        print("\n")
