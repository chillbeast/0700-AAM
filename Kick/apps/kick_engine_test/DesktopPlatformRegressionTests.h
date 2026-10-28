// Desktop simulator paths and separation from embedded hardware controls.
// SPDX-License-Identifier: AGPL-3.0-or-later
namespace
{
    void testDesktopPlatform(Results& R)
    {
        R.section("Desktop platform: portable paths and device isolation");
        const auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        R.check(getAppDirs(MTAssets) == exeDir.getChildFile("assets")
            && getAppDirs(MTTemp) == exeDir.getChildFile("templates")
            && getAppDirs(MTDemos) == exeDir.getChildFile("demos"),
            "L1: desktop runtime assets resolve beside the executable");
        R.check(getDefaultPath() == exeDir && getAppDirs(MTProjects) == exeDir.getChildFile("projects"),
            "L1: desktop project defaults do not use embedded device directories");
        const auto overridePath = juce::SystemStats::getEnvironmentVariable("KICK_PREFS_DIR", {});
        R.check(getAppDirs(MTPrefs) == (overridePath.isNotEmpty() ? juce::File(overridePath) : exeDir.getChildFile("prefs")),
            "L1: desktop preferences honor the isolated-directory override");
        mt::I2CDevice i2c;
        uint8_t bytes[]{1, 2};
        const bool wrote = i2c.writeBytes(bytes, 2);
        const bool read = i2c.readBytes(bytes, 2);
        R.check(!i2c.isOpen() && !wrote && !read && bytes[0] == 0 && bytes[1] == 0,
            "L2: desktop CV cannot open, read or write a host I2C device");
        mt::WifiManager wifi;
        R.check(!wifi.connectToWiFi("simulator-test", "unused", 0) && !wifi.disconnectWiFi() && wifi.scanWiFiNetworks().empty(),
            "L2: device Wi-Fi controls are unavailable on the desktop");
        volatile float invalid = std::numeric_limits<float>::quiet_NaN();
        R.check(!std::isfinite(invalid), "L3: desktop compiler flags retain non-finite audio guards");
    }
}
