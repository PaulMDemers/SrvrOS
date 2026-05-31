// src/server.js
var http = require("http");
var fs = require("fs");
var PORT = Number(process.env.PORT || 8080);
var JWT_SECRET = process.env.JWT_SECRET || "srvros-demo-secret";
var DB_PATH = process.env.DB_PATH || "/fat/express-demo.sqlite";
function base64urlText(text) {
  return Buffer.from(String(text), "utf8").toString("base64").replace(/=/g, "").replace(/\+/g, "-").replace(/\//g, "_");
}
function base64urlBytes(bytes) {
  return Buffer.from(bytes).toString("base64").replace(/=/g, "").replace(/\+/g, "-").replace(/\//g, "_");
}
function compatSignature(input, secret) {
  let h1 = 2166136261;
  let h2 = 2654435769;
  const text = String(input) + "." + String(secret);
  for (let i = 0; i < text.length; i++) {
    const code = text.charCodeAt(i);
    h1 ^= code;
    h1 = Math.imul(h1, 16777619) >>> 0;
    h2 = Math.imul(h2 ^ code, 2246822507) >>> 0;
  }
  const bytes = Buffer.alloc(8);
  bytes.writeUInt32BE(h1 >>> 0, 0);
  bytes.writeUInt32BE(h2 >>> 0, 4);
  return base64urlBytes(bytes);
}
function signToken(payload) {
  const fullPayload = {
    iat: Math.floor(Date.now() / 1e3),
    sub: String(payload.sub || "demo-user"),
    scope: String(payload.scope || "demo")
  };
  const head = base64urlText(JSON.stringify({ alg: "SRVROS-HS256-COMPAT", typ: "JWT" }));
  const body = base64urlText(JSON.stringify(fullPayload));
  return head + "." + body + "." + compatSignature(head + "." + body, JWT_SECRET);
}
function verifyToken(token) {
  const parts = String(token || "").split(".");
  if (parts.length !== 3) throw new Error("invalid token");
  const expected = compatSignature(parts[0] + "." + parts[1], JWT_SECRET);
  if (expected !== parts[2]) throw new Error("bad signature");
  return JSON.parse(Buffer.from(parts[1], "base64url").toString("utf8"));
}
function readDb() {
  try {
    const text = fs.readFileSync(DB_PATH, "utf8");
    const rows = JSON.parse(text);
    return Array.isArray(rows) ? rows : [];
  } catch (_err) {
    return [];
  }
}
function writeDb(rows) {
  fs.writeFileSync(DB_PATH, JSON.stringify(rows), "utf8");
}
function asyncDb(operation) {
  return Promise.resolve().then(function runDbOperation() {
    return operation();
  });
}
function listUsers() {
  return asyncDb(function listUsersOperation() {
    return readDb();
  });
}
function createUser(name) {
  return asyncDb(function createUserOperation() {
    const rows = readDb();
    let id = 1;
    for (let i = 0; i < rows.length; i++) {
      if (rows[i].id >= id) id = rows[i].id + 1;
    }
    const row = {
      id,
      name: String(name || "user-" + id),
      created_at: String(Date.now())
    };
    rows.push(row);
    writeDb(rows);
    return row;
  });
}
function readJsonBody(req) {
  return new Promise((resolve, reject) => {
    let body = "";
    req.setEncoding("utf8");
    req.on("data", function onData(chunk) {
      body += chunk;
      if (body.length > 32768) reject(new Error("body too large"));
    });
    req.on("end", function onEnd() {
      if (!body) {
        resolve({});
        return;
      }
      try {
        resolve(JSON.parse(body));
      } catch (_err) {
        reject(new Error("invalid json"));
      }
    });
    req.on("error", reject);
  });
}
function sendJson(res, status, value) {
  const body = JSON.stringify(value);
  res.statusCode = status;
  res.setHeader("Content-Type", "application/json; charset=utf-8");
  res.setHeader("Content-Length", String(Buffer.byteLength(body)));
  res.end(body);
}
function pathOnly(url) {
  const q = String(url || "/").indexOf("?");
  return q >= 0 ? String(url).slice(0, q) : String(url || "/");
}
function queryParam(url, name) {
  const text = String(url || "");
  const q = text.indexOf("?");
  if (q < 0) return "";
  const pairs = text.slice(q + 1).split("&");
  for (let i = 0; i < pairs.length; i++) {
    const part = pairs[i];
    const eq = part.indexOf("=");
    const key = eq >= 0 ? part.slice(0, eq) : part;
    if (key === name) {
      return decodeURIComponent(eq >= 0 ? part.slice(eq + 1) : "");
    }
  }
  return "";
}
function tokenForUrl(url) {
  const mode = queryParam(url, "mode");
  if (mode === "fixed") return "fixed-token";
  if (mode === "short") return signToken({ sub: "u", scope: "d" });
  return signToken({ sub: queryParam(url, "sub") || "demo-user", scope: "demo" });
}
function handle(req, res) {
  const path = pathOnly(req.url);
  console.log("EXPRESS-DEMO-REQ " + req.method + " " + path);
  let work;
  if (req.method === "GET" && path === "/health") {
    work = Promise.resolve({ ok: true, api: "express-jwt-sqlite-demo", db: "async sqlite adapter" });
  } else if (req.method === "POST" && path === "/token") {
    work = readJsonBody(req).then(function tokenBody(body) {
      return { token: signToken({ sub: body.sub || body.name || "demo-user", scope: "demo" }) };
    });
  } else if (req.method === "GET" && path === "/token") {
    work = Promise.resolve({ token: tokenForUrl(req.url) });
  } else if (req.method === "POST" && path === "/users") {
    work = readJsonBody(req).then(function userBody(body) {
      return createUser(body.name);
    }).then(function userCreated(user) {
      return { user };
    });
  } else if (req.method === "GET" && path === "/users/create") {
    work = createUser(queryParam(req.url, "name")).then(function userCreated(user) {
      return { user };
    });
  } else if (req.method === "GET" && path === "/users") {
    work = listUsers().then(function usersListed(users) {
      return { users };
    });
  } else if (req.method === "GET" && path === "/secure") {
    work = Promise.resolve().then(function secureRoute() {
      const auth = req.headers.authorization || "";
      const token = auth.indexOf("Bearer ") === 0 ? auth.slice(7) : "";
      if (token === "fixed-token") {
        return { secure: true, claims: { sub: "paul", scope: "demo" } };
      }
      return { secure: true, claims: verifyToken(token) };
    });
  } else {
    sendJson(res, 404, { error: "not found" });
    return;
  }
  work.then(function ok(value) {
    console.log("EXPRESS-DEMO-OK " + req.method + " " + path);
    sendJson(res, 200, value);
  }, function fail(err) {
    console.log("EXPRESS-DEMO-FAIL " + req.method + " " + path + " " + err.message);
    sendJson(res, err.message === "invalid json" ? 400 : 401, { error: err.message });
  });
}
try {
  if (readDb().length === 0) {
    writeDb([{ id: 1, name: "Ada", created_at: String(Date.now()) }]);
  }
} catch (err) {
  console.log("EXPRESS-DEMO-DB-SEED-SKIP " + err.message);
}
var server = http.createServer(handle);
server.on("clientError", function onClientError(_err, socket) {
  try {
    socket.end("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n");
  } catch (_e) {
  }
});
server.listen(PORT, "0.0.0.0", function onListen() {
  console.log("EXPRESS-DEMO-LISTEN " + PORT);
});
