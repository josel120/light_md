#!/usr/bin/env bash
# Signs light_md.app and packs it as a zip. Used by .github/workflows/build.yml.
#
#   packaging/macos/sign.sh dist/light_md.app light_md-v1.4.0-macos.zip
#
# With an Apple Developer ID (see docs/macos-signing.md) the app is signed with it, with the
# hardened runtime, sent to Apple to be notarized, and the notarization ticket is stapled to
# it: then it opens with a normal double-click. Without those secrets it gets the ad-hoc
# signature Apple Silicon needs to start at all, and users confirm it once in System Settings.
#
# Secrets (environment variables):
#   MACOS_CERTIFICATE           the "Developer ID Application" certificate, .p12, base64
#   MACOS_CERTIFICATE_PASSWORD  the password of that .p12
#   MACOS_SIGNING_IDENTITY      e.g. "Developer ID Application: Jane Doe (ABCDE12345)"
#   APPLE_ID                    the Apple ID email of the developer account
#   APPLE_TEAM_ID               the 10-character team ID
#   APPLE_APP_PASSWORD          an app-specific password for that Apple ID
set -euo pipefail

app="$1"
zip="$2"

missing=()
for secret in MACOS_CERTIFICATE MACOS_CERTIFICATE_PASSWORD MACOS_SIGNING_IDENTITY APPLE_ID APPLE_TEAM_ID APPLE_APP_PASSWORD; do
    [ -n "${!secret:-}" ] || missing+=("$secret")
done
if [ "${#missing[@]}" -gt 0 ]; then
    # Some but not all set is a setup mistake: say which, and still produce a package.
    if [ "${#missing[@]}" -lt 6 ]; then
        echo "::warning::Developer ID signing skipped, missing secrets: ${missing[*]}"
    fi
    echo "No Developer ID secrets: ad-hoc signature (users confirm the app once in System Settings)."
    codesign --force --deep --sign - "$app"
    codesign --verify --deep --strict "$app"
    ditto -c -k --keepParent "$app" "$zip"
    exit 0
fi

# A throwaway keychain holding only the certificate, deleted at the end.
keychain="$RUNNER_TEMP/signing.keychain-db"
keychain_password="$(uuidgen)"
certificate="$RUNNER_TEMP/certificate.p12"
cleanup() {
    security delete-keychain "$keychain" 2> /dev/null || true
    rm -f "$certificate"
}
trap cleanup EXIT
echo "$MACOS_CERTIFICATE" | base64 --decode > "$certificate"
security create-keychain -p "$keychain_password" "$keychain"
security set-keychain-settings -lut 21600 "$keychain"
security unlock-keychain -p "$keychain_password" "$keychain"
security import "$certificate" -P "$MACOS_CERTIFICATE_PASSWORD" -A -t cert -f pkcs12 -k "$keychain"
security set-key-partition-list -S apple-tool:,apple: -k "$keychain_password" "$keychain" > /dev/null
security list-keychains -d user -s "$keychain" $(security list-keychains -d user | tr -d '"')

sign() {
    codesign --force --timestamp --options runtime --keychain "$keychain" \
        --sign "$MACOS_SIGNING_IDENTITY" "$1"
}

# Inside out: every library, plugin and framework first, the app itself last.
while IFS= read -r -d '' item; do
    sign "$item"
done < <(find "$app/Contents" \( -name "*.dylib" -o -name "*.so" \) -type f -print0)
while IFS= read -r -d '' item; do
    sign "$item"
done < <(find "$app/Contents" -name "*.framework" -type d -prune -print0)
sign "$app"
codesign --verify --deep --strict --verbose=2 "$app"

# Apple checks the app (a few minutes) and issues a ticket, stapled to the app so it also
# opens without a network connection.
ditto -c -k --keepParent "$app" "$zip"
xcrun notarytool submit "$zip" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" \
    --password "$APPLE_APP_PASSWORD" --wait
xcrun stapler staple "$app"
xcrun stapler validate "$app"
spctl --assess --type execute --verbose "$app"

# The zip again, now with the ticket inside.
rm -f "$zip"
ditto -c -k --keepParent "$app" "$zip"
echo "Signed and notarized: $zip"
