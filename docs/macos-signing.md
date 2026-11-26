# Signing and notarizing the macOS app

Until the app is signed with an Apple Developer ID and notarized by Apple, macOS blocks it the
first time and users have to click **Open Anyway** in System Settings → Privacy & Security.
The build is ready for it: `packaging/macos/sign.sh` signs and notarizes as soon as the GitHub
secrets below exist, and keeps the ad-hoc signature until then.

## One-time setup (needs a Mac)

1. Join the [Apple Developer Program](https://developer.apple.com/programs/) (99 USD a year).
2. In Xcode → Settings → Accounts → Manage Certificates, create a
   **Developer ID Application** certificate. (Or create it at
   [developer.apple.com/account](https://developer.apple.com/account/resources/certificates).)
3. In Keychain Access, find that certificate under **My Certificates**, right-click it →
   **Export**, and save it as `certificate.p12` with a password.
4. At [account.apple.com](https://account.apple.com) → Sign-In and Security →
   **App-Specific Passwords**, create one named "light_md notarization".
5. Your team ID is on [developer.apple.com/account](https://developer.apple.com/account) →
   Membership details (10 characters).

## The GitHub secrets

In the repository: **Settings → Secrets and variables → Actions → New repository secret**.

| Secret | Value |
|---|---|
| `MACOS_CERTIFICATE` | the .p12 as base64: `base64 -i certificate.p12 \| pbcopy`, then paste |
| `MACOS_CERTIFICATE_PASSWORD` | the password from step 3 |
| `MACOS_SIGNING_IDENTITY` | the certificate's name, e.g. `Developer ID Application: Jane Doe (ABCDE12345)` (`security find-identity -v -p codesigning` lists it) |
| `APPLE_ID` | the Apple ID email of the developer account |
| `APPLE_TEAM_ID` | the team ID from step 5 |
| `APPLE_APP_PASSWORD` | the app-specific password from step 4 |

## Check it

Run the **Build** workflow (Actions → Build → Run workflow). The macOS step should end with
"Signed and notarized". Download the macOS package from the run, unzip it on a Mac and
double-click light_md: it opens without the security warning. Then remove the "Open Anyway"
sentence from the READMEs (issue #23).

Pull requests from forks don't get the secrets, so their builds keep the ad-hoc signature.
