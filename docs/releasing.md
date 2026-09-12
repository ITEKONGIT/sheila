# Releasing sSheila

Releases are created by pushing to `main`. Feature branches and pull requests use the normal build/test workflow and cannot publish releases. The main-only workflow builds and tests the Windows and Linux server, creates the platform packages, signs the artifacts with Sigstore keyless signing, and publishes a GitHub Release.

The generated version uses the CMake project major/minor version and the GitHub Actions run number. For example, project version `0.1.0` on run `42` becomes `v0.1.42`. This keeps every main push uniquely releasable without allowing a feature branch to create a release.

Re-running the same main workflow run is safe: GitHub Release assets are replaced, while versioned R2 objects are immutable by convention. If an older generated version is manually re-run, it publishes its versioned objects but does not move the R2 `latest/` aliases backward.

## GitHub Actions configuration

The release workflow uses GitHub's OIDC token for Sigstore, so no signing private key is stored in GitHub. The resulting `.sig` and `.pem` files are attached to the GitHub Release and can be verified with Cosign.

R2 publishing is enabled by adding these repository or environment secrets:

- `CLOUDFLARE_ACCOUNT_ID`
- `R2_ACCESS_KEY_ID`
- `R2_SECRET_ACCESS_KEY`
- `R2_BUCKET`
- `R2_PUBLIC_BASE_URL`, such as `https://downloads.example.com`

If those secrets are absent, the GitHub Release still publishes and the R2 job reports a warning. Once configured, re-run the workflow for the tag to publish the R2 objects and `latest.json`.

## R2 layout

The workflow keeps immutable, versioned objects and moves only stable aliases for the frontend:

```text
releases/<tag>/windows/...
releases/<tag>/linux/...
latest/windows/sSheila.exe
latest/linux/sSheila.tar.gz
latest/SHA256SUMS
latest.json
```

The frontend should fetch `${R2_PUBLIC_BASE_URL}/latest.json` and use the `platforms.windows-x64.url` or `platforms.linux-x64.url` value. It should not scrape GitHub Releases or enumerate the R2 bucket.

The public R2 domain must allow `GET` and `HEAD` from the release-site origin if the frontend fetches `latest.json` with JavaScript. Direct download links do not require CORS, but the manifest request does.

## Verifying a release

Download the artifact, its `.sig`, and its `.pem` certificate, then run:

```sh
cosign verify-blob \
  --signature ssheila-v0.2.0-windows-x64.exe.sig \
  --certificate ssheila-v0.2.0-windows-x64.exe.pem \
  --certificate-identity-regexp '^https://github.com/ITEKONGIT/sheila/.github/workflows/release.yml@refs/heads/main$' \
  --certificate-oidc-issuer https://token.actions.githubusercontent.com \
  ssheila-v0.2.0-windows-x64.exe
```

The same process applies to the Linux archive and `SHA256SUMS`. Verify the checksum after signature verification:

```sh
sha256sum --ignore-missing -c SHA256SUMS
```
