// GRUB starts this probe, then directly chainloads Limine from the same ESP.
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
const Handles = struct {
    items: [16]?Handle = .{null} ** 16,
    count: usize = 0,

    fn add(self: *Handles, handle: ?Handle) void {
        if (self.count < self.items.len) self.items[self.count] = handle;
        self.count += 1;
    }
};
pub fn framebuffer(bs: *Services, r: *Record) !Handles {
    var active = Handles{};
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
        active.add(handle);
        r.framebuffer = base;
        r.framebuffer_bytes = size;
    }
    if (active.count == 0) return error.NoGop;
    return active;
}
// Query the controller's protocol-open database. Never call the private
// interface or disconnect a driver from the active display.
pub const Eg2Device = extern struct {
    unused: u8,
    pub const guid align(8) = makeGuid(0x53cfeb0d, 0x9b9e, 0x4d0a, .{ 0x90, 0xb6, 0x02, 0x4a, 0x79, 0x90, 0x6a, 0x58 });
};
const core_eg2 = makeGuid(0x4ddeafa4, 0xf480, 0x4b9d, .{ 0x88, 0x9b, 0x1e, 0x3a, 0xe6, 0x60, 0x5b, 0xd6 });
// Bit N refers to the Nth BY_CHILD_CONTROLLER record in this boot only.
const Panel = struct {
    gops: u16 = 0,
    matches: u16 = 0,
    path_hash: u64 = 0,
    adr: ?u32 = null,

    fn identify(self: *Panel, bs: *Services, child: Handle) !void {
        var node = (try bs.openProtocol(efi.protocol.DevicePath, child, .{ .get_protocol = .{} })) orelse return;
        var remaining: usize = 4096;
        var hash = std.hash.Fnv1a_64.init();
        while (remaining >= 4) {
            const length = node.length;
            if (length < 4 or length > remaining) break;
            const raw: [*]const u8 = @ptrCast(node);
            hash.update(raw[0..length]);
            if (node.type == .acpi and node.subtype == 3 and length >= 8) {
                self.adr = std.mem.readInt(u32, raw[4..8], .little);
            }
            if (node.type == .end and node.subtype == 0xff and length == 4) {
                self.path_hash = hash.final();
                return;
            }
            remaining -= length;
            node = @ptrFromInt(@intFromPtr(node) + length);
        }
        return error.MalformedChildPath;
    }

    fn inspect(self: *Panel, bs: *Services, child: Handle, index: usize, base: u64, size: u64) !void {
        if (index >= 16) return error.TooManyChildren;
        const gop = (try bs.openProtocol(efi.protocol.GraphicsOutput, child, .{ .get_protocol = .{} })) orelse return;
        const bit = @as(u16, 1) << @intCast(index);
        self.gops |= bit;
        if (size == 0 or gop.mode.info.pixel_format == .blt_only or
            gop.mode.frame_buffer_base != base or gop.mode.frame_buffer_size != size) return;
        self.matches |= bit;
        if (self.matches == bit) {
            try self.identify(bs, child);
        } else {
            self.path_hash = 0;
            self.adr = null;
        }
    }
};

// Append the paths themselves. The collector can compare complete UEFI nodes,
// including distinct instances of the firmware ConOut variable.
const path_limit = 4096;
const route_header = 10;
var route_bytes: [route_header + 34 * (4 + path_limit)]u8 = undefined;

fn devicePath(bs: *Services, handle: Handle) !?[]const u8 {
    const path = (try bs.openProtocol(efi.protocol.DevicePath, handle, .{ .get_protocol = .{} })) orelse return null;
    const start: [*]const u8 = @ptrCast(path);
    var pos: usize = 0;
    while (pos + 4 <= path_limit) {
        const node = start + pos;
        const len = std.mem.readInt(u16, node[2..4], .little);
        if (len < 4 or len > path_limit - pos) break;
        pos += len;
        if (node[0] == 0x7f) {
            if (len != 4) break;
            if (node[1] == 0xff) return start[0..pos];
            if (node[1] != 1) break;
        }
    }
    return error.MalformedDevicePath;
}

const RouteWriter = struct {
    pos: usize = route_header,
    // These identifiers compare handle identity only within this receipt.
    handles: [33]Handle = undefined,
    used: usize = 0,
    rows: [34]struct { id: u8, state: u8, bytes: []const u8 } = undefined,
    count: usize = 0,

    fn row(self: *RouteWriter, id: u8, state: u8, path: []const u8) void {
        const dst = route_bytes[self.pos..];
        dst[0] = id;
        dst[1] = state;
        std.mem.writeInt(u16, dst[2..4], @intCast(path.len), .little);
        @memcpy(dst[4..][0..path.len], path);
        self.rows[self.count] = .{ .id = id, .state = state, .bytes = dst[4..][0..path.len] };
        self.count += 1;
        self.pos += 4 + path.len;
    }

    fn handlePath(self: *RouteWriter, bs: *Services, handle: ?Handle) void {
        const h = handle orelse return self.row(0, 1, "");
        const i = std.mem.indexOfScalar(Handle, self.handles[0..self.used], h) orelse index: {
            self.handles[self.used] = h;
            self.used += 1;
            break :index self.used - 1;
        };
        const id: u8 = @intCast(i + 1);
        const path = devicePath(bs, h) catch return self.row(id, 2, "");
        if (path) |bytes| self.row(id, 0, bytes) else self.row(id, 1, "");
    }
};

// ConOut can contain several instances. Compare at node boundaries and
// consume the entire variable before accepting even the first match.
fn consoleMatches(console: []const u8, active: []const u8) bool {
    const end = [_]u8{ 0x7f, 0xff, 4, 0 };
    if (active.len <= 4 or !std.mem.endsWith(u8, active, &end)) return false;
    var begin: usize = 0;
    var pos: usize = 0;
    var matches: usize = 0;
    while (pos + 4 <= console.len) {
        const bytes = console[pos..];
        const n = std.mem.readInt(u16, bytes[2..4], .little);
        if (n < 4 or n > bytes.len) return false;
        if (bytes[0] == 0x7f) {
            if (n != 4 or pos == begin) return false;
            if (std.mem.eql(u8, console[begin..pos], active[0 .. active.len - 4])) matches += 1;
            if (bytes[1] == 0xff) return matches == 1 and pos + n == console.len;
            if (bytes[1] != 1) return false;
            begin = pos + n;
        }
        pos += n;
    }
    return false;
}

fn singlePath(bytes: []const u8) bool {
    var pos: usize = 0;
    while (pos + 4 <= bytes.len) {
        const node = bytes[pos..];
        const len = std.mem.readInt(u16, node[2..4], .little);
        if (len < 4 or len > node.len) return false;
        if (node[0] == 0x7f) return pos != 0 and node[1] == 0xff and len == 4 and len == node.len;
        pos += len;
    }
    return false;
}

fn selectBranch(w: *const RouteWriter, gops: usize, children: usize, crc: u32) u64 {
    const console = w.rows[0];
    if (console.state != 0) return 0;
    var selected: usize = 0;
    for (w.rows[2..][0..gops], 1..) |g, i| {
        // Console splitters can supply the same framebuffer without a path.
        if (g.state == 1 and g.id == w.rows[1].id) continue;
        if (g.state != 0 or !singlePath(g.bytes)) return 0;
        if (consoleMatches(console.bytes, g.bytes)) {
            if (selected != 0) return 0;
            selected = i;
        }
    }
    if (selected == 0) return 0;
    const active = w.rows[1 + selected].bytes;
    var child: usize = 0;
    for (w.rows[2 + gops ..][0..children], 1..) |c, i| {
        if (c.state != 0 or !singlePath(c.bytes)) return 0;
        const prefix = c.bytes[0 .. c.bytes.len - 4];
        if (std.mem.startsWith(u8, active[0 .. active.len - 4], prefix)) {
            if (child != 0) return 0;
            child = i;
        }
    }
    return if (child == 0) 0 else (@as(u64, crc) << 32) | (selected << 8) | child;
}

test "select by console instance and unique child path in this enumeration" {
    const pci = [_]u8{ 1, 1, 6, 0, 0, 2 };
    const vendor = [_]u8{ 3, 10, 20, 0 } ++ ([_]u8{0} ** 16);
    const other = [_]u8{ 1, 1, 6, 0, 0, 3 };
    const end = [_]u8{ 0x7f, 0xff, 4, 0 };
    const split = [_]u8{ 0x7f, 1, 4, 0 };
    const active = pci ++ vendor ++ end;
    var w = RouteWriter{};
    w.row(0, 0, &(other ++ split ++ active));
    w.row(1, 1, "");
    w.row(1, 1, "");
    w.row(2, 0, &active);
    w.row(3, 0, &(other ++ end));
    w.row(4, 0, &(pci ++ end));
    try std.testing.expectEqual(@as(u64, 0x5500000202), selectBranch(&w, 2, 2, 0x55));
    std.mem.swap(@TypeOf(w.rows[4]), &w.rows[4], &w.rows[5]);
    try std.testing.expectEqual(@as(u64, 0x5500000201), selectBranch(&w, 2, 2, 0x55));
    w.rows[5] = w.rows[4];
    try std.testing.expectEqual(@as(u64, 0), selectBranch(&w, 2, 2, 0x55));
    w.rows[5].state = 1;
    try std.testing.expectEqual(@as(u64, 0), selectBranch(&w, 2, 2, 0x55));
    w.rows[5].state = 0;
    w.rows[5].bytes = &(other ++ end);
    w.rows[2] = w.rows[3];
    try std.testing.expectEqual(@as(u64, 0), selectBranch(&w, 2, 2, 0x55));
}

fn routes(bs: *Services, active: Handles, children: Handles, branch: *u64) ?[]const u8 {
    if (active.count == 0 or active.count > active.items.len or children.count > children.items.len) return null;
    var writer = RouteWriter{};
    @memcpy(route_bytes[0..4], "GOP1");
    route_bytes[8] = @intCast(active.count);
    route_bytes[9] = @intCast(children.count);
    var console: [path_limit]u8 = undefined;
    const result = efi.system_table.runtime_services.getVariable(std.unicode.utf8ToUtf16LeStringLiteral("ConOut"), &efi.tables.global_variable, &console);
    if (result) |found| {
        if (found) |value| writer.row(0, 0, value[0]) else writer.row(0, 1, "");
    } else |_| writer.row(0, 2, "");
    writer.handlePath(bs, efi.system_table.console_out_handle);
    for (active.items[0..active.count]) |h| writer.handlePath(bs, h);
    for (children.items[0..children.count]) |h| writer.handlePath(bs, h);
    const crc = std.hash.Crc32.hash(route_bytes[8..writer.pos]);
    std.mem.writeInt(u32, route_bytes[4..8], crc, .little);
    branch.* = selectBranch(&writer, active.count, children.count, crc);
    return route_bytes[0..writer.pos];
}

pub const Eg2Clients = struct {
    state: enum { observed, absent, unavailable } = .observed,
    entries: usize = 0,
    drivers: usize = 0,
    core_drivers: usize = 0,
    children: usize = 0,
    exclusive: usize = 0,
    panel: ?Panel = .{},
    paths: ?[]const u8 = null,
    branch: u64 = 0,
    selected_gop: ?Handle = null,
    selected_child: ?Handle = null,
};
pub fn eg2Clients(bs: *Services, handle: Handle, base: u64, size: u64, active: ?Handles) !Eg2Clients {
    var children = Handles{};
    const opens = (try bs.openProtocolInformation(handle, Eg2Device)) orelse return error.ProtocolUnavailable;
    defer if (opens.len != 0) bs.freePool(@ptrCast(opens.ptr)) catch {};
    if (opens.len > 0xffff) return error.TooManyClients;
    var result: Eg2Clients = .{ .entries = opens.len };
    for (opens) |op| {
        const bits = @intFromEnum(op.attributes);
        if (bits & 16 != 0) {
            result.drivers += 1;
            if (op.agent_handle) |agent| {
                if (driverFile(bs, agent)) |id| {
                    if (core_eg2.eql(@bitCast(id))) result.core_drivers += 1;
                }
            }
        }
        if (bits & 8 != 0) {
            if (result.panel) |*panel| {
                if (op.controller_handle) |child| {
                    panel.inspect(bs, child, result.children, base, size) catch {
                        result.panel = null;
                    };
                } else result.panel = null;
            }
            children.add(op.controller_handle);
            result.children += 1;
        }
        if (bits & 32 != 0) result.exclusive += 1;
    }
    if (active) |gops| {
        result.paths = routes(bs, gops, children, &result.branch);
        if (result.branch != 0) {
            result.selected_gop = gops.items[((result.branch >> 8) & 255) - 1];
            result.selected_child = children.items[(result.branch & 255) - 1];
        }
    }
    return result;
}
pub fn confirmBranch(bs: *Services, controller: Handle, r: *Record, first: Eg2Clients) !Eg2Clients {
    if (first.branch == 0) return first;
    const original = first.paths orelse return error.NoOriginalRoutes;
    const saved = try bs.allocatePool(.loader_data, original.len);
    defer bs.freePool(saved.ptr) catch {};
    @memcpy(saved, original);
    const active = try framebuffer(bs, r);
    const checked = try eg2Clients(bs, controller, r.framebuffer, r.framebuffer_bytes, active);
    if (checked.branch != first.branch or checked.selected_gop != first.selected_gop or
        checked.selected_child != first.selected_child or
        !std.mem.eql(u8, saved, checked.paths orelse return error.NoClosingRoutes)) return error.DisplayBranchChanged;
    return checked;
}
fn capture(bs: *Services, r: *Record, clients: *?Eg2Clients) !void {
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
        const active = try framebuffer(bs, r);
        clients.* = .{ .state = .absent };
        for (ids) |id| {
            if (Eg2Device.guid.eql(id.*)) clients.* = eg2Clients(bs, handle, r.framebuffer, r.framebuffer_bytes, active) catch .{ .state = .unavailable };
        }
        if (clients.*) |first| clients.* = try confirmBranch(bs, handle, r, first);
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
const File = efi.protocol.File;
fn openTrace(bs: *Services) !*File {
    const image = (try bs.openProtocol(efi.protocol.LoadedImage, efi.handle, .{ .get_protocol = .{} })) orelse return error.NoImage;
    const fs = (try bs.openProtocol(efi.protocol.SimpleFileSystem, image.device_handle orelse return error.NoDevice, .{ .get_protocol = .{} })) orelse return error.NoFs;
    const root = try fs.openVolume();
    defer root.close() catch {};
    const path = std.unicode.utf8ToUtf16LeStringLiteral("\\EFI\\srvros\\AIRBOOT.LOG");
    const file = try root.open(path, .read_write, .{});
    errdefer file.close() catch {};
    var data: [513]u8 = undefined;
    const prefix = "SRVR-AIR-EFI-V1 ";
    if (try file.read(&data) != 512 or !std.mem.startsWith(u8, &data, prefix)) return error.InvalidSeed;
    const id = data[prefix.len..][0..34];
    if (id[16] != ' ' or id[33] != '\n') return error.InvalidSeed;
    for (id, 0..) |c, i| {
        if (i != 16 and i != 33 and !(std.ascii.isDigit(c) or (c >= 'a' and c <= 'f'))) return error.InvalidSeed;
    }
    if (!std.mem.allEqual(u8, data[prefix.len + 34 .. 512], 0)) return error.UsedSeed;
    return file;
}
const Phase = enum(u8) { entered = 1, capture, locate, allocate, install };
fn mark(file: ?*File, phase: Phase, value: u64) void {
    const f = file orelse return;
    var block = [_]u8{0} ** 64;
    _ = std.fmt.bufPrint(&block, "{s} {x:0>16}\n", .{ @tagName(phase), value }) catch return;
    f.setPosition(@as(u64, @intFromEnum(phase)) * 64) catch return;
    if ((f.write(&block) catch return) == block.len) f.flush() catch {};
}
const config_guid: efi.Guid align(8) = @bitCast([2]u64{ wire.AIR_BOOT_GUID_LO, wire.AIR_BOOT_GUID_HI });

fn markClients(f: *File, c: Eg2Clients, captured: bool) void {
    var block = [_]u8{0} ** 64;
    _ = switch (c.state) {
        .observed => std.fmt.bufPrint(&block, "eg2-open {d} {d} {d} {d} {d}\n", .{
            c.entries, c.drivers, c.core_drivers, c.children, c.exclusive,
        }),
        else => std.fmt.bufPrint(&block, "eg2-{s}\n", .{@tagName(c.state)}),
    } catch return;
    f.setPosition(384) catch return;
    if ((f.write(&block) catch return) != block.len) return;
    f.flush() catch return;
    if (c.state != .observed or !captured) return;
    @memset(&block, 0);
    if (c.panel) |p| {
        _ = std.fmt.bufPrint(&block, "panel {x:0>4} {x:0>4} {x:0>16} {d} {x:0>8}\n", .{ p.gops, p.matches, p.path_hash, @intFromBool(p.adr != null), p.adr orelse 0 }) catch return;
    } else {
        @memcpy(block[0.."panel-unavailable\n".len], "panel-unavailable\n");
    }
    f.setPosition(448) catch return;
    if ((f.write(&block) catch return) != block.len) return;
    if (c.paths) |data| {
        if ((f.write(data) catch return) != data.len) return;
    }
    f.flush() catch {};
}

fn publish(bs: *Services, file: ?*File, comptime captureRecord: anytype) Status {
    var r = std.mem.zeroes(Record);
    r.signature = "OEMG".*;
    r.length = @sizeOf(Record);
    r.revision = 1;
    r.oem = "SRVROS".*;
    r.table_id = "BDWBOOT1".*;
    r.oem_revision = 2;
    r.first = wire.AIR_BOOT_SLOT;
    var clients: ?Eg2Clients = null;
    captureRecord(bs, &r, &clients) catch {
        r.state = wire.AIR_BOOT_PROBING;
    };
    if (clients) |c| {
        if (r.state == wire.AIR_BOOT_CAPTURED) r.display_branch = c.branch;
    }
    // Ownership of this firmware-described private buffer crosses ExitBootServices.
    // Its pages stay reserved for this boot even if the GPU declines admission.
    mark(file, .capture, (@as(u64, r.state) << 32) | r.step);
    if (clients) |c| if (file) |f| markClients(f, c, r.state == wire.AIR_BOOT_CAPTURED);
    var interface: ?*const anyopaque = null;
    const locate = bs._locateProtocol(&Acpi.guid, null, &interface);
    mark(file, .locate, @intFromEnum(locate));
    if (locate != .success and locate != .not_found) return locate;
    const use_config = locate == .not_found;
    const st = efi.system_table;
    if (st.number_of_table_entries > 1024) return .out_of_resources;
    for (st.configuration_table[0..st.number_of_table_entries]) |table| {
        if (config_guid.eql(table.vendor_guid)) return .already_started;
    }
    // The fallback record needs its own persistent page. InstallConfigurationTable
    // retains the pointer, whereas the ACPI protocol copies its input.
    const count: usize = wire.AIR_BOOT_FRAMES + @as(usize, @intFromBool(use_config));
    var address: [*]align(4096) efi.Page = @ptrFromInt(0xfffff000);
    const alloc = bs._allocatePages(.max_address, .acpi_memory_nvs, count, &address);
    mark(file, .allocate, @intFromEnum(alloc));
    if (alloc != .success) return alloc;
    const pages = address[0..wire.AIR_BOOT_FRAMES];
    r.ram = @intFromPtr(pages.ptr);
    for (pages, 0..) |*page, i| {
        @memset(page, 0);
        std.mem.writeInt(u64, page[0..8], wire.AIR_BOOT_MARKER ^ r.ram ^ i, .little);
    }
    var sum: u8 = 0;
    for (std.mem.asBytes(&r)) |b| sum +%= b;
    r.checksum = 0 -% sum;
    const installed = if (use_config) blk: {
        const metadata = &address[wire.AIR_BOOT_FRAMES];
        @memset(metadata, 0);
        metadata[0..@sizeOf(Record)].* = std.mem.asBytes(&r).*;
        break :blk bs._installConfigurationTable(&config_guid, metadata);
    } else blk: {
        const acpi: *Acpi = @ptrCast(@alignCast(@constCast(interface orelse {
            bs.freePages(address[0..count]) catch {};
            return .device_error;
        })));
        var key: usize = 0;
        break :blk acpi.install(acpi, &r, @sizeOf(Record), &key);
    };
    mark(file, .install, @intFromEnum(installed));
    if (installed != .success) bs.freePages(address[0..count]) catch {};
    return installed;
}
pub fn run(comptime captureRecord: anytype) Status {
    const bs = efi.system_table.boot_services orelse return .unsupported;
    const trace = openTrace(bs) catch null;
    defer if (trace) |f| f.close() catch {};
    mark(trace, .entered, 0);
    return publish(bs, trace, captureRecord);
}
pub fn main() Status {
    return run(capture);
}
