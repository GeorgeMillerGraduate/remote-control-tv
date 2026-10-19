# Security model and review status

This source has not had an independent security audit or live adversarial/device test. Use it initially on a trusted LAN/VPN with devices you own.

## Implemented boundaries

- QR grants only a brief opportunity to request trust. Both a valid unconsumed token and PC approval are required.
- Ticket consumption is atomic. Expired/cancelled codes cannot be consumed; cancellation/expiry also prevent approval after consumption.
- Exact certificate pinning is checked in Android's X509TrustManager before credentials are sent. Private IP hostname validation is replaced by that out-of-band certificate identity; no general trust-all fallback exists. Certificate validity is also checked.
- Server private key is a PKCS12 file. The password is in a separate user-private file; both rely on operating-system account/ACL protection. This is not TPM-backed encryption or protection against malware running as the same user. A secure-permission failure aborts startup.
- Android credentials are AES-GCM encrypted using a non-exportable Android Keystore key. App backup is disabled. Hardware backing depends on the device.
- PC registry writes use private temporary files and atomic replacement; unsupported filesystems fail the save rather than silently weakening this. PC stores only hashes of high-entropy client credentials, plus device names. Never log input text, QR tokens or credentials. The current UI logs connection state/errors but does not implement a durable full audit trail.
- Mouse, keyboard, media and viewing permissions are enforced per command. Viewing is off by default. Changing permissions disconnects sessions and releases held inputs.
- Emergency stop closes current sockets. It does not delete trusted credentials; use revoke to do that. Held inputs are released on disconnect and control-command errors.
- One active controller prevents different phones from interleaving key ownership.
- Discovery is an untrusted address hint. It cannot change a certificate pin or grant trust.
- No arbitrary remote shell, hidden startup, UAC/lock-screen bypass, NAT forwarding or public relay is implemented.

## Practical limitations

The service binds all interfaces so a private VPN can reach it; use host firewall rules to constrain the listening port. There is no built-in per-interface ACL. Turn off the server when unnecessary. A stolen unlocked phone with a saved credential can control an approved PC until you revoke its trust. Someone with the PC user account can replace its files or control the PC directly.

Device names are self-reported labels, not independently verified hardware identities. Approval has no second numeric verification code: possession of the short-lived QR plus the pinned server connection is the bootstrap mechanism. Do not send QR images through untrusted channels.

Unicode text intentionally overwrites the host clipboard then sends Ctrl+V. Clipboard managers may retain that text. Do not use it to transfer passwords. Restoration is deliberately not claimed because clipboard ownership/paste timing vary by application.

Known limitations include no per-IP authentication-failure backoff, TLS-handshake socket quotas beyond library defaults, Android process-death UI restoration, protection against all local-network denial-of-service, or formal protocol fuzzing. Dependency versions are fixed; review advisories and update with compatibility testing before distribution. Native JNA media and OS APIs require tests on target platforms.

Reset server identity only by stopping the application and deliberately removing its `.jenga-remote` state directory. This forgets phones and changes the pin; pair again. Do not share that directory in bug reports.
