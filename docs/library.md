Standard API
============

The Standard API is enabled only when a build option is specified. It
is implemented in a modular library called `libnoctapi`, separated
with the core `libnoct` library. The separation allows you to include
only the core components or the entire API in your binary through
build options.

The `noct` CLI command includes the standard APIs.

---

## File

`File.*` is a standard API and not included in the intrinsics.

### File.open(path, mode)

Opens a file.

```
var file = File.open("test.bin", "r");
var data = File.read(file, 100);
for (i in 0..100)
    print(data[i]);
File.close(file);
```

### File.close(file)

Closes a file.

```
var file = File.open("test.bin", "r");
var data = File.read(file, 100);
File.close(file);
```

### File.tell(file)

Gets the position in a file.

```
var file = File.open("test.bin", "r");
var offset = File.tell(file);
```

### File.seek(file, offset)

Moves to an offset in a file.

```
var file = File.open("test.bin", "r");
File.seek(file, 100);
```

### File.read(file, len)

Reads bytes and returns a UInt8 packed array.

```
var file = File.open("test.bin", "r");
var data = File.read(file, 100);
for (i in 0..100)
    print(data[i]);
File.close(file);
```

### File.write(file, data, offset, len)

Write bytes.

```
var data = Packed.uint8(100);
for (i in 0..100)
    data[i] = i;

var file = File.open("test.bin", "w");
File.write(file, data, 0, Packed.size(data));
File.close(file);
```

---

## FileUtil

`FileUtil.*` is a standard API and not included in the intrinsics.

### FileUtil.checkFileExists()

Checks whether a file exists.

```
if (File.checkFileExists("text.txt"))
    print("File exists!")
```

### FileUtil.readText(file)

Reads a text file as a string.

```
var text = File.readText("text.txt");
print(text);
```

### FileUtil.writeText(file, text)

Writes a string to a text file.

```
var text = "abc";
File.writeText("text.txt", text);
```

### FileUtil.readForEachLine(file, func)

Reads lines from a text file.

```
File.readForEachLine("text.txt", (line) => {
    print(line);
});
```

### FileUtil.writeForEachLine(file, lines)

Write lines to a text file.

```
File.writeForEachLine("text.txt", [
    "aaa",
    "bbb",
    "ccc"
]);
```

---

## System

`System.*` is a standard API and not included in the intrinsics.

### System.import()

Imports a script file or a bytecode file.

```
System.import("script.noct");
```

### System.shell()

Runs a shell command.

```
System.shell("ls -lha");
```

### System.runCommand(command, workDir, waitForFinish)

### System.getOSName()

---

## Console

`Console.*` is a standard API and not included in the intrinsics.

### Console.print()

Prints a text to the console.

```
Console.print("Hello, world!");
```

---

## Thread

`Thread.*` is a standard API, available only in a build configured
with `NOCT_ENABLE_MULTITHREAD=ON`.

Every blocking call below parks the calling thread for the garbage
collector, so a thread waiting on a lock, a join or a sleep never
stalls a collection running in another thread.

### Thread.createThread(func, param)

Starts a thread that runs `func(param)` and returns a thread handle.

```
func worker(param) {
    return param.a + param.b;
}

var th = Thread.createThread(worker, {a: 1, b: 2});
```

### Thread.joinThread(th)

Waits for a thread to finish and returns the value its function
returned. If the thread failed, the error is re-raised here.

```
print(Thread.joinThread(th));
```

### Thread.createShared(value)

Creates a shared value holder. The initial value is deep-copied, so
the holder never aliases the caller's data.

```
var shared = Thread.createShared({msg: ""});
```

### Thread.updateShared(shared, value)

Replaces the shared value with a deep copy of `value`, under a lock.

### Thread.snapshotShared(shared)

Returns a deep copy of the shared value, taken under the same lock.
A reader therefore never observes a half-written update.

```
func producer(shared) {
    for (i in 0..1000) {
        Thread.updateShared(shared, {a: i, b: i * 2});
    }
}

var snap = Thread.snapshotShared(shared);
```

### Thread.createCounter()

Creates an atomic counter starting at zero.

### Thread.incrementCounter(counter)

Increments the counter atomically and returns the new value.

### Thread.getCounter(counter)

Returns the current value.

```
var c = Thread.createCounter();
Thread.incrementCounter(c);
print(Thread.getCounter(c));
```

### Thread.createLocked(dict)

Creates a lock that guards the given dictionary. Unlike a shared
value, the dictionary is stored as-is and must only be touched inside
`Thread.withLock()`.

### Thread.withLock(locked, func)

Calls `func(dict)` with the lock held and returns its result.

```
var storage = Thread.createLocked({n: 0});
Thread.withLock(storage, (o) => { o.n = o.n + 1; });
```

### Thread.sleep(ms)

Sleeps for the given number of milliseconds.

---

## HttpServer

`HttpServer.*` is a standard API providing the sockets and the
readiness polling needed to write a server in Noct.

Sockets and pollers are handles: ordinary dictionaries carrying a
native control block. A script may add its own keys to a handle, which
is a convenient way to tag a listener or attach per-connection state.

A connection handle carries no thread affinity, so the thread that
accepts a connection and the thread that serves it may differ.

### HttpServer.listen(host, port)

Binds and listens, returning a listener handle. An empty host binds to
every interface.

```
var server = HttpServer.listen("127.0.0.1", 8080);
```

### HttpServer.connect(host, port)

Connects to a peer and returns a connection handle.

### HttpServer.accept(server)

Accepts one connection and returns its handle. On a non-blocking
listener with nothing pending, returns `0`.

### HttpServer.recv(conn, maxBytes)

Reads up to `maxBytes` and returns the data as a string. An empty
result means the peer closed the connection.

### HttpServer.send(conn, data)

Writes the whole string and returns the number of bytes written.

### HttpServer.close(sock)

Closes a listener or a connection. A socket that is still registered
with a poller must be removed first.

### HttpServer.isClosed(sock)

Returns 1 if the socket has been closed.

### HttpServer.setBlocking(sock, blocking)

Switches the socket between blocking and non-blocking mode.

### HttpServer.getPeer(conn)

Returns `{host: ..., port: ...}` for the remote end.

### HttpServer.createPoller()

Creates a poller.

A poller holds its registrations rather than taking a fresh set on
every wait, so adding and removing a socket is what costs, not
waiting. Readiness is level-triggered.

### HttpServer.addToPoller(poller, sock, events)

Registers a socket. `events` combines `HttpServer.READ` and
`HttpServer.WRITE`. A socket belongs to at most one poller.

### HttpServer.modifyPoller(poller, sock, events)

Changes which events a registered socket is watched for.

### HttpServer.removeFromPoller(poller, sock)

Unregisters a socket.

### HttpServer.waitPoller(poller, timeout)

Waits up to `timeout` milliseconds and returns an array of
`{socket: ..., events: ...}` for the sockets that are ready. With
nothing registered it returns immediately, so an event loop stays
responsive to its own shutdown checks.

```
var poller = HttpServer.createPoller();
HttpServer.addToPoller(poller, server, HttpServer.READ);
var ready = HttpServer.waitPoller(poller, 1000);
for (r in ready) {
    var conn = HttpServer.accept(r.socket);
    HttpServer.send(conn, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nhi");
    HttpServer.close(conn);
}
```

### HttpServer.countPoller(poller)

Returns the number of registered sockets.
