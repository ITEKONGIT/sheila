# sSheila release frontend

This directory contains the static release/download frontend.

## Pxxl deployment

- Deploy from the `frontend` branch of the parent `sheila` repository.
- Set the static publish directory to `release-site/dist`.
- No build command is required; the site is plain HTML, CSS, and JavaScript.
- Keep `release-config.js` configured with the public R2 origin used by the release workflow.

The page fetches `/latest.json` from that R2 origin and uses
`platforms.windows-x64.url` for the Windows download button. New releases update
the button through the manifest; the frontend does not need a code change.

For local development, leave the release base URL empty. The checked-in
`dist/latest.json` fixture and `dist/downloads/sSheila.exe` keep the page usable
without network credentials.
