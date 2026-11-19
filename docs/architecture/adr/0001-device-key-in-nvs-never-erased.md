---
id: ADR-0001
title: Keep the device key in NVS and never erase NVS to make room for it
status: proposed
date: 2026-11-14
requirements: [ESP-0004, ESP-KEY-0001, ESP-KEY-0002, SYS-0022]
supersedes: []
superseded_by: []
---

# Keep the device key in NVS and never erase NVS to make room for it

## Context

A board's key is rotated over its session without flashing it
(robohub:ADR-0010), so the rotated key must survive a reset. ESP-IDF already
has NVS, a wear-levelled key-value store in the `nvs` partition of the default
partition table, which an application may also use. NVS can need erasing (no
free pages, a newer format), and the usual recipe — erase and initialise again —
throws away everything in it.

## Decision

The keystore is two NVS blobs, `a` and `b`, in namespace `rh_keys`, holding
robohub-common's A/B records (robohub/keystore_core.h), kept even though NVS
writes atomically so that every platform chooses a key by the same rules. The
first keystore call initialises NVS when the application has not; when NVS
needs erasing, the keystore reports failure instead. The board's component
names the keystore symbols undefined on the link, so the NVS implementation is
always the one linked.

## Consequences

- A key survives a reset and an ordinary reflash, which does not touch the
  `nvs` partition.
- A board whose NVS needs erasing falls back to its compiled-in key until a
  person erases it deliberately.
- On a plain build NVS is readable from flash; protecting it needs NVS
  encryption, which needs flash encryption, and both are in the production
  preset (ADR-0002).
