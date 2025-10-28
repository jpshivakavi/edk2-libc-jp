## @file
#   Intel(r) UEFI Application Development Kit for EDK II.
#   This package contains applications which depend upon Standard Libraries
#   from the StdLib package.
#
#   See the comments in the [LibraryClasses.IA32] and [BuildOptions] sections
#   for important information about configuring this package for your
#   environment.
#
#   Copyright (c) 2010 - 2024, Intel Corporation. All rights reserved.<BR>
#   SPDX-License-Identifier: BSD-2-Clause-Patent
##

[Defines]
  PLATFORM_NAME                  = AppPkg
  PLATFORM_GUID                  = 0458dade-8b6e-4e45-b773-1b27cbda3e06
  PLATFORM_VERSION               = 0.01
  DSC_SPECIFICATION              = 0x00010006
  OUTPUT_DIRECTORY               = Build/AppPkg
  SUPPORTED_ARCHITECTURES        = IA32|X64|ARM|AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT

#
#  Debug output control
#
  DEFINE DEBUG_ENABLE_OUTPUT      = FALSE       # Set to TRUE to enable debug output
  DEFINE DEBUG_PRINT_ERROR_LEVEL  = 0x80000040  # Flags to control amount of debug output
  DEFINE DEBUG_PROPERTY_MASK      = 0

!include MdePkg/MdeLibs.dsc.inc

[PcdsFeatureFlag]

[PcdsFixedAtBuild]
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|$(DEBUG_PROPERTY_MASK)
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|$(DEBUG_PRINT_ERROR_LEVEL)
  
  # Enable all crypto services for SSL support
  gEfiCryptoPkgTokenSpaceGuid.PcdCryptoServiceFamilyEnable|{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

[LibraryClasses]
  #
  # Entry Point Libraries
  #
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  ShellCEntryLib|ShellPkg/Library/UefiShellCEntryLib/UefiShellCEntryLib.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  #
  # Common Libraries
  #
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  !if $(DEBUG_ENABLE_OUTPUT)
    DebugLib|MdePkg/Library/UefiDebugLibConOut/UefiDebugLibConOut.inf
    DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  !else   ## DEBUG_ENABLE_OUTPUT
    DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  !endif  ## DEBUG_ENABLE_OUTPUT

  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  PeCoffGetEntryPointLib|MdePkg/Library/BasePeCoffGetEntryPointLib/BasePeCoffGetEntryPointLib.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  PciLib|MdePkg/Library/BasePciLibCf8/BasePciLibCf8.inf
  PciCf8Lib|MdePkg/Library/BasePciCf8Lib/BasePciCf8Lib.inf
  SynchronizationLib|MdePkg/Library/BaseSynchronizationLib/BaseSynchronizationLib.inf
  UefiRuntimeLib|MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf
  HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
  UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
  PerformanceLib|MdeModulePkg/Library/DxePerformanceLib/DxePerformanceLib.inf
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  FileHandleLib|MdePkg/Library/UefiFileHandleLib/UefiFileHandleLib.inf
  SortLib|MdeModulePkg/Library/UefiSortLib/UefiSortLib.inf

  ShellLib|ShellPkg/Library/UefiShellLib/UefiShellLib.inf

  CacheMaintenanceLib|MdePkg/Library/BaseCacheMaintenanceLib/BaseCacheMaintenanceLib.inf
  
  # CCB Libraries
  CCDebugLib|AppPkg/Applications/CCB/Packages/Debug/EFIX64/Debug/Debug.inf
  CCHWAPILib|AppPkg/Applications/CCB/Packages/HWAPILib/EFIX64/Debug/Hwapi.inf
  CCOSIntLib|AppPkg/Applications/CCB/Packages/OSInterface/EFIX64/Debug/OSInterface.inf
  CCContainerLib|AppPkg/Applications/CCB/Packages/Container/EFIX64/Debug/Container.inf
  CCSMBIOSLib|AppPkg/Applications/CCB/Packages/SMBIOSLib/EFIX64/Debug/SMBIOSLib.inf
  UserCPUUtility|AppPkg/Applications/CCB/Packages/UserCPUUtility/EFIX64/Debug/UserCPUUtility.inf
  Container|AppPkg\Applications\CCB\Packages\Container\EFIX64\Debug\Container.inf
  CCXMLLib|AppPkg\Applications\CCB\Packages\XMLParserLib\EFIX64\Debug\XMLParser.inf
  MLib|AppPkg\Applications\CCB\Packages\XMLParserLib\mxmlsrc\EFIX64\Debug\mxml.inf

  # Dependencies of CCB Libraries
  SerialPortLib|MdeModulePkg/Library/BaseSerialPortLib16550/BaseSerialPortLib16550.inf
  TimerLib|MdePkg/Library/BaseTimerLibNullTemplate/BaseTimerLibNullTemplate.inf
  UefiDecompressLib|MdePkg/Library/BaseUefiDecompressLib/BaseUefiDecompressLib.inf
  UefiScsiLib|MdePkg/Library/UefiScsiLib/UefiScsiLib.inf
  UefiUsbLib|MdePkg/Library/UefiUsbLib/UefiUsbLib.inf
  PlatformHookLib|MdeModulePkg/Library/BasePlatformHookLibNull/BasePlatformHookLibNull.inf
  PathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  CpuLib|MdePkg/Library/BaseCpuLib/BaseCpuLib.inf

  # SSL specific Dependencies - minimal set, no IntrinsicLib to avoid conflicts
  OpensslLib|CryptoPkg/Library/OpensslLib/OpensslLib.inf
  SafeIntLib|MdePkg/Library/BaseSafeIntLib/BaseSafeIntLib.inf
  RngLib|MdePkg/Library/BaseRngLib/BaseRngLib.inf

###################################################################################################
#
# Components Section - list of the modules and components that will be processed by compilation
#                      tools and the EDK II tools to generate PE32/PE32+/Coff image files.
#
# Note: The EDK II DSC file is not used to specify how compiled binary images get placed
#       into firmware volume images. This section is just a list of modules to compile from
#       source into UEFI-compliant binaries.
#       It is the FDF file that contains information on combining binary files into firmware
#       volume images, whose concept is beyond UEFI and is described in PI specification.
#       Binary modules do not need to be listed in this section, as they should be
#       specified in the FDF file. For example: Shell binary (Shell_Full.efi), FAT binary (Fat.efi),
#       Logo (Logo.bmp), and etc.
#       There may also be modules listed in this section that are not required in the FDF file,
#       When a module listed here is excluded from FDF file, then UEFI-compliant binary will be
#       generated for it, but the binary will not be put into any firmware volume.
#
###################################################################################################

#### EfiPy::EfiPy.inf BEGIN
[LibraryClasses.Common.UEFI_APPLICATION]
  EfiPy|AppPkg/EfiPy/EfiPy.inf
  LoLeOp|AppPkg/LoLeOp/LoLeOp.inf
#### EfiPy::EfiPy.inf END

[Components]

#### Sample Applications.
  AppPkg/Applications/Hello/Hello.inf        # No LibC includes or functions.
  AppPkg/Applications/Main/Main.inf          # Simple invocation. No other LibC functions.
  AppPkg/Applications/Enquire/Enquire.inf    #
  AppPkg/Applications/ArithChk/ArithChk.inf  #

#### A simple fuzzer for OrderedCollectionLib, in particular for
#### BaseOrderedCollectionRedBlackTreeLib.
  AppPkg/Applications/OrderedCollectionTest/OrderedCollectionTest.inf {
    <LibraryClasses>
      OrderedCollectionLib|MdePkg/Library/BaseOrderedCollectionRedBlackTreeLib/BaseOrderedCollectionRedBlackTreeLib.inf
      DebugLib|MdePkg/Library/UefiDebugLibConOut/UefiDebugLibConOut.inf
      DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
    <PcdsFeatureFlag>
      gEfiMdePkgTokenSpaceGuid.PcdValidateOrderedCollection|TRUE
    <PcdsFixedAtBuild>
      gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x2F
      gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80400040
  }

#### Conditional compilation of python368.inf by passing -D BUILD_PYTHON368
#### through build command
  !if $(BUILD_PYTHON368)
  AppPkg/Applications/CCB/Products/Python/Python-3.6.8/Python368.inf {
    <PcdsFixedAtBuild>
      # Disable problematic crypto service functions that conflict with StdLib
      gEfiCryptoPkgTokenSpaceGuid.PcdCryptoServiceFamilyEnable|{0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF}
  }
  !endif

#### Un-comment the following line to build Lua.
#  AppPkg/Applications/Lua/Lua.inf


##############################################################################
#
# Specify whether we are running in an emulation environment, or not.
# Define EMULATE if we are, else keep the DEFINE commented out.
#
# DEFINE  EMULATE = 1

##############################################################################
#
#  Include Boilerplate text required for building with the Standard Libraries.
#
##############################################################################
!include StdLib/StdLib.inc
!include AppPkg/Applications/Sockets/Sockets.inc
