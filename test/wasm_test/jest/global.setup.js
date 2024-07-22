import { config } from "dotenv";

export default async () => {
  config();
  console.info("process.env.AMALGAM_WASM_DIR", process.env.AMALGAM_WASM_DIR);
};
