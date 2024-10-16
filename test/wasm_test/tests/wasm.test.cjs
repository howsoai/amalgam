const fs = require("node:fs");
const path = require("node:path");
const { describe, expect, test } = require("@jest/globals");

const AmalgamRuntime = require(path.resolve(
  process.env.AMALGAM_WASM_DIR,
  process.env.AMALGAM_BASE_FILE + ".cjs"
));

describe("Test Amalgam Webassembly", () => {
  let amlg;

  beforeAll(async () => {
    const binary = fs.readFileSync(
      path.resolve(
        process.env.AMALGAM_WASM_DIR,
        process.env.AMALGAM_BASE_FILE + ".wasm"
      )
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
  });

  test("get version", async () => {
    // Test that GetVersionString returns valid semver value
    const semverRegex = /^(\d+\.)(\d+\.)(\d+)(.*)$/;
    const getVersion = amlg.cwrap("GetVersionString", "string", []);
    const version = getVersion();
    expect(typeof version).toBe("string");
    expect(version).toMatch(semverRegex);
  });
});
