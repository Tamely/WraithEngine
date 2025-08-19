import os
import sys
import subprocess
from pathlib import Path

import Utils
import shutil

from io import BytesIO
from urllib.request import urlopen

class VulkanConfiguration:
    requiredVulkanVersion = "1.4."
    installVulkanVersion = "1.4.309.0"
    vulkanDirectory = "./Wraith/vendor/VulkanSDK"

    @classmethod
    def Validate(cls):
        if (not cls.CheckVulkanSDK()):
            print("Vulkan SDK not installed correctly.")
            return
        else:
            cls.CheckVulkanSDKLibs()

    @classmethod
    def CheckVulkanSDK(cls):
        vulkanSDK = os.environ.get("VULKAN_SDK")
        if (vulkanSDK is None):
            print("\nYou don't have the Vulkan SDK installed!")
            cls.__InstallVulkanSDK()
            return False
        else:
            print(f"\nLocated Vulkan SDK at {vulkanSDK}")

        if (cls.requiredVulkanVersion not in vulkanSDK):
            print(f"You don't have the correct Vulkan SDK version! (Engine requires {cls.requiredVulkanVersion})")
            cls.__InstallVulkanSDK()
            return False
    
        print(f"Correct Vulkan SDK located at {vulkanSDK}")
        return True

    @classmethod
    def CheckVulkanSDKLibs(cls):
        success = False
        vulkanSDK = os.environ.get("VULKAN_SDK")
        shadercdLib = Path(f"{vulkanSDK}/Lib/shaderc_shared.lib")
        success = shadercdLib.exists()

        print("\n")
        cls.CopyVulkanFolders(vulkanSDK, cls.vulkanDirectory)

        return success
    
    @classmethod
    def CopyVulkanFolders(cls, installDir, destDir):
        folders_to_copy = ["Bin", "Bin32", "Lib", "Lib32"]
        for folder in folders_to_copy:
            src_path = os.path.join(installDir, folder)
            dest_path = os.path.join(destDir, folder)
            if os.path.exists(dest_path):
                print(f"The Debug lib folder \"{folder}\" already exists in \"{destDir}\"!");
            else:
                if os.path.exists(src_path):
                    shutil.copytree(src_path, dest_path)
                    print(f"The Debug lib folder \"{folder}\" has been copied to \"{destDir}\"!")
                else:
                    print(f"The \"{src_path}\" does not exist!")

    @classmethod
    def __InstallVulkanSDK(cls):
        permissionGranted = False
        while not permissionGranted:
            reply = str(input("Would you like to install VulkanSDK {0:s}? [Y/N]: ".format(cls.installVulkanVersion))).lower().strip()[:1]
            if reply == 'n':
                return
            permissionGranted = (reply == 'y')

        vulkanInstallURL = f"https://sdk.lunarg.com/sdk/download/{cls.installVulkanVersion}/windows/VulkanSDK-{cls.installVulkanVersion}-Installer.exe"
        vulkanPath = f"{cls.vulkanDirectory}/VulkanSDK-{cls.installVulkanVersion}-Installer.exe"
        print("Downloading {0:s} to {1:s}".format(vulkanInstallURL, vulkanPath))
        Utils.DownloadFile(vulkanInstallURL, vulkanPath)
        print("Running Vulkan SDK installer...")
        os.startfile(os.path.abspath(vulkanPath))
        print("Re-run this script after installation!")
        quit()

    @classmethod
    def CheckVulkanSDKDebugLibs(cls):
        vulkanSDK = os.environ.get("VULKAN_SDK")
        shadercdLib = Path(f"{vulkanSDK}/Lib/shaderc_shared.lib")
        
        return shadercdLib.exists()

if __name__ == "__main__":
    VulkanConfiguration.Validate()