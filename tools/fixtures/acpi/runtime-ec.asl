DefinitionBlock ("", "SSDT", 2, "SRVROS", "ECTEST", 1)
{
    Scope (\_SB)
    {
        Device (ECT0)
        {
            Name (_HID, "SRV0001")
            Name (REGC, Zero)
            Name (RVAL, Zero)
            Name (DVAL, Zero)
            Name (DLYM, Zero)
            Name (QNUM, Zero)
            Name (QVAL, Zero)
            Mutex (SMTX, 0)
            OperationRegion (ER00, EmbeddedControl, Zero, 0x100)
            Field (ER00, ByteAcc, NoLock, Preserve)
            {
                Offset (0x10), EVAL, 8, EACK, 8,
                Offset (0x20), SPTR, 8, SSTS, 8, SADR, 8, SCMD, 8, SDAT, 16
            }
            Method (_REG, 2, NotSerialized)
            {
                If (Arg0 == 3) {
                    REGC = Arg1
                    If (Arg1) { EACK = 0xA5; RVAL = EVAL }
                    Else {
                        // Each Sleep is below the interpreter's two-second cap.
                        If (DLYM) {
                            // No While: independently exercise the worker's
                            // deadline without hitting the AML loop guard.
                            Sleep (1000); Sleep (1000); Sleep (1000)
                            Sleep (1000); Sleep (1000); Sleep (1000); Sleep (1000)
                        }
                        Sleep (10); EACK = 0x5A; DVAL = EVAL
                    }
                }
            }
            Method (DLYS, 0, NotSerialized) { DLYM = 7 }
            Method (RD00, 0, NotSerialized) { Return (EVAL) }
            Method (_Q20, 0, NotSerialized)
            {
                QVAL = EVAL
                Increment (QNUM)
                Notify (ECT0, 0x80)
            }
            Method (_Q21, 0, NotSerialized)
            {
                If (Acquire (SMTX, 0xFFFF)) { Return () }
                If (SPTR != Zero) { Release (SMTX); Return () }
                SADR = 0x16
                SCMD = 0x09
                SPTR = 0x09
                While (SPTR != Zero) { Sleep (1) }
                QVAL = SDAT
                Increment (QNUM)
                Release (SMTX)
            }
        }
    }
}
