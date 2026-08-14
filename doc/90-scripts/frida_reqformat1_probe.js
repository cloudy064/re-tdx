"use strict";

/*
 * One-shot reqformat=1 probe for an already running 32-bit TdxW process.
 *
 * Usage:
 *   frida -n TdxW.exe -l frida_reqformat1_probe.js -q -t 12
 *
 * The script sends one read-only request through TPData's existing anonymous
 * session.  It does not modify TdxW files or account state.
 */

const moduleName = "TPData100.dll";
const modulePath = "C:\\new_tdx\\ZDPlugins\\TPData100.dll";
const installPath = "C:\\new_tdx\\";
const userPath = "C:\\new_tdx\\T0001\\";
const entryText = "tdxyj_jjzt_jjzs_sczs";
const rawBodyText = "";
const requestSerial = 0x51f10001;
let handle = NULL;
let initializedHere = false;
let responseSeen = false;

function bytesToAscii(buffer) {
  const values = new Uint8Array(buffer);
  let result = "";
  for (const value of values) {
    result += value >= 32 && value < 127 ? String.fromCharCode(value) : ".";
  }
  return result;
}

function bytesToHex(buffer) {
  const values = new Uint8Array(buffer);
  return Array.from(values, value => value.toString(16).padStart(2, "0"))
    .join("");
}

function dumpCandidate(label, pointer, length) {
  if (pointer.isNull() || length <= 0 || length > 16 * 1024 * 1024) {
    return;
  }
  const sampleLength = Math.min(length, 4096);
  let sample;
  try {
    sample = pointer.readByteArray(sampleLength);
  } catch (_error) {
    return;
  }
  const ascii = bytesToAscii(sample);
  if (
    ascii.indexOf("Params") === -1
    && ascii.indexOf("TQL") === -1
    && ascii.indexOf("tdxyj") === -1
    && ascii.indexOf("ResultSets") === -1
    && ascii.indexOf("ErrorCode") === -1
  ) {
    return;
  }
  console.log(JSON.stringify({
    event: label,
    length,
    ascii,
    hex: bytesToHex(sample),
  }));
}

function hookWinsock() {
  const sendAddress = Module.findGlobalExportByName("send");
  if (sendAddress !== null) {
    Interceptor.attach(sendAddress, {
      onEnter(args) {
        dumpCandidate("send", args[1], args[2].toInt32());
      },
    });
  }
  const wsaSendAddress = Module.findGlobalExportByName("WSASend");
  if (wsaSendAddress !== null) {
    Interceptor.attach(wsaSendAddress, {
      onEnter(args) {
        const buffers = args[1];
        const count = args[2].toInt32();
        for (let index = 0; index < Math.min(count, 16); index++) {
          const item = buffers.add(index * Process.pointerSize * 2);
          const length = item.readU32();
          const data = item.add(Process.pointerSize).readPointer();
          dumpCandidate("WSASend", data, length);
        }
      },
    });
  }
}

function main() {
  let module;
  try {
    module = Process.getModuleByName(moduleName);
  } catch (_error) {
    module = Module.load(modulePath);
    initializedHere = true;
  }
  console.log(JSON.stringify({
    event: "module",
    name: module.name,
    base: module.base.toString(),
    size: module.size,
  }));

  hookWinsock();

  const parseResponse = module.base.add(0xce30);
  Interceptor.attach(parseResponse, {
    onEnter(args) {
      let text = "";
      try {
        text = args[1].readUtf8String();
      } catch (error) {
        text = "<read failed: " + error + ">";
      }
      console.log(JSON.stringify({
        event: "rapidjson-response",
        text,
      }));
      responseSeen = true;
      // A NULL completion HWND makes TPData release the handle after parsing.
      handle = NULL;
    },
  });

  const createEx = new NativeFunction(
    module.getExportByName("CreateFetchDataHandleEx"),
    "pointer",
    ["pointer", "pointer", "int"],
    "mscdecl"
  );
  const configure = new NativeFunction(
    module.base.add(0x9170),
    "int",
    ["pointer", "int", "pointer", "pointer", "int"],
    "thiscall"
  );
  const start = new NativeFunction(
    module.base.add(0x2d80),
    "bool",
    ["pointer"],
    "thiscall"
  );
  const destroy = new NativeFunction(
    module.getExportByName("DeleteFetchDataHandle"),
    "void",
    ["pointer"],
    "mscdecl"
  );

  const setEnvironment = new NativeFunction(
    module.getExportByName("ITPConn_SetEnvironPath"),
    "int",
    ["pointer", "pointer", "int"],
    "mscdecl"
  );
  const initialize = new NativeFunction(
    module.getExportByName("ITPConn_Init"),
    "int",
    [],
    "mscdecl"
  );
  const loginAnonymous = new NativeFunction(
    module.getExportByName("ITPConn_LoginAnony"),
    "int",
    ["int", "pointer", "int"],
    "mscdecl"
  );
  const isLoggedIn = new NativeFunction(
    module.getExportByName("ITPConn_IsLogined"),
    "int",
    ["int"],
    "mscdecl"
  );
  const getSession = new NativeFunction(
    module.getExportByName("ITPConn_GetSession"),
    "pointer",
    ["int"],
    "mscdecl"
  );
  const setActiveSession = new NativeFunction(
    module.getExportByName("ITPConn_SetActiveSession"),
    "void",
    ["pointer"],
    "mscdecl"
  );
  const setUser = new NativeFunction(
    module.getExportByName("ITPConn_SetUser"),
    "void",
    ["pointer", "int", "int"],
    "mscdecl"
  );
  const uninitialize = new NativeFunction(
    module.getExportByName("ITPConn_UnInit"),
    "void",
    [],
    "mscdecl"
  );

  const hostUserInfo = Memory.alloc(96);
  hostUserInfo.writeByteArray(new Uint8Array(96));
  const getHostData = new NativeFunction(
    Process.mainModule.base.add(0x21b630),
    "int",
    [
      "pointer", "int", "int", "pointer", "int", "int",
      "int", "int", "int", "int", "int",
    ],
    "stdcall"
  );
  const configuredInstallPath = Process.mainModule.base
    .add(0xdd4340).readPointer().readUtf8String();
  const configuredUserPath = Process.mainModule.base
    .add(0xdd41e4).readPointer().readUtf8String();
  const userResult = getHostData(
    NULL, -1, 122, hostUserInfo, 1, 0, 0, 0, 0, 0, 0
  );
  const ssoBuffer = Memory.alloc(256);
  ssoBuffer.writeByteArray(new Uint8Array(256));
  const ssoResult = getHostData(
    NULL, -1, 133, ssoBuffer, 256, 0, 0, 0, 0, 9, 0
  );
  const userName = hostUserInfo.add(20);
  const hasUser = userName.readU8() !== 0;
  console.log(JSON.stringify({
    event: "user-context",
    result: userResult,
    hasUser,
    market: hostUserInfo.add(82).readU16(),
    userFlags: hostUserInfo.add(72).readS32(),
    ssoResult,
    hasSso: ssoBuffer.readU8() !== 0,
    configuredInstallPath,
    configuredUserPath,
  }));

  function initializeOnUiThread(hostWindow) {
    const existingLoginState = isLoggedIn(100);
    if (existingLoginState) {
      uninitialize();
    }
    const environmentResult = setEnvironment(
      Memory.allocUtf8String(installPath),
      Memory.allocUtf8String(userPath),
      0
    );
    const initializeResult = initialize();
    setUser(
      userName,
      hostUserInfo.add(82).readU16(),
      hostUserInfo.add(72).readS32()
    );
    const loginResult = loginAnonymous(100, hostWindow, 1074);
    const session = getSession(100);
    if (!session.isNull()) {
      setActiveSession(session);
    }
    initializedHere = true;
    console.log(JSON.stringify({
      event: "initialize",
      existingLoginState,
      environmentResult,
      initializeResult,
      loginResult,
    }));
  }

  const user32 = Process.getModuleByName("user32.dll");
  const getWindowThreadProcessId = new NativeFunction(
    user32.getExportByName("GetWindowThreadProcessId"),
    "uint32",
    ["pointer", "pointer"],
    "stdcall"
  );
  const isWindowVisible = new NativeFunction(
    user32.getExportByName("IsWindowVisible"),
    "bool",
    ["pointer"],
    "stdcall"
  );
  const enumWindows = new NativeFunction(
    user32.getExportByName("EnumWindows"),
    "bool",
    ["pointer", "pointer"],
    "stdcall"
  );
  let hostWindow = NULL;
  let hostUiThread = 0;
  const processId = Memory.alloc(4);
  const enumCallback = new NativeCallback((window, _context) => {
    processId.writeU32(0);
    const threadId = getWindowThreadProcessId(window, processId);
    if (processId.readU32() === Process.id && isWindowVisible(window)) {
      hostWindow = window;
      hostUiThread = threadId;
      return 0;
    }
    return 1;
  }, "bool", ["pointer", "pointer"], "stdcall");
  enumWindows(enumCallback, NULL);
  if (hostWindow.isNull() || hostUiThread === 0) {
    throw new Error("TdxW top-level window was not found");
  }
  console.log(JSON.stringify({
    event: "ui-thread",
    window: hostWindow.toString(),
    threadId: hostUiThread,
  }));

  let initializationScheduled = false;
  const dispatchMessage = Module.findGlobalExportByName("DispatchMessageA");
  if (dispatchMessage === null) {
    throw new Error("DispatchMessageA was not found");
  }
  Interceptor.attach(dispatchMessage, {
    onEnter(_args) {
      if (
        initializationScheduled
        || Process.getCurrentThreadId() !== hostUiThread
      ) {
        return;
      }
      initializationScheduled = true;
      initializeOnUiThread(hostWindow);
    },
  });

  let attempts = 0;
  const timer = setInterval(() => {
    attempts += 1;
    if (!initializationScheduled) {
      console.log(JSON.stringify({
        event: "login-state",
        attempts,
        loggedIn: 0,
        waitingForUiThread: true,
      }));
      return;
    }
    const loggedIn = isLoggedIn(100);
    console.log(JSON.stringify({event: "login-state", attempts, loggedIn}));
    if (!loggedIn && attempts < 20) {
      return;
    }
    clearInterval(timer);
    if (!loggedIn) {
      throw new Error("anonymous TPData session did not log in");
    }
    const entry = Memory.allocUtf8String(entryText);
    const body = Memory.allocUtf8String(rawBodyText);
    const session = getSession(100);
    handle = createEx(session, NULL, 1124);
    if (handle.isNull()) {
      throw new Error("CreateFetchDataHandle returned NULL");
    }
    console.log(JSON.stringify({
      event: "handle",
      address: handle.toString(),
    }));
    const configured = configure(
      handle,
      1,
      entry,
      body,
      requestSerial
    );
    console.log(JSON.stringify({event: "configured", result: configured}));
    const started = start(handle);
    console.log(JSON.stringify({event: "started", result: started}));
  }, 500);

  setTimeout(() => {
    if (!handle.isNull()) {
      destroy(handle);
      handle = NULL;
      console.log(JSON.stringify({event: "destroyed"}));
    }
    console.log(JSON.stringify({event: "probe-finished", responseSeen}));
  }, 14000);
}

setImmediate(main);
