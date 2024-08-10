import os
import sys
import getopt
import portio

MaxBusNum                         =   256
MaxDevNum                         =   32
MaxFuncNum                        =   8

PCI_CONFIGURATION_ADDRESS_PORT    = 0xCF8
PCI_CONFIGURATION_DATA_PORT       = 0xCFC

def PCI_IO_ADDRESS (Bus, Dev, Func, Reg):
    return 0x80000000+(Bus<<16)+(Dev<<11)+(Fun<<8)+Reg

def EnableIoAccess ():
    return portio.iopl(3)

def DisableIoAccess ():
    return portio.iopl(0)

def ShowPciConfigSpace (Addr):
    for Index in range(0, 0x1000):
        if Index == 0x100 and MmioRead(Addr + Index, 4) == [0, 0, 0, 0]:
            break
        Data = MmioRead(Addr + Index, 1)
        print ("%02x " %(Data[0]), end="")
        if (Index + 1) % 16 == 0:
            print("")
    
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

    # check for root privileges
    if os.getuid():
        print('This program needs to be root! Exiting.')
        sys.exit()

    InputPcieDev = ""
    ShowRegs = False
    print("\nPython tool for doing Pcie devices by CAM(CPU IO):\n")

    # Parse input parameters.
    opts,args = getopt.getopt(sys.argv[1:], "hHxXs:S:", )
    for opt_name,opt_value in opts:
        if opt_name in ("-h", "-H"):
            HelpMessage()
            sys.exit()
        elif opt_name in ("-x", "-X"):
            ShowRegs = True
        elif opt_name in ("-s", "-S"):
            InputPcieDev = opt_value

    ReturnVal = EnableIoAccess()
    if ReturnVal != 0:
        print('Get io port accessing permission failed.')
        DisableIoAccess()
        sys.exit()

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
            PcieIoAddr = PCI_IO_ADDRESS(Bus, Dev, Func, 0)
            VId =
            if VId[0] == 0xff and VId[1] == 0xff:
                print("Invalid Pci address")
                sys.exit()
            DId = 
            print("%02x:%02x.%x  VId = 0x%02x%02x, DId = 0x%02x%02x" %(Bus, Dev, Func, VId[1], VId[0], DId[1], DId[0]))
            if ShowRegs == True:
                ShowPciConfigSpace(PcieIoAddr)
                print("\n")
    else:
        for Bus in range(0, MaxBusNum):
            for Dev in range(0, MaxDevNum):
                for Func in range(0, MaxFuncNum):
                    PcieIoAddr = PCI_IO_ADDRESS(Bus, Dev, Func, 0)
                    VId = 
                    if VId[0] == 0xff and VId[1] == 0xff:
                        continue
                    DId = 
                    print("%02x:%02x.%x  VId = 0x%02x%02x, DId = 0x%02x%02x" %(Bus, Dev, Func, VId[1], VId[0], DId[1], DId[0]))
                    if ShowRegs == True:
                        ShowPciConfigSpace(PcieIoAddr)
                        print("\n")