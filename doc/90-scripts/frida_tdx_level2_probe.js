"use strict";

/*
 * Passive Level2 probe for the analyzed 32-bit TdxW build.
 *
 * It only observes existing calls.  It does not create a session, send a
 * request, change arguments/return values, or read authentication material.
 * The companion capture_tdx_level2.py launcher supplies TDX_L2_CONFIG and
 * performs a SHA-256 preflight before attaching.
 */

const defaults = {
  allowUnknownVersion: false,
  label: "",
  targetCode: "",
  maxEvents: 500,
  maxRecordsPerEvent: 20,
  rawSampleBytes: 64,
  correlationWindowMs: 300000,
  snapshotEventBusRegistry: false,
};
const supplied = globalThis.TDX_L2_CONFIG || {};
const config = Object.assign({}, defaults, supplied);

const schema = "tdx-level2-probe/v1";
let emittedEvents = 0;
let limitReported = false;
let tpbusInstalled = false;
let tpbusTimer = null;
let eventBusSnapshotDone = false;
const recentSdkRequests = Object.create(null);
const recentTpSubscriptions = Object.create(null);

const expectedModules = {
  "tdxw.exe": {
    imageSize: 0x14c4000,
    sha256: "f5f2e6025a4d80bb1afbcb2a51c753d3c1909e09aa9d1bc8f7c3b701b081f74c",
  },
  "tpbus.dll": {
    imageSize: 0x696000,
    sha256: "0b6576270baf5b8421df7c282820306dcaee382b90f8b04ffd7e1075ebbcb481",
  },
};

function emit(event, fields, bypassLimit) {
  if (!bypassLimit && emittedEvents >= config.maxEvents) {
    if (!limitReported) {
      limitReported = true;
      send({
        schema,
        event: "capture-limit",
        max_events: config.maxEvents,
      });
    }
    return false;
  }
  if (!bypassLimit) {
    emittedEvents += 1;
  }
  const envelope = {schema, event};
  if (config.label) {
    envelope.capture_label = config.label;
  }
  send(Object.assign(envelope, fields || {}));
  return true;
}

function errorText(error) {
  try {
    return String(error.stack || error);
  } catch (_ignored) {
    return "unknown error";
  }
}

function bytesMatch(address, signature) {
  try {
    const expected = signature.split(" ").map(value => value === "??" ? null : parseInt(value, 16));
    const actual = new Uint8Array(address.readByteArray(expected.length));
    for (let index = 0; index < expected.length; index++) {
      if (expected[index] !== null && actual[index] !== expected[index]) {
        return false;
      }
    }
    return true;
  } catch (_error) {
    return false;
  }
}

function hookChecked(module, offset, signature, name, callbacks) {
  const address = module.base.add(offset);
  const end = module.base.add(module.size);
  if (address.compare(module.base) < 0 || address.compare(end) >= 0) {
    emit("hook-refused", {hook: name, reason: "address-outside-module"}, true);
    return false;
  }
  if (!bytesMatch(address, signature)) {
    emit("hook-refused", {
      hook: name,
      reason: "signature-mismatch",
      address: address.toString(),
    }, true);
    return false;
  }
  Interceptor.attach(address, callbacks);
  emit("hook-installed", {hook: name, address: address.toString()}, true);
  return true;
}

function moduleAllowed(module) {
  const expected = expectedModules[module.name.toLowerCase()];
  if (!expected) {
    return config.allowUnknownVersion;
  }
  if (module.size === expected.imageSize) {
    return true;
  }
  emit("module-version-mismatch", {
    module: module.name,
    path: module.path,
    expected_image_size: expected.imageSize,
    actual_image_size: module.size,
    expected_sha256: expected.sha256,
  }, true);
  return config.allowUnknownVersion;
}

function readFixedAscii(pointer, maximum) {
  if (!pointer || pointer.isNull()) {
    return "";
  }
  try {
    let result = "";
    for (let index = 0; index < maximum; index++) {
      const value = pointer.add(index).readU8();
      if (value === 0) {
        break;
      }
      if (value < 32 || value > 126) {
        break;
      }
      result += String.fromCharCode(value);
    }
    return result;
  } catch (_error) {
    return "";
  }
}

function readCString(pointer, maximum) {
  if (!pointer || pointer.isNull()) {
    return "";
  }
  try {
    return pointer.readUtf8String(maximum) || "";
  } catch (_error) {
    return readFixedAscii(pointer, maximum);
  }
}

function readMsvc2010String(object, maximum) {
  try {
    const length = object.add(16).readU32();
    const capacity = object.add(20).readU32();
    if (length > maximum || capacity < length || capacity > 16 * 1024 * 1024) {
      return null;
    }
    const source = capacity < 16 ? object : object.readPointer();
    return readCString(source, length + 1).slice(0, length);
  } catch (_error) {
    return null;
  }
}

function pointerOwner(address) {
  if (!address || address.isNull()) {
    return null;
  }
  try {
    const module = Process.findModuleByAddress(address);
    if (module !== null) {
      return {address: address.toString(), module: module.name, offset: address.sub(module.base).toString()};
    }
    const indirect = address.readPointer();
    const indirectModule = Process.findModuleByAddress(indirect);
    if (indirectModule !== null) {
      return {
        address: address.toString(),
        indirect_address: indirect.toString(),
        module: indirectModule.name,
        offset: indirect.sub(indirectModule.base).toString(),
      };
    }
  } catch (_error) {
    // Heap pointers and stale observer slots are expected possibilities.
  }
  return {address: address.toString(), module: null};
}

function snapshotEventBusRegistry(bus, tpbusModule) {
  if (eventBusSnapshotDone || !bus || bus.isNull()) {
    return;
  }
  try {
    const vtable = bus.readPointer();
    const observeImplementation = vtable.readPointer();
    if (!observeImplementation.equals(tpbusModule.base.add(0xe72ae))) {
      emit("eventbus-registry-refused", {
        reason: "unexpected-vtable",
        bus: bus.toString(),
        vtable: vtable.toString(),
        observe_implementation: observeImplementation.toString(),
      }, true);
      return;
    }
    eventBusSnapshotDone = true;
    const head = bus.add(44).readPointer();
    if (head.isNull()) {
      throw new Error("EventBus observer map has a null head");
    }
    const pending = [head.add(4).readPointer()];
    const seen = Object.create(null);
    const topics = [];
    const topicLimit = Math.min(Math.max(config.maxRecordsPerEvent, 1) * 16, 1024);
    while (pending.length > 0 && topics.length < topicLimit) {
      const node = pending.pop();
      if (!node || node.isNull() || node.equals(head)) {
        continue;
      }
      const nodeKey = node.toString();
      if (seen[nodeKey]) {
        continue;
      }
      seen[nodeKey] = true;
      pending.push(node.readPointer());
      pending.push(node.add(8).readPointer());

      const topic = readMsvc2010String(node.add(12), 1024);
      if (topic === null) {
        throw new Error("invalid EventBus topic string at " + nodeKey);
      }
      // map<string, shared_ptr<vector<ObserveInfo>>>: the pair value starts
      // at node+40, and its first word points to the vector object.
      const vector = node.add(40).readPointer();
      if (vector.isNull()) {
        throw new Error("null EventBus observer vector for " + topic);
      }
      const begin = vector.readPointer();
      const end = vector.add(4).readPointer();
      const byteLength = end.sub(begin).toInt32();
      if (byteLength < 0 || byteLength % 60 !== 0 || byteLength > 60 * 10000) {
        throw new Error("invalid EventBus observer vector for " + topic);
      }
      const observerCount = byteLength / 60;
      const observers = [];
      const detailLimit = Math.min(observerCount, config.maxRecordsPerEvent);
      for (let index = 0; index < detailLimit; index++) {
        const record = begin.add(index * 60);
        observers.push({
          index,
          callback_owner: pointerOwner(record.readPointer()),
          thread_type: record.add(36).readS32(),
          sticky: record.add(40).readU32() !== 0,
          observe_level: record.add(44).readU32(),
          weight: record.add(56).readS32(),
        });
      }
      topics.push({topic, observer_count: observerCount, observers});
    }
    topics.sort((left, right) => left.topic.localeCompare(right.topic));
    emit("eventbus-registry", {
      bus: bus.toString(),
      topic_count_reported: topics.length,
      topic_limit_reached: pending.length > 0,
      topics,
      matching_note: "A.B dispatches to A.* and A.B; bare A is not an implicit match",
    }, true);
  } catch (error) {
    emit("probe-error", {hook: "eventbus-registry", error: errorText(error)}, true);
  }
}

function bytesToHex(pointer, length) {
  if (!pointer || pointer.isNull() || length <= 0) {
    return "";
  }
  try {
    const bytes = new Uint8Array(pointer.readByteArray(length));
    return Array.from(bytes, value => value.toString(16).padStart(2, "0")).join("");
  } catch (_error) {
    return "";
  }
}

function readProtobufVarint(source, size, offset) {
  const start = offset;
  let value = 0;
  let multiplier = 1;
  let safe = true;
  let spelling = "";
  for (let index = 0; index < 10; index++) {
    if (offset >= size) {
      throw new Error("protobuf varint truncated at offset " + start);
    }
    const byte = source.add(offset).readU8();
    offset += 1;
    spelling += byte.toString(16).padStart(2, "0");
    if (index === 9 && byte > 1) {
      throw new Error("protobuf varint exceeds 64 bits at offset " + start);
    }
    if (safe) {
      const next = value + (byte & 0x7f) * multiplier;
      safe = Number.isSafeInteger(next);
      if (safe) {
        value = next;
        if (multiplier <= Number.MAX_SAFE_INTEGER / 128) {
          multiplier *= 128;
        } else {
          safe = false;
        }
      }
    }
    if ((byte & 0x80) === 0) {
      return {next: offset, value: safe ? value : null, hex: spelling};
    }
  }
  throw new Error("protobuf varint exceeds 10 bytes at offset " + start);
}

function protobufAsciiCandidate(source, length) {
  if (length <= 0 || length > Math.min(Math.max(config.rawSampleBytes, 0), 256)) {
    return null;
  }
  let result = "";
  for (let index = 0; index < length; index++) {
    const byte = source.add(index).readU8();
    if (byte < 32 || byte > 126) {
      return null;
    }
    result += String.fromCharCode(byte);
  }
  return result;
}

function inspectProtobufWire(source, size) {
  const fields = [];
  let offset = 0;
  const limit = Math.min(config.maxRecordsPerEvent, 100);
  try {
    while (offset < size && fields.length < limit) {
      const fieldOffset = offset;
      const key = readProtobufVarint(source, size, offset);
      offset = key.next;
      if (key.value === null) {
        throw new Error("protobuf field key is too large at offset " + fieldOffset);
      }
      const fieldNumber = Math.floor(key.value / 8);
      const wireType = key.value & 7;
      if (fieldNumber === 0 || ![0, 1, 2, 5].includes(wireType)) {
        throw new Error(
          "invalid protobuf key at offset " + fieldOffset
          + " (field=" + fieldNumber + ", wire=" + wireType + ")"
        );
      }
      const field = {
        index: fields.length,
        offset: fieldOffset,
        field_number: fieldNumber,
        wire_type: wireType,
        wire_type_name: ({0: "varint", 1: "fixed64", 2: "length_delimited", 5: "fixed32"})[wireType],
        key_hex: key.hex,
      };
      if (wireType === 0) {
        const item = readProtobufVarint(source, size, offset);
        offset = item.next;
        field.value_unsigned = item.value;
        field.value_hex = item.hex;
      } else if (wireType === 1 || wireType === 5) {
        const width = wireType === 1 ? 8 : 4;
        if (offset + width > size) {
          throw new Error("protobuf fixed field truncated at offset " + offset);
        }
        field.value_hex = bytesToHex(source.add(offset), width);
        offset += width;
      } else {
        const item = readProtobufVarint(source, size, offset);
        offset = item.next;
        if (item.value === null || item.value < 0 || offset + item.value > size) {
          throw new Error("protobuf length-delimited field truncated at offset " + fieldOffset);
        }
        const length = item.value;
        const sampleLength = Math.min(length, Math.max(config.rawSampleBytes, 0), 256);
        field.length = length;
        field.length_hex = item.hex;
        field.sample_hex = bytesToHex(source.add(offset), sampleLength);
        field.sample_truncated = sampleLength < length;
        const text = protobufAsciiCandidate(source.add(offset), length);
        if (text !== null) {
          field.ascii_candidate = text;
        }
        offset += length;
      }
      fields.push(field);
    }
    return {
      encoding: "protobuf_wire_unknown_schema",
      valid_prefix: true,
      field_count_reported: fields.length,
      field_limit_reached: offset < size,
      next_offset: offset,
      fields,
      interpretation_note: "field numbers and wire types only; business names and scalar meanings remain unknown",
    };
  } catch (error) {
    return {
      encoding: "protobuf_wire_unknown_schema",
      valid_prefix: false,
      field_count_reported: fields.length,
      next_offset: offset,
      fields,
      error: errorText(error),
    };
  }
}

function printableByte(value) {
  return value >= 32 && value <= 126 ? String.fromCharCode(value) : null;
}

function formatClock(secondsFromMidnight) {
  const normalized = ((secondsFromMidnight % 86400) + 86400) % 86400;
  const hours = Math.floor(normalized / 3600);
  const minutes = Math.floor((normalized % 3600) / 60);
  const seconds = normalized % 60;
  return [hours, minutes, seconds]
    .map(value => String(value).padStart(2, "0"))
    .join(":");
}

function readSecurity(pointer) {
  if (!pointer || pointer.isNull()) {
    return null;
  }
  try {
    const code = readFixedAscii(pointer, 6);
    if (!code) {
      return null;
    }
    return {
      code,
      market: pointer.add(280).readU16(),
    };
  } catch (_error) {
    return null;
  }
}

function targetMatches(target) {
  if (!config.targetCode) {
    return true;
  }
  return target !== null && target.code === config.targetCode;
}

function callbackTarget(mainModule, dataType, key1, key2) {
  if (dataType === 18031) {
    return recentSdkRequests[String(dataType)] || null;
  }
  try {
    const begin = mainModule.base.add(0x9f74f4).readPointer();
    const end = mainModule.base.add(0x9f74f8).readPointer();
    if (begin.isNull() || end.isNull() || end.compare(begin) < 0) {
      return recentSdkRequests[String(dataType)] || null;
    }
    const byteLength = end.sub(begin).toInt32();
    if (byteLength < 0 || byteLength > 25 * 10001) {
      return recentSdkRequests[String(dataType)] || null;
    }
    const count = Math.floor(byteLength / 25);
    for (let index = 0; index < count; index++) {
      const item = begin.add(index * 25);
      if (
        item.readU32() === key1
        && item.add(4).readU32() === key2
        && item.add(17).readU32() === dataType
      ) {
        return {
          market: item.add(8).readU16(),
          code: readFixedAscii(item.add(10), 6),
          consumer: item.add(21).readU32(),
          source: "callback-map",
        };
      }
    }
  } catch (_error) {
    // Fall through to the most recent request for this data type.
  }
  return recentSdkRequests[String(dataType)] || null;
}

function parseSdkTransactionRecords(source, count) {
  const records = [];
  const limit = Math.min(Math.max(count, 0), config.maxRecordsPerEvent);
  for (let index = 0; index < limit; index++) {
    const record = source.add(index * 52);
    try {
      records.push({
        time_raw: record.readU64().toString(),
        price: record.add(8).readDouble(),
        volume_raw: record.add(16).readU64().toString(),
        aux_1: record.add(36).readU32(),
        aux_2: record.add(40).readU32(),
        direction: record.add(48).readU32(),
      });
    } catch (error) {
      records.push({error: errorText(error)});
      break;
    }
  }
  return records;
}

function parseSdkOrderRecords(source, count) {
  const records = [];
  const limit = Math.min(Math.max(count, 0), config.maxRecordsPerEvent);
  for (let index = 0; index < limit; index++) {
    const record = source.add(index * 40);
    try {
      records.push({
        time_raw: record.readU64().toString(),
        price: record.add(8).readDouble(),
        volume_raw: record.add(16).readU64().toString(),
        order_id: record.add(24).readU32(),
        record_type: record.add(36).readU32(),
      });
    } catch (error) {
      records.push({error: errorText(error)});
      break;
    }
  }
  return records;
}

function boundedSideCounts(firstCount, secondCount) {
  let remaining = config.maxRecordsPerEvent;
  let firstLimit = Math.min(firstCount, Math.ceil(remaining / 2));
  let secondLimit = Math.min(secondCount, remaining - firstLimit);
  remaining -= firstLimit + secondLimit;
  if (remaining > 0) {
    const addFirst = Math.min(firstCount - firstLimit, remaining);
    firstLimit += addFirst;
    remaining -= addFirst;
  }
  if (remaining > 0) {
    secondLimit += Math.min(secondCount - secondLimit, remaining);
  }
  return [firstLimit, secondLimit];
}

function parseSdkMultiLevelSnapshot(source) {
  const firstCountRaw = source.add(8).readU32();
  const secondCountRaw = source.add(12).readU32();
  const firstCount = Math.min(firstCountRaw, 1000);
  const secondCount = Math.min(secondCountRaw, 1000);
  const limits = boundedSideCounts(firstCount, secondCount);
  const firstLevels = [];
  const secondLevels = [];

  function appendLevels(target, count, priceOffset, volumeOffset, auxiliaryOffset) {
    for (let index = 0; index < count; index++) {
      target.push({
        index,
        price: source.add(priceOffset + index * 8).readDouble(),
        volume_raw: source.add(volumeOffset + index * 4).readU32(),
        auxiliary_raw: source.add(auxiliaryOffset + index * 4).readU16(),
      });
    }
  }

  appendLevels(firstLevels, limits[0], 16, 8016, 12016);
  appendLevels(secondLevels, limits[1], 16016, 24016, 28016);
  return {
    header_u32_0: source.readU32(),
    header_u32_1: source.add(4).readU32(),
    first_side_count_raw: firstCountRaw,
    second_side_count_raw: secondCountRaw,
    first_side_levels: firstLevels,
    second_side_levels: secondLevels,
  };
}

function parseSdkOrderQueueSnapshot(source) {
  const countRaw = source.add(8).readU32();
  const count = Math.min(countRaw, 5000);
  const limit = Math.min(count, config.maxRecordsPerEvent);
  const quantities = [];
  for (let index = 0; index < limit; index++) {
    quantities.push(source.add(12 + index * 4).readU32());
  }
  return {
    header_u32_0: source.readU32(),
    header_u32_1: source.add(4).readU32(),
    count_raw: countRaw,
    quantities_raw: quantities,
  };
}

function parseTpbusQuotePush(source, size) {
  const depthCountRaw = source.add(24).readS8();
  const declaredCount = Math.max(depthCountRaw, 0);
  const availableCount = size >= 99 ? Math.floor((size - 99) / 20) : 0;
  const validCount = Math.min(declaredCount, availableCount);
  const limit = Math.min(validCount, config.maxRecordsPerEvent);
  const levels = [];
  for (let index = 0; index < limit; index++) {
    const level = source.add(99 + index * 20);
    levels.push({
      index,
      buy_price: level.readFloat(),
      buy_volume_raw: level.add(4).readU32(),
      buy_seat_count: level.add(8).readU16(),
      sell_price: level.add(10).readFloat(),
      sell_volume_raw: level.add(14).readU32(),
      sell_seat_count: level.add(18).readU16(),
    });
  }
  return {
    snapshot_kind: "quote_with_depth_levels",
    depth_count_raw: depthCountRaw,
    required_size: 99 + declaredCount * 20,
    truncated: depthCountRaw < 0 || availableCount < declaredCount,
    hq_time_raw: source.add(35).readU32(),
    item_number: source.add(39).readU32(),
    close: source.add(43).readFloat(),
    open: source.add(47).readFloat(),
    high: source.add(51).readFloat(),
    low: source.add(55).readFloat(),
    last: source.add(59).readFloat(),
    lead: source.add(63).readFloat(),
    volume_raw: source.add(67).readU32(),
    rest_volume_raw: source.add(71).readU32(),
    amount_raw: source.add(75).readFloat(),
    volume_in_stock_raw: source.add(79).readU32(),
    jjjz: source.add(83).readFloat(),
    in_out_flag: source.add(87).readU8(),
    hktt_flag: source.add(88).readU8(),
    volume_unit: source.add(89).readU8(),
    ph_volume: source.add(90).readFloat(),
    levels,
  };
}

function parseTpbusBestQueuesPush(source, size) {
  const buyCountRaw = source.add(36).readU32();
  const sellCountRaw = source.add(40).readU32();
  const availableCount = size >= 54 ? Math.floor((size - 54) / 4) : 0;
  const validBuyCount = Math.min(buyCountRaw, availableCount);
  const remaining = Math.max(availableCount - validBuyCount, 0);
  const validSellCount = buyCountRaw <= availableCount ? Math.min(sellCountRaw, remaining) : 0;
  const limits = boundedSideCounts(validBuyCount, validSellCount);
  const buyQuantities = [];
  const sellQuantities = [];
  for (let index = 0; index < limits[0]; index++) {
    buyQuantities.push(source.add(54 + index * 4).readU32());
  }
  if (buyCountRaw <= availableCount) {
    for (let index = 0; index < limits[1]; index++) {
      sellQuantities.push(source.add(54 + buyCountRaw * 4 + index * 4).readU32());
    }
  }
  return {
    snapshot_kind: "best_bid_ask_order_queues",
    refresh_number: source.add(24).readU32(),
    buy_price: source.add(28).readFloat(),
    sell_price: source.add(32).readFloat(),
    buy_count_raw: buyCountRaw,
    sell_count_raw: sellCountRaw,
    required_size: 54 + (buyCountRaw + sellCountRaw) * 4,
    truncated: validBuyCount < buyCountRaw || validSellCount < sellCountRaw,
    buy_quantities_raw: buyQuantities,
    sell_quantities_raw: sellQuantities,
  };
}

function parseInternalTransactions(pointer, count) {
  const records = [];
  const limit = Math.min(Math.max(count, 0), config.maxRecordsPerEvent);
  for (let index = 0; index < limit; index++) {
    const record = pointer.add(index * 20);
    try {
      const timeOffset = record.readU16();
      records.push({
        time_offset: timeOffset,
        time: formatClock(timeOffset + 21600),
        price_raw: record.add(2).readS32(),
        price: record.add(2).readS32() / 10000,
        volume_quotient: record.add(6).readS32(),
        volume_remainder: record.add(10).readU8(),
        status: record.add(11).readU8(),
        aux_1: record.add(12).readS32(),
        aux_2: record.add(16).readS32(),
      });
    } catch (error) {
      records.push({error: errorText(error)});
      break;
    }
  }
  return records;
}

function parseInternalOrders(pointer, count) {
  const records = [];
  const limit = Math.min(Math.max(count, 0), config.maxRecordsPerEvent);
  for (let index = 0; index < limit; index++) {
    const record = pointer.add(index * 20);
    try {
      const timeOffset = record.readU16();
      const side = record.add(11).readU8();
      const action = record.add(12).readU8();
      records.push({
        time_offset: timeOffset,
        time: formatClock(timeOffset + 21600),
        price_raw: record.add(2).readS32(),
        price: record.add(2).readS32() / 10000,
        volume_quotient: record.add(6).readS32(),
        volume_remainder: record.add(10).readU8(),
        side: printableByte(side),
        side_raw: side,
        action: printableByte(action),
        action_raw: action,
        order_id: record.add(16).readS32(),
      });
    } catch (error) {
      records.push({error: errorText(error)});
      break;
    }
  }
  return records;
}

function installMainHooks() {
  const mainModule = Process.mainModule;
  if (Process.pointerSize !== 4 || Process.arch !== "ia32") {
    emit("probe-refused", {
      reason: "expected-32-bit-x86",
      pointer_size: Process.pointerSize,
      arch: Process.arch,
    }, true);
    return false;
  }
  if (mainModule.name.toLowerCase() !== "tdxw.exe" || !moduleAllowed(mainModule)) {
    emit("probe-refused", {
      reason: "unexpected-main-module",
      module: mainModule.name,
      path: mainModule.path,
      image_size: mainModule.size,
    }, true);
    return false;
  }

  emit("module", {
    module: mainModule.name,
    path: mainModule.path,
    base: mainModule.base.toString(),
    image_size: mainModule.size,
    expected_sha256: expectedModules["tdxw.exe"].sha256,
  }, true);

  hookChecked(mainModule, 0x28c320, "55 8b ec 81 ec 44 04 00 00", "sdk-subscribe", {
    onEnter(args) {
      try {
        const codes = readCString(args[0], 1024);
        const count = args[1].toInt32();
        const dataType = args[2].toInt32();
        if (config.targetCode && codes.indexOf(config.targetCode) === -1) {
          return;
        }
        emit("sdk-subscribe", {
          data_type: dataType,
          count,
          codes,
          consumer: args[3].toInt32(),
        });
      } catch (error) {
        emit("probe-error", {hook: "sdk-subscribe", error: errorText(error)}, true);
      }
    },
  });

  hookChecked(mainModule, 0x28c510, "55 8b ec 83 ec 28", "sdk-request", {
    onEnter(args) {
      try {
        const security = readSecurity(args[0]);
        const dataType = args[1].toInt32();
        if (!targetMatches(security)) {
          return;
        }
        if (security !== null) {
          recentSdkRequests[String(dataType)] = Object.assign({
            source: "recent-sdk-request",
            observed_at_ms: Date.now(),
          }, security);
        }
        emit("sdk-request", Object.assign({
          data_type: dataType,
          request_mode: args[2].toInt32(),
        }, security || {}));
      } catch (error) {
        emit("probe-error", {hook: "sdk-request", error: errorText(error)}, true);
      }
    },
  });

  hookChecked(mainModule, 0x28c750, "55 8b ec 81 ec 88 03 00 00", "sdk-callback", {
    onEnter(args) {
      try {
        const dataType = args[0].toInt32();
        if ([1801, 1802, 1803, 1804, 1807, 18031, 18071].indexOf(dataType) === -1) {
          return;
        }
        const key1 = args[1].toUInt32();
        const key2 = args[2].toUInt32();
        const source = args[3];
        const count = args[4].toInt32();
        const target = callbackTarget(mainModule, dataType, key1, key2);
        if (!targetMatches(target)) {
          return;
        }
        const fields = Object.assign({
          data_type: dataType,
          key_1: key1,
          key_2: key2,
          count,
          callback_arg_6: args[5].toInt32(),
        }, target || {});
        if (!source.isNull() && dataType === 1801 && count > 0) {
          fields.source_record_size = 52;
          fields.records = parseSdkTransactionRecords(source, count);
        } else if (!source.isNull() && dataType === 1802 && count > 0) {
          fields.source_record_size = 40;
          fields.records = parseSdkOrderRecords(source, count);
        } else if (!source.isNull() && dataType === 1803) {
          fields.snapshot_kind = "multi_level_depth";
          fields.source_size = 0x7d10;
          fields.snapshot = parseSdkMultiLevelSnapshot(source);
        } else if (!source.isNull() && dataType === 18031) {
          fields.snapshot_kind = "order_queue_at_price";
          fields.source_size = 0x4e2c;
          fields.snapshot = parseSdkOrderQueueSnapshot(source);
        } else if (!source.isNull() && config.rawSampleBytes > 0) {
          fields.sample_hex = bytesToHex(source, Math.min(config.rawSampleBytes, 512));
        }
        emit("sdk-callback", fields);
      } catch (error) {
        emit("probe-error", {hook: "sdk-callback", error: errorText(error)}, true);
      }
    },
  });

  hookChecked(mainModule, 0x29ea00, "55 8b ec 83 ec 10 56 8b 75 08 85 f6", "decoded-transactions", {
    onEnter(args) {
      this.security = readSecurity(args[0]);
      this.output = args[2];
      this.capacity = args[3].toInt32() & 0xffff;
      this.capture = targetMatches(this.security);
    },
    onLeave(returnValue) {
      if (!this.capture || this.output.isNull()) {
        return;
      }
      try {
        const count = Math.min(returnValue.toInt32() & 0xffff, this.capacity);
        emit("decoded-transactions", Object.assign({
          count,
          record_size: 20,
          records: parseInternalTransactions(this.output, count),
        }, this.security || {}));
      } catch (error) {
        emit("probe-error", {hook: "decoded-transactions", error: errorText(error)}, true);
      }
    },
  });

  hookChecked(mainModule, 0x29eff0, "55 8b ec 81 ec b8 00 00 00", "decoded-orders", {
    onEnter(args) {
      this.security = readSecurity(args[0]);
      this.output = args[3];
      this.capacity = args[4].toInt32() & 0xffff;
      this.capture = targetMatches(this.security);
    },
    onLeave(returnValue) {
      if (!this.capture || this.output.isNull()) {
        return;
      }
      try {
        const count = Math.min(returnValue.toInt32() & 0xffff, this.capacity);
        emit("decoded-orders", Object.assign({
          count,
          record_size: 20,
          records: parseInternalOrders(this.output, count),
        }, this.security || {}));
      } catch (error) {
        emit("probe-error", {hook: "decoded-orders", error: errorText(error)}, true);
      }
    },
  });
  return true;
}

function subscriptionKey(market, code) {
  return String(market) + ":" + code;
}

function rememberTpSubscription(market, code, lx, operation) {
  const key = subscriptionKey(market, code);
  if (!recentTpSubscriptions[key]) {
    recentTpSubscriptions[key] = [];
  }
  const items = recentTpSubscriptions[key];
  items.push({market, code, lx, operation, observed_at_ms: Date.now()});
  while (items.length > 16) {
    items.shift();
  }
}

function recentTpSubscriptionContext() {
  const now = Date.now();
  const result = [];
  Object.keys(recentTpSubscriptions).forEach(key => {
    recentTpSubscriptions[key].forEach(item => {
      if (item.operation === "subscribe" && now - item.observed_at_ms <= config.correlationWindowMs) {
        result.push({
          market: item.market,
          code: item.code,
          lx: item.lx,
          age_ms: now - item.observed_at_ms,
        });
      }
    });
  });
  result.sort((left, right) => left.age_ms - right.age_ms);
  return result.slice(0, Math.min(config.maxRecordsPerEvent, 100));
}

function correlatedSubscriptions(market, code) {
  const now = Date.now();
  const items = recentTpSubscriptions[subscriptionKey(market, code)] || [];
  return items
    .filter(item => item.operation === "subscribe" && now - item.observed_at_ms <= config.correlationWindowMs)
    .map(item => ({lx: item.lx, age_ms: now - item.observed_at_ms}));
}

function installTpbusHooks() {
  if (tpbusInstalled) {
    return true;
  }
  const module = Process.findModuleByName("tpbus.dll");
  if (module === null) {
    return false;
  }
  if (!moduleAllowed(module)) {
    emit("tpbus-skipped", {reason: "module-version-mismatch", path: module.path}, true);
    tpbusInstalled = true;
    return false;
  }
  emit("module", {
    module: module.name,
    path: module.path,
    base: module.base.toString(),
    image_size: module.size,
    expected_sha256: expectedModules["tpbus.dll"].sha256,
  }, true);

  hookChecked(module, 0x76a7a, "55 8b ec 6a ff", "fasthq-subscribe", {
    onEnter(args) {
      try {
        const instance = this.context.ecx;
        const lx = args[0].toInt32();
        let code = readCString(args[1], 64);
        let market = 0;
        if (!instance.isNull()) {
          market = instance.add(796).readS32();
          const storedCode = readCString(instance.add(792).readPointer(), 64);
          if (!code) {
            code = storedCode;
          }
        }
        if (config.targetCode && code !== config.targetCode) {
          return;
        }
        const operation = args[3].toInt32() === 0 ? "subscribe" : "unsubscribe";
        rememberTpSubscription(market, code, lx, operation);
        emit("fasthq-subscribe", {
          code,
          market,
          lx,
          set_code: args[2].toInt32(),
          operation,
        });
      } catch (error) {
        emit("probe-error", {hook: "fasthq-subscribe", error: errorText(error)}, true);
      }
    },
  });

  hookChecked(module, 0x7b4e8, "68 28 09 00 00", "fasthq-push", {
    onEnter(args) {
      try {
        const pushType = args[0].toInt32();
        if (pushType !== 111 && pushType !== 112) {
          return;
        }
        const source = args[1];
        const size = args[2].toUInt32();
        if (source.isNull() || size < 8 || size > 16 * 1024 * 1024) {
          return;
        }
        const market = source.readU16();
        const code = readFixedAscii(source.add(2), 22);
        if (config.targetCode && code !== config.targetCode) {
          return;
        }
        const fields = {
          push_type: pushType,
          code,
          market,
          size,
          recent_subscriptions: correlatedSubscriptions(market, code),
        };
        if (pushType === 111 && size >= 99) {
          fields.snapshot = parseTpbusQuotePush(source, size);
        } else if (pushType === 112 && size >= 54) {
          fields.snapshot = parseTpbusBestQueuesPush(source, size);
        }
        if (config.rawSampleBytes > 0) {
          fields.sample_hex = bytesToHex(source, Math.min(size, config.rawSampleBytes, 512));
        }
        emit("fasthq-push", fields);
      } catch (error) {
        emit("probe-error", {hook: "fasthq-push", error: errorText(error)}, true);
      }
    },
  });

  // Types 113/114/116 are length-framed by sub_1007BFBE and passed through
  // MaintainData.HQPUSHPB without a schema.  This instruction is immediately
  // after the confirmed frame reader has written v41=[ebp-9Ch] and
  // v40=[ebp-A0h].  Observe that decoded frame before EventBus publication.
  hookChecked(module, 0x7c0fa, "39 1d ?? ?? ?? ?? 75 0d", "fasthq-protobuf-push", {
    onEnter() {
      try {
        const frame = this.context.ebp;
        const pushType = frame.sub(0x94).readS32();
        if (pushType !== 113 && pushType !== 114 && pushType !== 116) {
          return;
        }
        const size = frame.sub(0xa0).readU32();
        const source = frame.sub(0x9c).readPointer();
        if (source.isNull() || size === 0 || size > 16 * 1024 * 1024) {
          return;
        }
        const fields = {
          push_type: pushType,
          size,
          target_filter_applied: false,
          target_filter_note: "opaque protobuf has no confirmed code field",
          recent_subscriptions: recentTpSubscriptionContext(),
          wire: inspectProtobufWire(source, size),
        };
        if (config.rawSampleBytes > 0) {
          fields.sample_hex = bytesToHex(source, Math.min(size, config.rawSampleBytes, 512));
        }
        emit("fasthq-protobuf-push", fields);
      } catch (error) {
        emit("probe-error", {hook: "fasthq-protobuf-push", error: errorText(error)}, true);
      }
    },
  });

  if (config.snapshotEventBusRegistry) {
    hookChecked(module, 0xe72ae, "68 04 01 00 00", "eventbus-observe", {
      onEnter(args) {
        try {
          emit("eventbus-observe", {
            variant: 1,
            topic: readCString(args[0], 1024),
            observe_level: args[4].toUInt32(),
            thread_type: args[5].toInt32(),
          });
        } catch (error) {
          emit("probe-error", {hook: "eventbus-observe", error: errorText(error)}, true);
        }
      },
    });
    hookChecked(module, 0xe7666, "68 00 01 00 00", "eventbus-observe2", {
      onEnter(args) {
        try {
          emit("eventbus-observe", {
            variant: 2,
            topic: readCString(args[0], 1024),
            observe_level: args[4].toUInt32(),
            thread_type: args[5].toInt32(),
          });
        } catch (error) {
          emit("probe-error", {hook: "eventbus-observe2", error: errorText(error)}, true);
        }
      },
    });
    const getterAddress = module.base.add(0xda21a);
    if (bytesMatch(getterAddress, "83 3d ?? ?? ?? ?? 00 75 03")) {
      try {
        const getEventBus = new NativeFunction(getterAddress, "pointer", []);
        snapshotEventBusRegistry(getEventBus(), module);
      } catch (error) {
        emit("probe-error", {hook: "eventbus-getter", error: errorText(error)}, true);
      }
    } else {
      emit("hook-refused", {
        hook: "eventbus-getter",
        reason: "signature-mismatch",
        address: getterAddress.toString(),
      }, true);
    }
  }
  tpbusInstalled = true;
  return true;
}

function main() {
  try {
    if (!installMainHooks()) {
      return;
    }
    if (!installTpbusHooks()) {
      let attempts = 0;
      tpbusTimer = setInterval(() => {
        attempts += 1;
        if (installTpbusHooks() || attempts >= 120) {
          clearInterval(tpbusTimer);
          tpbusTimer = null;
          if (!tpbusInstalled) {
            emit("tpbus-skipped", {reason: "module-not-loaded"}, true);
          }
        }
      }, 500);
    }
    emit("ready", {
      capture_label: config.label || null,
      target_code: config.targetCode || null,
      max_events: config.maxEvents,
      max_records_per_event: config.maxRecordsPerEvent,
      raw_sample_bytes: config.rawSampleBytes,
    }, true);
  } catch (error) {
    emit("probe-error", {hook: "main", error: errorText(error)}, true);
  }
}

setImmediate(main);
