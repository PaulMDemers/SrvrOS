// EFI application for the Air blitter trial. Take the snapshot before Limine
// exits boot services. Child image receives the original boot device path.
const std = @import("std");
const efi = std.os.uefi;
const wire = @cImport(@cInclude("srvros/air_gpu_boot.h"));
const Record = wire.struct_air_gpu_boot;
const cc = efi.cc;
const Services = efi.tables.BootServices;
const Status = efi.Status;
const Handle = efi.Handle;

fn guid(a: u32, b: u16, c: u16, d: [8]u8) efi.Guid {
    return .{ .time_low = a, .time_mid = b, .time_high_and_version = c, .clock_seq_high_and_reserved = d[0], .clock_seq_low = d[1], .node = d[2..8].* };
}
const Acpi = extern struct {
    install: *const fn (*Acpi, *const anyopaque, usize, *usize) callconv(cc) Status,
    uninstall: *const fn (*Acpi, usize) callconv(cc) Status,
    pub const guid align(8) = makeAcpiGuid();
    fn makeAcpiGuid() efi.Guid {
        return makeGuid(0xffe06bdd, 0x6107, 0x46a6, .{ 0x7b, 0xb2, 0x5a, 0x9c, 0x7e, 0xc5, 0x27, 0x5c });
    }
};
const makeGuid = guid;

// EFI_PCI_IO_PROTOCOL through GetLocation. Only the two reads and GetLocation
// are called. EFI status is checked before using any output.
const Pci = extern struct {
    poll_mem: usize,
    poll_io: usize,
    mem_read: *const fn (*Pci, u32, u8, u64, usize, *u32) callconv(cc) Status,
    mem_write: usize,
    io: [2]usize,
    cfg_read: *const fn (*Pci, u32, u32, usize, *u32) callconv(cc) Status,
    cfg_write: usize,
    copy: usize,
    map: usize,
    unmap: usize,
    allocate: usize,
    free: usize,
    flush: usize,
    location: *const fn (*Pci, *usize, *usize, *usize, *usize) callconv(cc) Status,
    pub const guid align(8) = makeGuid(0x4cf5b200, 0x68b8, 0x4ca5, .{ 0x9e, 0xec, 0xb2, 0x3e, 0x3f, 0x50, 0x02, 0x9a });
    fn cfg(p: *Pci, offset: u32) !u32 {
        var value: u32 = undefined;
        if (p.cfg_read(p, 2, offset, 1, &value) != .success) return error.PciRead;
        return value;
    }
    fn word(p: *Pci, offset: u64) !u32 {
        var value: u32 = undefined;
        if (p.mem_read(p, 2, 0, offset, 1, &value) != .success or value == 0xffffffff) return error.MmioRead;
        return value;
    }
};
const ImagePath = extern struct {
    pub const guid = efi.protocol.LoadedImage.device_path_guid;
};
fn snapshot(p: *Pci, r: *Record) !void {
    const plane = try p.word(0x70180);
    const surface = try p.word(0x701ac);
    if (plane & (1 << 31) == 0 or surface & 4095 != 0 or ((try p.word(0x7019c)) & ~@as(u32, 8)) != surface)
        return error.PlaneChanged;
    if (r.plane != 0 and (r.plane != plane or r.surface != surface)) return error.PlaneChanged;
    for (0..wire.AIR_BOOT_PAGES) |i| {
        const offset = 0x800000 + 8 * (wire.AIR_BOOT_SLOT + i);
        const hi = try p.word(offset + 4);
        const value = (@as(u64, hi) << 32) | (try p.word(offset));
        if (hi != try p.word(offset + 4) or value & 1 == 0 or (r.plane != 0 and r.entries[i] != value))
            return error.TranslationChanged;
        r.entries[i] = value;
    }
    if ((try p.word(0x70180)) != plane or (try p.word(0x701ac)) != surface or
        ((try p.word(0x7019c)) & ~@as(u32, 8)) != surface) return error.PlaneChanged;
    r.plane = plane;
    r.surface = surface;
}
const DriverBinding = extern struct {
    supported: usize,
    start: usize,
    stop: usize,
    version: u32,
    image: Handle,
    binding: Handle,
    pub const guid align(8) = makeGuid(0x18a031ab, 0xb443, 0x4d1a, .{ 0xa5, 0xc0, 0x0c, 0x09, 0x26, 0x1e, 0x9f, 0x71 });
};
fn driverFile(bs: *Services, agent: Handle) ?[16]u8 {
    const binding = bs.openProtocol(DriverBinding, agent, .{ .get_protocol = .{} }) catch null;
    const image = (bs.openProtocol(efi.protocol.LoadedImage, if (binding) |b| b.image else agent, .{ .get_protocol = .{} }) catch null) orelse return null;
    var node = image.file_path;
    var bytes: usize = 0;
    while (bytes < 4096 and node.type != .end) {
        const length = node.length;
        if (length < 4 or length > 4096 - bytes) return null;
        const data: [*]const u8 = @ptrCast(node);
        if (node.type == .media and node.subtype == 6 and length == 20) return data[4..20].*;
        bytes += length;
        node = @ptrFromInt(@intFromPtr(node) + length);
    }
    return null;
}
// The console handle may be a console splitter. Match a GOP on the active
// surface by physical address rather than treating the first GOP as primary.
fn framebuffer(bs: *Services, r: *Record) !void {
    const Gop = efi.protocol.GraphicsOutput;
    const handles = (try bs.locateHandleBuffer(.{ .by_protocol = &Gop.guid })) orelse return error.NoGop;
    defer bs.freePool(@ptrCast(handles.ptr)) catch {};
    for (handles) |handle| {
        const g = (try bs.openProtocol(Gop, handle, .{ .get_protocol = .{} })) orelse continue;
        const base = g.mode.frame_buffer_base;
        const size = g.mode.frame_buffer_size;
        if (base < r.aperture or base - r.aperture != r.surface or size == 0 or
            g.mode.info.pixel_format == .blt_only) continue;
        if (r.framebuffer_bytes != 0 and r.framebuffer_bytes != size) return error.ConflictingGop;
        r.framebuffer = base;
        r.framebuffer_bytes = size;
    }
    if (r.framebuffer_bytes == 0) return error.NoGop;
}
fn capture(bs: *Services, r: *Record) !void {
    const handles = (try bs.locateHandleBuffer(.{ .by_protocol = &Pci.guid })) orelse return;
    defer bs.freePool(@ptrCast(handles.ptr)) catch {};
    for (handles) |handle| {
        const p = (try bs.openProtocol(Pci, handle, .{ .get_protocol = .{} })) orelse continue;
        var seg: usize = 0;
        var bus: usize = 0;
        var dev: usize = 0;
        var func: usize = 0;
        if (p.location(p, &seg, &bus, &dev, &func) != .success or seg != 0 or bus != 0 or dev != 2 or func != 0) continue;
        if (try p.cfg(0) != 0x16268086 or try p.cfg(0x2c) != 0x011b106b) continue;
        r.state = wire.AIR_BOOT_PROBING;
        r.step = 1;
        r.pci_id = try p.cfg(0);
        r.subsystem = try p.cfg(0x2c);
        if ((try p.cfg(4)) & 2 == 0) return error.DecodeClosed;
        const bar = try p.cfg(0x10);
        const ap = try p.cfg(0x18);
        if (bar & 7 != 4 or ap & 7 != 4) return error.WrongBar;
        r.mmio = (@as(u64, try p.cfg(0x14)) << 32) | (bar & 0xfffffff0);
        r.aperture = (@as(u64, try p.cfg(0x1c)) << 32) | (ap & 0xfffffff0);
        r.step = 2;
        if (try bs.openProtocolInformation(handle, Pci)) |opens| {
            defer bs.freePool(@ptrCast(opens.ptr)) catch {};
            for (opens) |op| {
                const bits = @intFromEnum(op.attributes);
                if (bits & 16 != 0) {
                    r.driver_opens += 1;
                    if (op.agent_handle) |agent| {
                        if (driverFile(bs, agent)) |id| {
                            if (r.firmware_files == r.driver_file_ids.len) return error.TooManyDrivers;
                            r.driver_file_ids[r.firmware_files] = id;
                            r.firmware_files += 1;
                        }
                    }
                }
                if (bits & 8 != 0) r.child_opens += 1;
            }
        }
        const ids = try bs.protocolsPerHandle(handle);
        defer bs.freePool(@ptrCast(ids.ptr)) catch {};
        if (ids.len > r.protocol_ids.len) return error.TooManyProtocols;
        for (ids, 0..) |id, i| r.protocol_ids[i] = @bitCast(id.*);
        r.protocols = @intCast(ids.len);
        r.step = 3;
        try snapshot(p, r);
        r.step = 4;
        try framebuffer(bs, r);
        try snapshot(p, r);
        r.step = 5;
        if (r.framebuffer < r.aperture or r.framebuffer - r.aperture != r.surface or r.framebuffer_bytes == 0 or
            r.surface >= wire.AIR_BOOT_SLOT * 4096 or r.framebuffer_bytes > wire.AIR_BOOT_SLOT * 4096 - r.surface)
            return error.ForeignFramebuffer;
        r.step = 6;
        r.state = wire.AIR_BOOT_CAPTURED;
        return;
    }
}
const Publication = struct {
    acpi: *Acpi,
    key: usize,
    pages: []align(4096) efi.Page,
};
fn publish(bs: *Services) !Publication {
    const acpi = (try bs.locateProtocol(Acpi, null)) orelse return error.NoAcpi;
    var r = std.mem.zeroes(Record);
    r.signature = "OEMG".*;
    r.length = @sizeOf(Record);
    r.revision = 1;
    r.oem = "SRVROS".*;
    r.table_id = "BDWBOOT1".*;
    r.oem_revision = 1;
    r.first = wire.AIR_BOOT_SLOT;
    capture(bs, &r) catch {
        r.state = wire.AIR_BOOT_PROBING;
    };
    // Ownership of this firmware-described private buffer crosses ExitBootServices.
    // Its pages stay reserved for this boot even if the GPU declines admission.
    const pages = try bs.allocatePages(.{ .max_address = @ptrFromInt(0xfffff000) }, .acpi_memory_nvs, wire.AIR_BOOT_FRAMES);
    errdefer bs.freePages(pages) catch {};
    r.ram = @intFromPtr(pages.ptr);
    for (pages, 0..) |*page, i| {
        @memset(page, 0);
        std.mem.writeInt(u64, page[0..8], wire.AIR_BOOT_MARKER ^ r.ram ^ i, .little);
    }
    var sum: u8 = 0;
    for (std.mem.asBytes(&r)) |b| sum +%= b;
    r.checksum = 0 -% sum;
    var key: usize = 0;
    const installed = acpi.install(acpi, &r, @sizeOf(Record), &key);
    if (installed != .success) return error.InstallTable;
    return .{ .acpi = acpi, .key = key, .pages = pages };
}
pub fn main() Status {
    const bs = efi.system_table.boot_services orelse return .unsupported;
    const path = (bs.openProtocol(ImagePath, efi.handle, .{ .get_protocol = .{} }) catch return .not_found) orelse return .not_found;
    const payload = @embedFile("limine");
    var child: Handle = undefined;
    const loaded = bs._loadImage(false, efi.handle, @ptrCast(@alignCast(path)), payload.ptr, payload.len, &child);
    if (loaded != .success) return loaded;
    defer _ = bs._unloadImage(child);
    // Refusals still enter SrvrOS so it can save the reason and return.
    const table = publish(bs) catch null;
    const result = bs._startImage(child, null, null);
    if (table) |t| {
        if (t.acpi.uninstall(t.acpi, t.key) == .success) bs.freePages(t.pages) catch {};
    }
    return result;
}
