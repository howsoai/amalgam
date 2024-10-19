# Getting Started

## Install dependencies

```bash
npm install
```

## Define environment variables

Define the environment variable `AMALGAM_WASM_DIR` and `AMALGAM_BASE_FILE` to the directory where the following files exist:

- amalgam-st(-debug).wasm
- amalgam-st(-debug).cjs
- amalgam-st(-debug).data

On your personal machine, you may find it easier to copy [.env.sample](./.env.sample) to `.env`.

## Run tests

```bash
npm run test
```
