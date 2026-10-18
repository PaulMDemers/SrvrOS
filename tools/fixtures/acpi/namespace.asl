DefinitionBlock ("", "DSDT", 2, "SRVROS", "AMLTEST", 1)
{
    OperationRegion (SNAP, SystemMemory, 0x100000, 16)
    Field (SNAP, ByteAcc, NoLock, Preserve) { VAL0, 8 }
    Method (READ, 0) { Return (VAL0) }
    Method (WRIT, 0) { VAL0 = 0x37 Return (VAL0) }
    OperationRegion (PORT, SystemIO, 0x62, 1)
    Field (PORT, ByteAcc, NoLock, Preserve) { IOV0, 8 }
    Method (BADR, 0) { Return (IOV0) }
    Method (LOOP, 0) { While (One) {} Return (Zero) }
    Scope (\_SB)
    {
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
                Method (_STA, 0) { If (_OSI ("Darwin")) { Return (0x0F) } Return (Zero) }
                Device (SBS0) { Name (_HID, "ACPI0002") Name (_SBS, One) }
            }
        }
    }
}
