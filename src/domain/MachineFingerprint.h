#pragma once

#include <QMap>
#include <QString>

// Machine fingerprint for node-locked licensing. C++ port of the LabDesk
// fingerprint.py: collects up to three stable Linux signals and returns them
// HASHED (SHA-256, app-salted) so the raw hardware ids never travel in the
// activation request or sit in the license file.
//
// Signals, in order of reliability:
//   * machine_id — /etc/machine-id (set at OS install; survives hardware swaps).
//   * mac        — first non-virtual, non-loopback NIC address.
//   * disk       — first physical block-device serial (best-effort).
//
// The license records whichever signals were available at issue time; the
// verifier (Licensing) tolerates one of them changing.
namespace MachineFingerprint {

// {name -> sha256hex} for every signal currently readable on this machine
// (omitting any that can't be read). Sorted by name (QMap) for stable output.
QMap<QString, QString> collectSignals();

// Short, human-friendly code for display/quick reference (NOT the binding data —
// the request token carries the full hashed signals). e.g. PD-9F3A-2C71-B048-5E16.
QString fingerprintCode();

} // namespace MachineFingerprint
