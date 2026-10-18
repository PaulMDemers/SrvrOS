/* Synthetic VM resource contract; no embedded-controller hardware accesses. */
DefinitionBlock ("", "SSDT", 2, "SRVROS", "MBARES", 1)
{
    External (\_SB.PCI0, DeviceObj)
    Name (OSYS, Zero)
    Method (PINI, 0) { If (_OSI ("Darwin")) { OSYS = 0x2710 } }
    Method (OSDW, 0) { Return (OSYS == 0x2710) }
    Scope (\_SB.PCI0)
    {
        Device (LPCB)
        {
            Name (_ADR, 0x001F0000)
            Device (EC)
            {
                Name (_HID, EisaId ("PNP0C09"))
                Name (_UID, Zero)
                Name (_GPE, 0x4E)
                Name (_CRS, ResourceTemplate () {
                    IO (Decode16, 0x62, 0x62, 0, 1)
                    IO (Decode16, 0x66, 0x66, 0, 1)
                })
                Device (SMB0) {
                    Name (_HID, "ACPI0001")
                    Name (_EC, 0x2010)
                    Method (_STA, 0) { If (OSDW ()) { Return (0x0F) } Return (Zero) }
                    Device (SBS0) { Name (_HID, "ACPI0002") Name (_SBS, One) }
                }
            }
        }
    }
}
