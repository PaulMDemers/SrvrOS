#ifndef BATTERY_FAULT
#define BATTERY_FAULT 0
#endif
DefinitionBlock ("", "DSDT", 2, "SRVROS", "BATTEST ", BATTERY_FAULT)
{
#if BATTERY_FAULT == 39
    Method (HOLD, 0) {
        If (Acquire (\_GL, 0xFFFF)) { Return (Zero) }
        // The pinned interpreter caps each AML Sleep at 2000 ms.
        Sleep (2000)
        Sleep (2000)
        Sleep (2000)
        Sleep (2000)
        Sleep (2000)
        Sleep (2000)
        Sleep (2000)
        Sleep (2000)
        Sleep (2000)
        Sleep (2000)
        Sleep (2000)
        Sleep (2000)
        Sleep (2000)
        Release (\_GL)
        Return (One)
    }
#endif
    Scope (\_SB) {
        Device (EC) {
            Name (_HID, EisaId ("PNP0C09"))
            Name (_UID, Zero)
            Name (_GPE, One)
            Name (_CRS, ResourceTemplate () {
                IO (Decode16, 0x62, 0x62, 1, 1)
                IO (Decode16, 0x66, 0x66, 1, 1)
            })
            OperationRegion (ER00, EmbeddedControl, Zero, 0x100)
            Field (ER00, ByteAcc, NoLock, Preserve) { Offset (0x10), EACK, 8, Offset (0x60), STAT, 8 }
            OperationRegion (BAD0, SystemIO, 0x1234, 1)
            Field (BAD0, ByteAcc, NoLock, Preserve) { FAIL, 8 }
            Method (_REG, 2) {
                If (Arg0 == 3) {
                    If (Arg1) {
#if BATTERY_FAULT == 1
                        FAIL = One
#endif
#if BATTERY_FAULT == 4
                        Sleep (1000); Sleep (1000); Sleep (1000); Sleep (1000)
                        Sleep (1000); Sleep (1000); Sleep (1000)
#endif
                        EACK = One
                        Notify (EC, 0x80)
                    } Else {
#if BATTERY_FAULT == 47
                        FAIL = One
#endif
#if BATTERY_FAULT == 6
                        Sleep (1000); Sleep (1000); Sleep (1000); Sleep (1000)
#endif
                        EACK = Zero
                    }
                }
            }
            Method (_Q20, 0) {
#if BATTERY_FAULT == 44
                EACK = 7
#endif
#if BATTERY_FAULT == 5
                Sleep (1000); Sleep (1000); Sleep (1000)
#endif
                If (STAT) { Notify (EC, 0x80) }
            }
            Device (SMB0) {
                Name (_HID, "ACPI0001")
                Name (_EC, 0x2010)
                Device (SBS0) { Name (_HID, "ACPI0002"); Name (_SBS, One) }
            }
        }
    }
}
