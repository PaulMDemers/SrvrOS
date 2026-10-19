// Minimal namespace for initialization ordering; no native device definition.
DefinitionBlock ("", "DSDT", 2, "SRVROS", "INITTEST", 1)
{
    Name (INIT, Zero)
    Method (_INI, 0, NotSerialized) { INIT = One }
    Scope (\_SB) {
        Method (_INI, 0, NotSerialized) { If (INIT == One) { INIT = 2 } }
    }
}
