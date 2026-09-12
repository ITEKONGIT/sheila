#!/usr/bin/env bash

set -euo pipefail

release_dir=${1:?release directory is required}
release_tag=${2:?release tag is required}
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

: "${R2_ACCOUNT_ID:?R2_ACCOUNT_ID is required}"
: "${R2_ACCESS_KEY_ID:?R2_ACCESS_KEY_ID is required}"
: "${R2_SECRET_ACCESS_KEY:?R2_SECRET_ACCESS_KEY is required}"
: "${R2_BUCKET:?R2_BUCKET is required}"
: "${R2_PUBLIC_BASE_URL:?R2_PUBLIC_BASE_URL is required}"

endpoint="https://${R2_ACCOUNT_ID}.r2.cloudflarestorage.com"
version_root="releases/${release_tag}"

export AWS_ACCESS_KEY_ID="$R2_ACCESS_KEY_ID"
export AWS_SECRET_ACCESS_KEY="$R2_SECRET_ACCESS_KEY"
export AWS_DEFAULT_REGION=auto

upload() {
    local source=$1
    local key=$2
    local content_type=$3
    local cache_control=$4
    local disposition=${5:-}
    local args=(s3 cp "$source" "s3://${R2_BUCKET}/${key}" --endpoint-url "$endpoint" --content-type "$content_type" --cache-control "$cache_control" --only-show-errors)
    if [[ -n "$disposition" ]]; then
        args+=(--content-disposition "$disposition")
    fi
    aws "${args[@]}"
}

windows="${release_dir}/ssheila-${release_tag}-windows-x64.exe"
linux="${release_dir}/ssheila-${release_tag}-linux-x64.tar.gz"
checksums="${release_dir}/SHA256SUMS"

for required_file in "$windows" "$linux" "$checksums" \
    "${windows}.sig" "${windows}.pem" \
    "${linux}.sig" "${linux}.pem" \
    "${checksums}.sig" "${checksums}.pem" \
    "${release_dir}/latest.SHA256SUMS" "${release_dir}/latest.SHA256SUMS.sig" "${release_dir}/latest.SHA256SUMS.pem" \
    "${release_dir}/latest.json" "${release_dir}/latest.json.sig" "${release_dir}/latest.json.pem"; do
    [[ -f "$required_file" ]] || { echo "Missing release file: $required_file" >&2; exit 1; }
done

immutable_cache="public,max-age=31536000,immutable"
latest_cache="public,max-age=300,must-revalidate"
attachment_exe='attachment; filename="sSheila.exe"'
attachment_tar='attachment; filename="sSheila.tar.gz"'

upload "$windows" "${version_root}/windows/ssheila-${release_tag}-windows-x64.exe" "application/vnd.microsoft.portable-executable" "$immutable_cache" "$attachment_exe"
upload "${windows}.sig" "${version_root}/windows/ssheila-${release_tag}-windows-x64.exe.sig" "application/octet-stream" "$immutable_cache"
upload "${windows}.pem" "${version_root}/windows/ssheila-${release_tag}-windows-x64.exe.pem" "application/x-pem-file" "$immutable_cache"

upload "$linux" "${version_root}/linux/ssheila-${release_tag}-linux-x64.tar.gz" "application/gzip" "$immutable_cache" "$attachment_tar"
upload "${linux}.sig" "${version_root}/linux/ssheila-${release_tag}-linux-x64.tar.gz.sig" "application/octet-stream" "$immutable_cache"
upload "${linux}.pem" "${version_root}/linux/ssheila-${release_tag}-linux-x64.tar.gz.pem" "application/x-pem-file" "$immutable_cache"

upload "$checksums" "${version_root}/SHA256SUMS" "text/plain; charset=utf-8" "$immutable_cache"
upload "${checksums}.sig" "${version_root}/SHA256SUMS.sig" "application/octet-stream" "$immutable_cache"
upload "${checksums}.pem" "${version_root}/SHA256SUMS.pem" "application/x-pem-file" "$immutable_cache"

current_manifest=$(mktemp)
trap 'rm -f "$current_manifest"' EXIT
if aws s3 cp "s3://${R2_BUCKET}/latest.json" "$current_manifest" --endpoint-url "$endpoint" --only-show-errors; then
    current_tag=$(python3 -c 'import json, sys; print(json.load(open(sys.argv[1], encoding="utf-8"))["tag"])' "$current_manifest")
    if ! python3 "$script_dir/compare-release-tags.py" "$current_tag" "$release_tag"; then
        echo "Keeping R2 latest at $current_tag; $release_tag is older. Versioned artifacts were published."
        exit 0
    fi
fi

upload "$windows" "latest/windows/sSheila.exe" "application/vnd.microsoft.portable-executable" "$latest_cache" "$attachment_exe"
upload "${windows}.sig" "latest/windows/sSheila.exe.sig" "application/octet-stream" "$latest_cache"
upload "${windows}.pem" "latest/windows/sSheila.exe.pem" "application/x-pem-file" "$latest_cache"

upload "$linux" "latest/linux/sSheila.tar.gz" "application/gzip" "$latest_cache" "$attachment_tar"
upload "${linux}.sig" "latest/linux/sSheila.tar.gz.sig" "application/octet-stream" "$latest_cache"
upload "${linux}.pem" "latest/linux/sSheila.tar.gz.pem" "application/x-pem-file" "$latest_cache"

upload "${release_dir}/latest.SHA256SUMS" "latest/SHA256SUMS" "text/plain; charset=utf-8" "$latest_cache"
upload "${release_dir}/latest.SHA256SUMS.sig" "latest/SHA256SUMS.sig" "application/octet-stream" "$latest_cache"
upload "${release_dir}/latest.SHA256SUMS.pem" "latest/SHA256SUMS.pem" "application/x-pem-file" "$latest_cache"
upload "${release_dir}/latest.json" "latest.json" "application/json; charset=utf-8" "$latest_cache"
upload "${release_dir}/latest.json.sig" "latest.json.sig" "application/octet-stream" "$latest_cache"
upload "${release_dir}/latest.json.pem" "latest.json.pem" "application/x-pem-file" "$latest_cache"
