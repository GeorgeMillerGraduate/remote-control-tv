# QR pairing and remote access

## Same Wi-Fi

Start the PC server. Use its real LAN IPv4 address in Advertised LAN / VPN IP. A computer with several interfaces may default to an unsuitable one: check this before generating the QR. Port changes take effect when you stop and restart the server.

Open the QR window. Scan with the phone's integrated CameraX/ML Kit scanner. Read and approve the new-device request on the PC. Keep the QR window open until approval completes: closing it cancels approval, even if scanning has already occurred. Closing and reopening also generates a fresh code. An unused QR expires after 120 seconds; unauthenticated sockets time out after 30 seconds, so approve promptly after scanning.

The temporary secret is sent only over the connection whose certificate matches the QR fingerprint. The PC saves a SHA-256 hash of a new 256-bit credential; the phone saves the credential encrypted by Android Keystore. Temporary QR fields are removed before saving. Later connections verify the same server identity and use the long-term credential.

The phone list allows Connect, address changes and Forget computer. Forget on the phone removes that local entry; revoke on the PC to invalidate the credential at the server. PC Manage trusted phones supports individual disconnect, revoke and rename. One phone controls at a time.

## Another network / mobile data

1. Configure WireGuard or Tailscale separately on the PC and phone. Both must be authorised on the same private network and permitted by its ACLs.
2. Verify the PC's private VPN IP is reachable from the phone. Keep the PC awake and the VPN running.
3. For initial pairing, advertise that VPN IP before showing the QR. The phone can scan the screen in person or scan a privately transmitted image on a second display. Someone at the PC still must approve. The app cannot scan an image displayed on the same phone and has no gallery-import feature.
4. If already paired, edit that saved computer's address to its VPN IP. The certificate pin and client credential stay unchanged.
5. Connect. The handshake verifies identity and permission. Failure messages distinguish application errors from general reachability/TLS failures only approximately; detailed diagnostics remain manual.

A QR does not create a VPN, cross NAT or open a firewall. No router port forwarding is needed for a correctly working mesh VPN. Do not publicly expose the control port.

Discovery uses UDP broadcast on default port 8765 and only updates addresses for already trusted certificate fingerprints. It does not add trust. It is not used for remote VPN discovery or custom ports. QR pairing works without discovery.
