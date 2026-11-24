// Disposable Q35 producer. Distinct raw PTE words exercise all 64 bits of
// the transport independently of an Intel device. It supplies no PCI identity.
const handoff = @import("handoff");
const efi = @import("std").os.uefi;
fn vector(bs: *efi.tables.BootServices, r: anytype, clients: *?handoff.Eg2Clients) !void {
    var device: handoff.Eg2Device = .{ .unused = 0 };
    // EFI InstallProtocolInterface takes EFI_HANDLE * as its first argument.
    const install: *const fn (*?efi.Handle, *const efi.Guid, u32, *anyopaque) callconv(efi.cc) efi.Status =
        @ptrCast(bs._installProtocolInterface);
    var created: ?efi.Handle = null;
    if (install(&created, &handoff.Eg2Device.guid, 0, &device) != .success) return error.InstallProtocol;
    const handle = created orelse return error.NoHandle;
    defer _ = bs._uninstallProtocolInterface(handle, &handoff.Eg2Device.guid, &device);
    _ = (try bs.openProtocol(handoff.Eg2Device, handle, .{
        .by_driver = .{ .agent = efi.handle, .controller = handle },
    })) orelse return error.NoTestProtocol;
    defer bs.closeProtocol(handle, handoff.Eg2Device, efi.handle, handle) catch {};
    const Gop = efi.protocol.GraphicsOutput;
    const outputs = (try bs.locateHandleBuffer(.{ .by_protocol = &Gop.guid })) orelse return error.NoFixtureDisplay;
    defer bs.freePool(@ptrCast(outputs.ptr)) catch {};
    for (outputs) |child| {
        const gop = (try bs.openProtocol(Gop, child, .{ .get_protocol = .{} })) orelse continue;
        const mode = gop.mode;
        if (mode.info.pixel_format == .blt_only or mode.frame_buffer_size == 0 or
            (try bs.openProtocol(efi.protocol.DevicePath, child, .{ .get_protocol = .{} })) == null) continue;
        _ = (try bs.openProtocol(handoff.Eg2Device, handle, .{
            .by_child_controller = .{ .agent = efi.handle, .controller = child },
        })) orelse return error.NoTestChild;
        defer bs.closeProtocol(handle, handoff.Eg2Device, efi.handle, child) catch {};
        _ = (try bs.openProtocol(handoff.Eg2Device, handle, .{
            .by_child_controller = .{ .agent = efi.handle, .controller = efi.handle },
        })) orelse return error.NoSecondChild;
        defer bs.closeProtocol(handle, handoff.Eg2Device, efi.handle, efi.handle) catch {};
        const wrong = try handoff.eg2Clients(bs, handle, mode.frame_buffer_base, mode.frame_buffer_size - 1, null);
        if (wrong.panel == null or wrong.panel.?.gops == 0 or wrong.panel.?.matches != 0) return error.FalseMatch;
        var scanout = r.*;
        scanout.aperture = mode.frame_buffer_base;
        const active = try handoff.framebuffer(bs, &scanout);
        clients.* = try handoff.eg2Clients(bs, handle, mode.frame_buffer_base, mode.frame_buffer_size, active);
        break;
    }
    if (clients.* == null) return error.NoFixtureDisplay;
    for (&r.entries, 0..) |*word, i|
        word.* = 0x000000015a000001 + 0x31000 * @as(u64, i);
    r.state = 2;
    r.step = 6;
}
pub fn main() efi.Status {
    return handoff.run(vector);
}
