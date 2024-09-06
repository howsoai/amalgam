const fs = require("node:fs");
const path = require("node:path");
const { describe, expect, test } = require("@jest/globals");
const AmalgamRuntime = require(path.resolve(
  process.env.AMALGAM_WASM_DIR,
  "amalgam-st.cjs"
));

describe("Test Amalgam Webassembly", () => {
  let amlg;

  beforeAll(async () => {
    const binary = fs.readFileSync(
      path.resolve(process.env.AMALGAM_WASM_DIR, "amalgam-st.wasm")
    );
    amlg = await AmalgamRuntime({
      wasmBinary: binary,
      getPreloadedPackage: function (packagePath) {
        // Manually load package data from file system
        const data = fs.readFileSync(packagePath);
        return data.buffer;
      },
      locateFile: function (filepath) {
        // Override the local file method to use local file system
        return path.resolve(process.env.AMALGAM_WASM_DIR, filepath);
      },
    });
    amlg.pointerToString = function (ptr) {
      // Convert pointer to UTF-8 string value and free memory
      let value;
      try {
        value = amlg.UTF8ToString(Number(ptr));
      } finally {
        amlg._free(ptr);
      }
      return value;
    };
  });

  test("get version", async () => {
    // Test that GetVersionString returns valid semver value
    const semverRegex = /^(\d+\.)(\d+\.)(\d+)(.*)$/;
    const getVersion = amlg.cwrap("GetVersionString", "string", []);
    const version = getVersion();
    expect(typeof version).toBe("string");
    expect(version).toMatch(semverRegex);
  });

  test("get concurrency type", async () => {
    // Test concurrency type is SingleThreaded
    const ptr = amlg.ccall("GetConcurrencyTypeString", "number", [], []);
    const value = amlg.pointerToString(ptr);
    expect(typeof value).toBe("string");
    expect(value).toMatch("SingleThreaded");
  });

  test("get max threads", async () => {
    // Test max threads is 1
    const value = amlg.cwrap("GetMaxNumThreads", "number", [])();
    expect(typeof value).toBe("bigint");
    expect(Number(value)).toEqual(1);
  });
});
