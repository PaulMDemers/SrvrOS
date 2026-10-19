DefinitionBlock ("", "SSDT", 2, "SRVROS", "WORKTEST", 1)
{
    Mutex (MTX0, 0)
    Name (CNT0, Zero)
    Name (HELD, Zero)
    Method (TST0, 0, NotSerialized)
    {
        If (Acquire (MTX0, 0xFFFF)) { Return (Ones) }
        HELD = One
        Sleep (100)
        Increment (CNT0)
        Local0 = CNT0
        HELD = Zero
        Release (MTX0)
        Return (Local0)
    }
}
