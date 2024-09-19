# Getting Started

## Install dependencies

```bash
npm install
```

## Define environment variables

Define the environment variable `AMALGAM_WASM_NODEJS_DIR` and `AMALGAM_WASM_BROWSER_DIR` to the directory where the following files exist:

- amalgam-st.wasm
- amalgam-st.cjs
- amalgam-st.data

On your personal machine, you may find it easier to copy [.env.sample](./.env.sample) to `.env`.

## Run tests

```bash
npm run test
```
