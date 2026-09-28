DefinitionBlock ("", "SSDT", 2, "SRVROS", "A1466SPI", 0x00000001)
{
    External (\_SB.PCI0, DeviceObj)

    Scope (\_SB.PCI0)
    {
        Device (SPI1)
        {
            Name (_ADR, 0x00160000)

            Device (SPIT)
            {
                Name (_HID, "APP000D")
                Name (_CID, "APPLE-SPI-TOPCASE")
                Name (_UID, One)
                Name (SPEN, Zero)

                Name (_CRS, ResourceTemplate ()
                {
                    SpiSerialBusV2 (0x0000, PolarityLow, FourWireMode, 0x08,
                        ControllerInitiated, 0x007A1200, ClockPolarityLow,
                        ClockPhaseFirst, "\\_SB.PCI0.SPI1",
                        0x00, ResourceConsumer, , Exclusive,
                        )
                    Interrupt (ResourceConsumer, Level, ActiveLow, Shared, ,, )
                    {
                        0x0000000B,
                    }
                })

                Method (SIEN, 1, NotSerialized)
                {
                    SPEN = Arg0
                    Return (SPEN)
                }

                Method (SIST, 0, NotSerialized)
                {
                    Return (SPEN)
                }

                Method (UIEN, 1, NotSerialized)
                {
                    SPEN = !Arg0
                    Return (!SPEN)
                }

                Method (UIST, 0, NotSerialized)
                {
                    Return (!SPEN)
                }

                Name (_DSD, Package ()
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                    Package ()
                    {
                        Package () { "spiCSDelay", Buffer (8) { 10, 0, 0, 0, 0, 0, 0, 0 } },
                        Package () { "resetA2RUsec", Buffer (8) { 10, 0, 0, 0, 0, 0, 0, 0 } },
                        Package () { "resetRecUsec", Buffer (8) { 10, 0, 0, 0, 0, 0, 0, 0 } },
                    }
                })
            }
        }
    }
}
