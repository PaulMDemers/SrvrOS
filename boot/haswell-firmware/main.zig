// Inventory the selected Intel GPU through EFI Boot Services before Limine.
// The only write is to a pre-existing, seeded receipt on the application's ESP.
const std = @import("std");
const efi = std.os.uefi;
const Services = efi.tables.BootServices;
const Handle = efi.Handle;
const Status = efi.Status;
const File = efi.protocol.File;
const Gop = efi.protocol.GraphicsOutput;
const cc = efi.cc;

const Pci = extern struct {
    before_config: [6]usize,
    config_read: *const fn (*Pci, u32, u32, usize, *u32) callconv(cc) Status,
    before_location: [7]usize,
    location: *const fn (*Pci, *usize, *usize, *usize, *usize) callconv(cc) Status,

    pub const guid: efi.Guid align(8) = .{
        .time_low = 0x4cf5b200,
        .time_mid = 0x68b8,
        .time_high_and_version = 0x4ca5,
        .clock_seq_high_and_reserved = 0x9e,
        .clock_seq_low = 0xec,
        .node = .{ 0xb2, 0x3e, 0x3f, 0x50, 0x02, 0x9a },
    };

    fn read(p: *Pci, offset: u32) !u32 {
        var value: u32 = undefined;
        if (p.config_read(p, 2, offset, 1, &value) != .success) return error.ConfigRead;
        return value;
    }
};

const DriverBinding = extern struct {
    callbacks: [3]usize,
    version: u32,
    image_handle: Handle,
    binding_handle: Handle,

    pub const guid: efi.Guid align(8) = .{
        .time_low = 0x18a031ab,
        .time_mid = 0xb443,
        .time_high_and_version = 0x4d1a,
        .clock_seq_high_and_reserved = 0xa5,
        .clock_seq_low = 0xc0,
        .node = .{ 0x0c, 0x09, 0x26, 0x1e, 0x9f, 0x71 },
    };
};

const Handles = struct {
    entries: [24]Handle = undefined,
    count: usize = 0,

    fn add(self: *Handles, handle: ?Handle) !usize {
        const h = handle orelse return 0;
        if (std.mem.indexOfScalar(Handle, self.entries[0..self.count], h)) |index| return index + 1;
        if (self.count == self.entries.len) return error.HandleLimit;
        self.entries[self.count] = h;
        self.count += 1;
        return self.count;
    }
};

fn get(comptime Protocol: type, bs: *Services, h: Handle) !*Protocol {
    return (try bs.openProtocol(Protocol, h, .{ .get_protocol = .{} })) orelse error.MissingInterface;
}

fn driverFile(bs: *Services, h: Handle) ?efi.Guid {
    const binding = get(DriverBinding, bs, h) catch null;
    const image = get(efi.protocol.LoadedImage, bs, if (binding) |b| b.image_handle else h) catch return null;
    var node = @as(*const ?*efi.protocol.DevicePath, @ptrCast(&image.file_path)).* orelse return null;
    var remaining: usize = 4096;
    while (remaining >= 4 and node.type != .end) {
        const length = node.length;
        if (length < 4 or length > remaining) return null;
        const bytes: [*]const u8 = @ptrCast(node);
        if (node.type == .media and node.subtype == 6 and length == 20)
            return @bitCast(bytes[4..20].*);
        remaining -= length;
        node = @ptrFromInt(@intFromPtr(node) + length);
    }
    return null;
}

fn controller(bs: *Services) !?Handle {
    const candidates = (try bs.locateHandleBuffer(.{ .by_protocol = &Pci.guid })) orelse return null;
    defer bs.freePool(@ptrCast(candidates.ptr)) catch {};
    if (candidates.len > 128) return error.PciLimit;
    var found: ?Handle = null;
    for (candidates) |h| {
        const pci = try get(Pci, bs, h);
        var segment: usize = undefined;
        var bus: usize = undefined;
        var device: usize = undefined;
        var function: usize = undefined;
        if (pci.location(pci, &segment, &bus, &device, &function) != .success) return error.PciLocation;
        if (segment != 0 or bus != 0 or device != 2 or function != 0) continue;
        if (found != null) return error.DuplicateController;
        if (try pci.read(0) != 0x0a268086 or try pci.read(8) != 0x03000009 or try pci.read(0x2c) != 0x011a106b)
            return null;
        found = h;
    }
    return found;
}

fn writeProtocols(bs: *Services, handles: *Handles, index: usize, w: *std.Io.Writer) !void {
    const h = handles.entries[index - 1];
    const ids = try bs.protocolsPerHandle(h);
    defer bs.freePool(@ptrCast(ids.ptr)) catch {};
    if (ids.len == 0 or ids.len > 32) return error.ProtocolLimit;
    for (ids, 0..) |id, pos| {
        for (ids[0..pos]) |other| {
            if (id.eql(other.*)) return error.DuplicateProtocol;
        }
        try w.print("protocol {d} {f}\n", .{ index, id.* });
        // Only enumerate the controller's relationships. The additional
        // handles are leaves in this inventory, even if they have clients.
        if (index != 1) continue;
        var opens: [*]efi.tables.ProtocolInformationEntry = undefined;
        var count: usize = undefined;
        switch (bs._openProtocolInformation(h, id, &opens, &count)) {
            .success => {},
            .not_found => continue,
            else => return error.OpenInformation,
        }
        if (count == 0) continue;
        defer bs.freePool(@ptrCast(opens)) catch {};
        if (count > 16) return error.OpenLimit;
        for (opens[0..count]) |op| {
            const attributes = @intFromEnum(op.attributes);
            if (attributes & 0x18 == 0) continue; // BY_DRIVER or BY_CHILD_CONTROLLER
            const agent = try handles.add(op.agent_handle);
            const child = try handles.add(op.controller_handle);
            if (agent == 0 or child == 0 or op.open_count == 0) return error.OpenInformation;
            try w.print("open {f} {d} {d} {x} {d}\n", .{ id.*, agent, child, attributes, op.open_count });
        }
    }
    if (index != 1) {
        if (driverFile(bs, h)) |id| try w.print("image {d} {f}\n", .{ index, id });
    }
}

fn writeGops(bs: *Services, handles: *Handles, aperture: u64, w: *std.Io.Writer) !usize {
    const candidates = (try bs.locateHandleBuffer(.{ .by_protocol = &Gop.guid })) orelse return 0;
    defer bs.freePool(@ptrCast(candidates.ptr)) catch {};
    if (candidates.len > 16) return error.GopLimit;
    var matched: usize = 0;
    for (candidates) |h| {
        const gop = try get(Gop, bs, h);
        const mode_ptr = @as(*const ?*Gop.Mode, @ptrCast(&gop.mode)).* orelse continue;
        const mode = mode_ptr.*;
        if (mode.mode >= mode.max_mode or mode.size_of_info < @sizeOf(Gop.Mode.Info)) continue;
        const info_ptr = @as(*const ?*Gop.Mode.Info, @ptrCast(&mode.info)).* orelse continue;
        const info = info_ptr.*;
        if (info.horizontal_resolution != 1366 or info.vertical_resolution != 768) continue;
        if (mode.frame_buffer_base < aperture or mode.frame_buffer_base - aperture >= 0x10000000 or
            mode.frame_buffer_size == 0 or mode.frame_buffer_size > 0x10000000 - (mode.frame_buffer_base - aperture)) continue;
        if (info.pixels_per_scan_line < 1366 or info.pixels_per_scan_line > 8192) return error.InvalidStride;
        const index = try handles.add(h);
        matched += 1;
        const console = h == efi.system_table.console_out_handle;
        try w.print("gop {d} {x} {x} {d} {d}\n", .{
            index, mode.frame_buffer_base, mode.frame_buffer_size, info.pixels_per_scan_line, @intFromBool(console),
        });
    }
    return matched;
}

fn capture(bs: *Services, w: *std.Io.Writer) !void {
    const h = (try controller(bs)) orelse {
        try w.writeAll("absent\n");
        return;
    };
    const pci = try get(Pci, bs, h);
    const bar = try pci.read(0x18);
    const aperture = (@as(u64, try pci.read(0x1c)) << 32) | (bar & 0xfffffff0);
    if (bar & 7 != 4 or aperture == 0 or aperture & 0xfffffff != 0 or (try pci.read(4)) & 2 == 0)
        return error.InvalidAperture;
    try w.print("igd {x}\n", .{aperture});
    var handles: Handles = .{};
    _ = try handles.add(h);
    const gops = try writeGops(bs, &handles, aperture, w);
    var index: usize = 1;
    while (index <= handles.count) : (index += 1) try writeProtocols(bs, &handles, index, w);
    try w.print("complete {d} {d}\n", .{ handles.count, gops });
}

fn receipt(bs: *Services) !void {
    const image = try get(efi.protocol.LoadedImage, bs, efi.handle);
    const fs = try get(efi.protocol.SimpleFileSystem, bs, image.device_handle orelse return error.NoDevice);
    const root = try fs.openVolume();
    defer root.close() catch {};
    const path = std.unicode.utf8ToUtf16LeStringLiteral("\\HSW-EFI.LOG");
    const file = try root.open(path, .read_write, .{});
    defer file.close() catch {};

    const prefix = "SRVROS-HSW-EFI-V1 ";
    const header_size = prefix.len + 16 + 1 + 32 + 1;
    var seed: [4097]u8 = undefined;
    if (try file.read(&seed) != 4096 or !std.mem.startsWith(u8, &seed, prefix) or
        seed[prefix.len + 16] != ' ' or seed[header_size - 1] != '\n' or
        !std.mem.allEqual(u8, seed[header_size..4096], 0)) return error.InvalidSeed;
    for (seed[prefix.len .. header_size - 1], 0..) |c, index| {
        if (index != 16 and !std.ascii.isDigit(c) and (c < 'a' or c > 'f')) return error.InvalidSeed;
    }

    var output = [_]u8{0} ** 4096;
    @memcpy(output[0..header_size], seed[0..header_size]);
    const body_end = output.len - "end 00000000\n".len;
    var writer = std.Io.Writer.fixed(output[header_size..body_end]);
    capture(bs, &writer) catch |err| {
        @memset(output[header_size..], 0);
        writer = std.Io.Writer.fixed(output[header_size..body_end]);
        try writer.print("unavailable {s}\n", .{@errorName(err)});
    };
    const checksum = std.hash.Crc32.hash(writer.buffered());
    _ = try std.fmt.bufPrint(output[header_size + writer.end ..], "end {x:0>8}\n", .{checksum});

    try file.setPosition(0);
    if (try file.write(&output) != output.len) return error.ShortWrite;
    try file.flush();
}

pub fn main() Status {
    receipt(efi.system_table.boot_services orelse return .unsupported) catch return .device_error;
    return .success;
}
