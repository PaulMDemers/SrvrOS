// Disposable Q35 producer. Distinct raw PTE words exercise all 64 bits of
// the transport independently of an Intel device. It supplies no PCI identity.
const handoff = @import("handoff");
const efi = @import("std").os.uefi;
fn vector(_: *efi.tables.BootServices, r: anytype) !void {
    for (&r.entries, 0..) |*word, i|
        word.* = 0x000000015a000001 + 0x31000 * @as(u64, i);
    r.state = 2;
    r.step = 6;
}
pub fn main() efi.Status {
    return handoff.run(vector);
}
