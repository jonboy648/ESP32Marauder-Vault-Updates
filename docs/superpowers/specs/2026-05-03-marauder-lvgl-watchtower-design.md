# ESP32 Marauder LVGL Rebuild + Watchtower Feature

**Status:** Design (awaiting review)
**Date:** 2026-05-03
**Author:** Brainstorming session, jonboy648 fork (`ESP32Marauder-Vault-Updates`)
**Target hardware:** Marauder V6.2 (V6.1 firmware path, ESP32-D0WD-V3, 320×240 ILI9341)

## 1. Overview

This spec covers two coupled work streams shipped together:

1. **A full LVGL UI rebuild** of the Marauder firmware, replacing the current TFT_eSPI button-grid renderer with an LVGL-driven UI that supports modern primitives (status bar, modals, tabs, banners, theming, animations) while preserving the existing 191-item menu structure and full backward compatibility with stock Marauder behavior.

2. **A new mega feature, "Watchtower"** — a continuous-awareness mode that runs in the background, records every nearby RF observation tagged with time + GPS + classification, surfaces interesting items in real-time across four priority tiers, and provides on-device triage of past sessions. Includes three layers of intelligence (rules + location baselines + behavioral/CSI fingerprinting) plus single-antenna spatial inference (RSSI-gradient tail detection, GPS-anchored device positioning, on-demand Locate mode).

The two streams are coupled because Watchtower's UI requirements drive several LVGL primitives that the rebuild must provide (live banner, tab bar inside a screen, modal alert overlay, map widget). Building them separately would force the LVGL work to be either over- or under-specified.

## 2. Goals & Non-Goals

### Goals

- **Preserve full Marauder feature set.** All 191 existing menu items and 73 scan/attack modes remain reachable via the same logical paths. Existing user muscle memory is honored.
- **Deliver a genuine intelligence uplift.** Watchtower's combined rules + baselines + fingerprinting + CSI + RSSI-gradient stack must produce alerts measurably more useful than naive event lists.
- **Honesty about hardware limits.** No UI shall imply data we do not have (e.g., no fake compass bearing from a single antenna).
- **Stay rebase-friendly with upstream `justcallmekoko/ESP32Marauder`.** UI rebuild operates on the renderer/navigation layer; menu structure remains data-driven from upstream's existing definitions. Watchtower lives in clearly-bounded new files.
- **Single board target for v1: Marauder V6.2 (uses V6.1 firmware path).** Other boards remain compilable but Watchtower is gated behind `HAS_WATCHTOWER` and only enabled for V6.1+.

### Non-Goals

- **Cross-board UI parity.** Older/different boards (V4, Mini, Cardputer) keep the existing TFT_eSPI renderer. LVGL build is V6.x-targeted only.
- **Cloud/network sync of Watchtower data.** Captures stay on-device. Federated learning, remote control, web UI over device AP are explicitly out of scope.
- **ML inference (TFLite Micro).** Tier 4 intelligence deferred to a future cycle once a labeled dataset exists.
- **New attack capabilities.** This cycle is awareness + UI. New attacks (WPA3, direction-finding via dual radios, Wi-Fi 6) belong to the next cycle.
- **Phone/companion app.** Pairing for off-device alerts deferred.
- **Reorganizing the existing menu hierarchy.** Watchtower is added as the 6th home-grid tile; the rest of the structure is unchanged.

## 3. Watchtower: Behavior & Runtime Contract

### 3.1 Operating modes

Watchtower runs in one of three states:

- **ACTIVE** — capturing events, classifying, updating Live views, firing alerts. Default state when enabled and not blocked.
- **PAUSED** — temporarily suspended because an active attack (deauth, beacon spam, evil portal, etc.) has the radio. The session log records a gap entry; UI shows "paused — radio in use by {attack}". Resumes automatically when attack ends.
- **OFF** — not running at all. No captures, no log writes. User must explicitly disable.

State transitions are deterministic and event-driven. There is no "automatic off after N minutes" — once enabled, Watchtower runs until disabled or the device powers down.

### 3.2 Coexistence rule

Watchtower runs alongside any **passive** scan (signal monitor, packet count, channel analyzer, sniffers in observe-only mode). Passive scans contribute their captured frames to Watchtower's event pipeline rather than competing with it.

Watchtower **pauses** during any **active** scan/attack that requires exclusive radio control on a fixed channel. The list of attacks that pause Watchtower is enumerated in `WatchtowerPauseConditions.h` and reviewed at every release. When pausing, a gap-event is recorded with `{start_ts, end_ts, reason, attack_type}` so the triage view can show the gap honestly ("attack ran 14:32–14:38, no Watchtower data").

### 3.3 Alert tiers

Four priority tiers, mapped to four output behaviors:

| Tier | Examples | Screen behavior | NeoPixel | Backlight | Buzzer (gated) |
|---|---|---|---|---|---|
| **CRITICAL** | Deauth flood, evil twin, Karma responder | Hard interrupt: full-screen overlay, must dismiss | Solid red, fast pulse | 3-pulse strobe | 4× rapid 800Hz |
| **HIGH** | Tail, AirTag-class, Flock signature, surveillance OUI | Soft banner: top-of-screen card, 6s auto-dismiss | Solid orange 3s, then dim | None | Single 600Hz chirp |
| **MEDIUM** | Hidden SSID, MAC-randomization defeat, unusual channel | Status-bar icon + radar list highlight | Yellow blink ×2 | None | Single 400Hz blip |
| **LOW** | First-seen MAC, novel-at-this-location | Status-bar icon only | Cyan single blip | None | Silent |

Buzzer is gated behind `HAS_BUZZER` define (V6.2 default: undefined; alert engine no-ops the audio path). All other channels are always active.

### 3.4 User dismissal & snooze

CRITICAL alerts must be acknowledged (tap dismiss). HIGH banners auto-dismiss after 6s; user may also tap to dismiss early. MEDIUM/LOW have no per-event dismissal — they accumulate in the radar list.

User may **snooze a tier** for 5/15/60 minutes via Settings (e.g., "snooze LOW alerts for 1 hour while at this coffee shop"). Snoozed alerts are still logged to SD; only the output channels are suppressed.

User may **mute an individual MAC** for the rest of the session via the Detail tab on a flagged item. Muted MACs continue to appear in the log but generate no alerts. Useful for a known-friendly device that keeps tripping the "tail" rule (e.g., your own AirTag).

## 4. UI Architecture (LVGL)

### 4.1 Display layers

LVGL renders the Marauder UI in three composited layers:

- **Base layer** — the active screen (home grid, a menu, or the Watchtower Synthesis view). Owns its own widgets and lifecycle. Switching screens is `lv_scr_load(target_screen)`.
- **Top layer** — a persistent status bar (12px tall) drawn at the very top of the display via `lv_disp_top_layer`. Always visible. Shows time, GPS fix state, battery %, RF activity LEDs, current Watchtower mode (ACTIVE / PAUSED / OFF).
- **System layer** — modal alert overlay used only by CRITICAL alerts. Drawn on `lv_disp_sys_layer` so it sits above everything including menus. Hard-blocks input until dismissed.

This three-layer split is what lets a CRITICAL deauth alert interrupt a sniff session without losing the user's place: the base-layer screen state is preserved and resumes after the modal is dismissed.

### 4.2 Home screen (option B — menu home + Watchtower banner)

The 6-tile home grid is preserved with one tile redirected to Watchtower:

```
┌─ status bar ────────────────────────────────────┐
│ 14:32  GPS✓  BAT 78%  ●●●            ACTIVE      │
├─ alert summary banner (only when alerts >0) ────┤
│ ⚠ 1 CRITICAL  ▲ 2 HIGH       [tap for radar]    │
├─ 2x3 grid ──────────────────────────────────────┤
│   WiFi          │   Bluetooth                    │
│   GPS           │   Watchtower                   │
│   Device        │   Reboot                       │
└─────────────────────────────────────────────────┘
```

The Watchtower tile glyph reflects current alert level: dim cyan (no alerts), yellow (MEDIUM), orange (HIGH), red (CRITICAL). Tapping the banner OR the Watchtower tile both jump to the Synthesis screen — same destination, two entry points.

### 4.3 Watchtower Synthesis screen (option 4)

Tabbed view with persistent bottom alert strip:

```
┌─ status bar ────────────────────────────────────┐
│   tab bar:  [MAP] [LIST] [DETAIL] [SET]          │
├─ active tab content ────────────────────────────┤
│   (varies by tab — see 4.4)                     │
├─ bottom alert strip (always visible) ───────────┤
│   ⚠ deauth -38dBm           14s ago    [✕ ack]  │
└─────────────────────────────────────────────────┘
```

The bottom strip shows the highest-tier unacknowledged event. CRITICAL events stay until tapped; HIGH events auto-fade after 6s. The strip is the single non-modal channel that surfaces alerts regardless of which tab is active.

### 4.4 Tab contents

| Tab | Content | Update rate |
|---|---|---|
| **MAP** | GPS-anchored device positions over walked path. Range rings at 5/25/100m. Bearing arrows from RSSI gradient. Tap a marker → DETAIL tab populates. | 1 Hz |
| **LIST** | Live event feed, newest first, color-coded by tier. Hidden SSID indicator, RSSI, distance estimate, age. Tap row → DETAIL. | 2 Hz |
| **DETAIL** | Selected device's full record: classification reason chain, RSSI history sparkline, GPS path it appeared on, fingerprint match confidence, mute control. | on demand |
| **SETTINGS** | Tier snooze toggles, baseline reset, current rule set version, log file size, "export session" command. | static |

### 4.5 Navigation rules

- **Hardware buttons:** Marauder's 4-button layout (UP/DOWN/SELECT/BACK) is preserved. UP/DOWN traverse rows in LIST and zoom in MAP. SELECT enters DETAIL. BACK returns to home grid (or to home from any depth via long-press).
- **Tab switching:** SELECT-long while on Synthesis cycles tabs (MAP → LIST → DETAIL → SETTINGS → MAP).
- **Alert dismissal:** SELECT-tap on the bottom strip acknowledges the displayed event. CRITICAL modal is dismissed by SELECT-tap.
- **Entering existing menus:** unchanged from current Marauder behavior. The LVGL renderer wraps `MenuFunctions` output but preserves all 191 existing menu paths.

### 4.6 Triage mode

Wardrive Triage (post-session review) reuses the Synthesis screen with one difference: the bottom alert strip is replaced by a timeline scrubber. MAP/LIST/DETAIL tabs show the frozen state at the scrubber's position. SETTINGS is replaced by an EXPORT tab. One layout, two contexts — saves substantial LVGL work.

## 5. Data Model

### 5.1 Event record (96 bytes, packed)

Every observation Watchtower captures becomes a `wt_event_t`:

```c
typedef struct __attribute__((packed)) wt_event_t {
    uint32_t  ts_unix;          // 4   capture time (seconds since epoch, GPS-disciplined)
    uint16_t  ts_ms;            // 2   sub-second remainder
    uint8_t   src_kind;         // 1   WT_SRC_WIFI_BEACON | WT_SRC_WIFI_PROBE | WT_SRC_BLE_ADV | ...
    uint8_t   tier;             // 1   0=LOW 1=MED 2=HIGH 3=CRIT
    uint8_t   mac[6];           // 6   source MAC
    int8_t    rssi;             // 1   dBm
    uint8_t   channel;          // 1   wifi channel or BLE PHY id
    int32_t   gps_lat_e7;       // 4   latitude * 1e7 (degrees)
    int32_t   gps_lon_e7;       // 4   longitude * 1e7
    int16_t   gps_alt_m;        // 2   meters, signed
    uint8_t   gps_fix_quality;  // 1   0=no fix, 1=2D, 2=3D, 3=DGPS
    uint8_t   reason_code;      // 1   classification result, see 6.x
    uint16_t  rule_bitmap;      // 2   which rules fired (Tier 1)
    uint16_t  baseline_z_q8;    // 2   Tier 2 z-score, fixed-point Q8 (signed)
    uint8_t   fp_match_pct;     // 1   Tier 3 fingerprint match confidence 0-100
    uint8_t   _pad0;            // 1
    char      ssid[32];         // 32  SSID or BLE name (null-padded)
    uint8_t   csi_summary[16];  // 16  compressed CSI features (multipath, motion)
    uint16_t  seq;              // 2   monotonic sequence within session
    uint16_t  crc;              // 2   CRC-16 over preceding 94 bytes
} wt_event_t;                   // 96 bytes total
```

### 5.2 SRAM ring buffer

- Capacity: **512 events** (48 KB), allocated in PSRAM if available, else internal SRAM.
- Producer: capture ISR / RX callbacks. Lock-free SPSC ring (single producer, single consumer).
- Consumer: SD-flush task running at 1 Hz (or when ring crosses 384 events, whichever first).
- Overflow policy: drop oldest non-CRITICAL event. CRITICAL events are pinned and never dropped — if the ring is full of CRITs, the producer blocks (this is intentional; we'd rather block briefly than lose attack evidence).

### 5.3 SD log format

One file per session: `/wt/YYYYMMDD_HHMMSS.wtl` (Watchtower Log).

- **Header (96 bytes)**: magic `WTLG`, version (uint16), session start ts, hardware ID, firmware version, baseline-set hash, rule-set hash. Same size as one event so block alignment stays clean.
- **Body**: contiguous `wt_event_t` records, no delimiters. Length implied by file size: `(filesize - 96) / 96`.
- **Footer (96 bytes)**: written on graceful close. Magic `WTLF`, end ts, total event count, count-per-tier, CRC of body. Absent on power-loss; reader must tolerate truncated logs.

A 4 KB SD sector holds exactly 42 events + 64 bytes slack (used for in-flight buffer alignment metadata). Flush writes whole sectors only.

### 5.4 Baseline storage

Baselines (Tier 2) live in a separate, persistent file: `/wt/baselines.db`. Format is a flat array of `wt_baseline_t` records keyed by GPS H3 cell at resolution 9 (~150 m hex):

```c
typedef struct __attribute__((packed)) wt_baseline_t {
    uint64_t  h3_cell;          // 8   H3 index, resolution 9
    uint32_t  first_seen;       // 4
    uint32_t  last_seen;        // 4
    uint32_t  cumulative_dwell; // 4   seconds spent at this cell (for "30 min / 2 visits" rule)
    uint16_t  visit_count;      // 2
    uint16_t  unique_macs;      // 2   approximate, via counting Bloom filter elsewhere
    uint8_t   typical_rssi_mean;// 1   normalized population mean
    uint8_t   typical_rssi_sd;  // 1   standard deviation
    uint8_t   _pad[6];          // 6   reserved
} wt_baseline_t;                // 32 bytes
```

Baseline becomes "established" when `cumulative_dwell >= 1800s AND visit_count >= 2` (per option B from the brainstorm — moderately aggressive). Until then, the cell is "learning" and Tier 2 contributes no z-score to events captured there.

### 5.5 Fingerprint storage

Tier 3 device fingerprints live in `/wt/fingerprints.db`. Bounded LRU of 1024 entries (~64 KB):

```c
typedef struct __attribute__((packed)) wt_fingerprint_t {
    uint8_t   mac[6];           // 6
    uint8_t   _pad0[2];         // 2
    uint32_t  first_seen;       // 4
    uint32_t  last_seen;        // 4
    uint16_t  observation_count;// 2
    uint8_t   csi_signature[32];// 32  64-subcarrier fingerprint, compressed
    uint8_t   probe_pattern[8]; // 8   hash of probe-request SSID set
    uint8_t   timing_features[4];//4   inter-frame timing summary
    uint8_t   movement_score;   // 1   0=static, 255=highly mobile
    uint8_t   tail_score;       // 1   0=never near user, 255=persistent tail
    uint8_t   _pad1[4];         // 4
} wt_fingerprint_t;             // 64 bytes
```

LRU eviction: when full, drop the entry with the lowest `(observation_count * recency_weight)` product. Tail-score-positive entries are pinned and never evicted automatically.

## 6. Classification Rules (Tier 1 starter set)

Twelve rules across four tiers. Each rule has: a clear firing condition, a `reason_code` written into the event, a human-readable reason string, and an estimated false-positive cost. Rules are evaluated in priority order (CRITICAL first); the highest-tier rule that matches sets `event.tier`. Lower-tier rules that also match are recorded in `rule_bitmap` for the explainability chain (see §7).

### 6.1 CRITICAL tier — active attacks

**R1. Deauth flood** (`reason_code = 0x10`)
- **Fires when:** ≥5 deauth or disassoc frames observed within 10s from a non-self source MAC.
- **Reason string:** `"Deauth attack from {mac}"`
- **FP risk:** Low. Real APs rarely emit ≥5 deauths in 10s outside legitimate roaming events; the 10s window absorbs most of those.
- **Cost:** O(1) per frame using a sliding-window counter keyed by source MAC.

**R2. Evil twin AP** (`reason_code = 0x11`)
- **Fires when:** beacon SSID exactly matches a previously-seen SSID, BSSID differs, AND new beacon's RSSI is ≥10 dB stronger than the strongest historical sample of the legitimate BSSID.
- **Reason string:** `"Possible evil twin: '{ssid}' ({bssid})"`
- **FP risk:** Medium-low. Captures dual-radio enterprise APs occasionally; the RSSI delta requirement filters most legitimate cases.
- **Cost:** O(log N) lookup against the SSID→BSSID-set table maintained per session.

**R3. Karma / Pineapple responder** (`reason_code = 0x12`)
- **Fires when:** the same source MAC sends probe-responses for ≥3 distinct SSIDs within 5s.
- **Reason string:** `"Karma behavior: {mac} answering as {n} SSIDs"`
- **FP risk:** Very low. Legitimate APs almost never claim multiple SSIDs from one radio in this pattern.
- **Cost:** O(1) per response, small per-MAC counter.

### 6.2 HIGH tier — surveillance / following

**R4. Persistent tail** (`reason_code = 0x20`)
- **Fires when:** the same MAC has been observed at ≥3 distinct GPS points spaced ≥100 m apart, all within the last 30 minutes, while the user was moving.
- **Reason string:** `"Followed across {n} stops over {min}m"`
- **FP risk:** Medium. Two real-world false-positive sources: (a) MACs from stationary fleets like buses/trains briefly co-located along a route, mitigated by requiring motion variance in the device's GPS track; (b) the user's own paired devices, mitigated by the per-MAC mute control.
- **Cost:** O(log N) per event using a per-MAC sparse GPS-point list (max 8 points retained per MAC). Cleared after 30 min of no observations.

**R5. AirTag-class BLE beacon** (`reason_code = 0x21`)
- **Fires when:** BLE advertisement matches Apple Find My, Tile, Samsung SmartTag, or Chipolo packet signatures, AND the same advertising address has been seen for ≥10 minutes following the user's GPS path.
- **Reason string:** `"AirTag-class tracker: {vendor} for {min}m"`
- **FP risk:** Low for the trackers the user doesn't own; the per-MAC mute is the escape hatch for the user's own tags.
- **Cost:** O(1) packet signature check; reuses R4's per-MAC GPS list.

**R6. Flock / LPR signature** (`reason_code = 0x22`)
- **Fires when:** WiFi probe pattern or BLE advertisement matches the existing Flock-detection OUI list and packet structure already shipped in upstream Marauder (justcallmekoko/ESP32Marauder Flock module).
- **Reason string:** `"Flock LPR camera: {model_guess}"`
- **FP risk:** Very low — the existing Flock detection has a curated OUI list and structural checks; we reuse it verbatim.
- **Cost:** O(1) per frame; piggybacks on the existing Flock scanner state.

**R7. Surveillance / IMSI-catcher OUI** (`reason_code = 0x23`)
- **Fires when:** OUI matches a curated list of vendors associated with surveillance gear (Stingray-class, IMSI catchers, known LE/IC equipment vendors). List is maintained in `WatchtowerOUIList.h` and is reviewed at every release.
- **Reason string:** `"Surveillance vendor OUI: {vendor}"`
- **FP risk:** Medium — some of these vendors also make legitimate consumer/enterprise gear. The rule fires HIGH but the reason chain in §7 is what makes the alert actionable; combined with R4/R5 it becomes much more confident.
- **Cost:** O(1) hash lookup against the OUI list (~256 entries, fits in flash).

### 6.3 MEDIUM tier — anomalies

**R8. Hidden SSID** (`reason_code = 0x30`)
- **Fires when:** beacon frame has SSID-length = 0 OR SSID = all-zero bytes, observed for ≥30s on a single BSSID.
- **Reason string:** `"Hidden SSID on {bssid} ch{channel}"`
- **FP risk:** Low for false positives in the technical sense — the rule is correct. The interesting question is whether hidden SSIDs are *worth* flagging at MEDIUM; we say yes because the population of hidden SSIDs skews toward (a) consumer paranoia and (b) deliberately covert APs, and the user can always snooze the tier.
- **Cost:** O(1) per beacon; small per-BSSID hidden-duration counter.

**R9. MAC randomization defeat** (`reason_code = 0x31`)
- **Fires when:** two probe requests within 60s have different MACs but identical vendor-specific IE payload signatures (HT capabilities, extended capabilities, vendor IEs ordered identically). Indicates the same physical device behind randomized MACs.
- **Reason string:** `"Same device behind {n} randomized MACs"`
- **FP risk:** Medium-low. Modern stacks have converged on a small set of IE patterns; collisions are possible but the 60s window keeps them manageable. Worth flagging because it's a genuine privacy-erosion signal.
- **Cost:** O(N) hash of IE payload + O(log N) lookup against a 64-entry recent-IE-signature LRU. Recompute hash only on probe requests, not beacons.

**R10. Unusual channel for SSID** (`reason_code = 0x32`)
- **Fires when:** a known-SSID baseline exists (SSID seen ≥10 times historically) AND a new beacon for that SSID arrives on a channel never seen before for that SSID.
- **Reason string:** `"'{ssid}' on unexpected channel {channel}"`
- **FP risk:** Medium. Real APs sometimes change channels for DFS or congestion reasons. The rule is most useful when combined with R2 (evil-twin) — see §7 reason chains.
- **Cost:** O(1) lookup; per-SSID channel bitmask stored in the SSID baseline table.

### 6.4 LOW tier — novelty

**R11. First-seen MAC** (`reason_code = 0x40`)
- **Fires when:** the source MAC is not present in the global session-history Bloom filter (1024-bit, ~3% FP rate at 10k inserts).
- **Reason string:** `"New device: {mac} ({vendor})"`
- **FP risk:** N/A — by design, this rule is just "first time observed." The Bloom filter's collision rate means a small fraction of devices won't trigger first-seen even though they should; this is acceptable.
- **Cost:** O(1) Bloom check + insert. Bloom is reset at session start.

**R12. Novel at this location** (`reason_code = 0x41`)
- **Fires when:** the user's current GPS position has an established Tier 2 baseline (see §5.4) AND the source MAC is not present in that baseline's MAC bloom filter.
- **Reason string:** `"Unusual here: {mac}"`
- **FP risk:** Low once baselines are established. Before establishment, the rule fails closed and emits nothing — this avoids the day-1 "everything is novel" problem.
- **Cost:** O(1) Bloom check against the per-baseline filter (256-bit, stored alongside `wt_baseline_t`).

## 7. Intelligence Layers and Fusion

Watchtower's intelligence is layered. Each event passes through up to three independent classifiers; their combined output is what gets shown to the user. The fourth layer (ML) is explicitly out of scope for v1.

### 7.1 The three active layers

**Tier 1 — Rules (§6).** Hand-written, deterministic, fast. Every event is evaluated by all 12 rules; the highest matching tier sets `event.tier`, and the `rule_bitmap` records every match. Rules answer: *"Does this event match a known pattern of interest?"*

**Tier 2 — Location baselines (§5.4, option B from brainstorm).** Per-H3-cell statistics built up as the user moves. Established after 30 minutes cumulative dwell across at least 2 visits. Tier 2 produces a z-score on RSSI, MAC-population novelty, and channel distribution. Z-score is written to `event.baseline_z_q8`. Tier 2 answers: *"Is this event unusual for THIS place, given how I've seen it before?"*

**Tier 3 — Behavioral / CSI fingerprinting.** Per-device fingerprint built from CSI summary, probe-pattern hash, and timing features. A new event is matched against the 1024-entry fingerprint LRU; the best match's confidence (0–100) is written to `event.fp_match_pct`. Tier 3 answers: *"Have I seen this specific device before, even if its MAC has changed?"*

### 7.2 Fusion → reason chain

The four output tiers (CRITICAL/HIGH/MEDIUM/LOW) are determined by Tier 1. Tiers 2 and 3 do not change the alert tier — they enrich the reason chain.

When the Detail tab is opened on an event, the reason chain is rendered as up to three lines:

```
RULE     R4 Persistent tail (4 stops, 28 min)
PLACE    +3.2σ unusual at this baseline (Cafe Mile-End)
DEVICE   91% match: same fingerprint as MAC seen at Library yesterday
```

Lines are present only when their layer contributed evidence. A bare "first-seen MAC" event might show only the RULE line; a fully-corroborated tail event shows all three. The chain is the *explanation* — it's why the user can trust the alert.

### 7.3 Confidence-scored fingerprint matches (option C from brainstorm)

Tier 3 is not binary. The 0–100 confidence comes from the cosine similarity of CSI signatures, weighted by probe-pattern overlap and timing-feature distance. Three thresholds:

| Confidence | UI label | Behavior |
|---|---|---|
| 90–100 | "match" | Reason chain says "same device as {prior_mac}". |
| 60–89 | "likely match" | Chain says "likely same device as {prior_mac} ({n}%)". |
| 0–59 | (no match) | No DEVICE line in chain. |

This explicit confidence band is what made option C compelling in the brainstorm: a single threshold would flip the chain on/off arbitrarily, while three bands let the user see the gradient.

### 7.4 Tier 4 (TFLite Micro) — deferred

A future cycle. Two prerequisites must exist first:

1. A labeled dataset of confirmed attacks captured on V6.x hardware, large enough to train a small classifier.
2. A measurement showing the rule engine has hit a clear false-negative ceiling (i.e., there are real attacks rules can't catch).

Until both are true, Tier 4 would be technical debt without payoff. The data model (§5.1) reserves no Tier 4 fields specifically — when added, Tier 4 output will live in a sidecar file keyed by `event.seq`, not in the event record itself, to keep on-disk format stable.

## 8. Spatial Inference on Single-Antenna Hardware

### 8.1 Hardware reality

Marauder V6.2 has **one** WiFi/BLE antenna (shared, on the ESP32-WROVER) and **one** GPS antenna. Earlier discussion of "dual antenna" features was incorrect — those belong to V8 / Apex 5G / Double Barrel variants. Everything in this section works with a single antenna.

This rules out: angle-of-arrival (AoA), MUSIC, ESPRIT, classical direction-finding. None of those work without ≥2 antennas in known geometric relationship.

What's still feasible — and where Watchtower extracts genuinely novel value — is summarized below.

### 8.2 GPS-anchored RSSI gradient (synthetic aperture via the user)

**The trick.** As the user walks, the device samples the same target's RSSI at multiple GPS-tagged positions. If RSSI rises while walking north, the target is north of you. Over 30s of motion with ≥3 samples at distinct GPS points, the gradient gives a usable bearing.

- **Hardware needed:** what V6.2 has (1 WiFi/BLE antenna + GPS).
- **Output:** bearing estimate ±~30° after 30s of walking. NOT a point fix; a direction.
- **Caveat:** **requires user motion**. Stationary user → no bearing. The UI must label bearings with a freshness/motion indicator and decay them when the user stops moving.
- **What it enables:** the persistent-tail rule (R4) gets dramatically stronger. A real tail's RSSI stays roughly constant as you walk (because they're moving with you); a static device's RSSI changes systematically with your motion. The gradient *is* the tail signature.

The MAP tab (§4.4) renders bearings as short arrows from the user's current GPS point toward the inferred direction, with arrow translucency = freshness.

### 8.3 CSI multipath fingerprinting

**The trick.** Every WiFi packet's preamble lets the receiver compute Channel State Information — 64 subcarriers of phase + amplitude. With a single antenna you can't compute AoA, but the CSI vector is a fingerprint of the multipath environment between transmitter and receiver.

- **Use 1 — line-of-sight vs through-wall.** LoS environments produce smooth CSI magnitude across subcarriers; non-LoS (wall, body, obstruction) produces deep notches at frequency-selective fading nulls. We classify devices as LoS / NLoS / unknown and surface this in the Detail tab.
- **Use 2 — motion detection.** CSI variance across consecutive packets reveals if anything in the propagation path is moving (including people walking nearby, even if neither endpoint is moving). Used by R4 (persistent tail) as a corroborating signal when the user is stationary.
- **Use 3 — device fingerprinting (Tier 3).** A 32-byte compressed CSI signature feeds the fingerprint LRU (§5.5). Same physical device produces similar CSI signatures across MAC rotations, which is how R9 (MAC randomization defeat) and Tier 3's confidence score get their evidence.

CSI extraction on ESP32 uses the espressif/ESP-CSI library; we link it conditionally behind `HAS_CSI` (default ON for V6.x). The CSI callback fires per-packet and writes the compressed signature directly into `event.csi_summary`.

### 8.4 On-demand Locate mode

A user-triggered, focused mode reachable from the Detail tab on any device. While Locate is active:

- The radio fixes channel to the target's last-seen channel.
- Sample rate increases to 10 Hz on the target MAC.
- The screen shows a single large RSSI dial + a bearing arrow + range ring (RSSI→distance estimate, ±30% accuracy).
- A "warmer/colder" tone (gated behind `HAS_BUZZER`) plays when motion + RSSI delta agree on a direction.

Locate is an *active* mode in §3.2's terms — it pauses normal Watchtower capture for the duration. The event log records a Locate session record at start and end, so triage can replay what happened.

### 8.5 What we will NOT do

This section exists to make commitments to the user about hardware honesty:

- **No fake compass bearings** — bearings are only displayed when the user has been moving; stationary-user UI shows distance ranges only.
- **No false precision in distance estimates** — RSSI→distance is shown as bands (5m / 25m / 100m), never as decimal meters.
- **No claims of "tracking" before R4 has fired** — proximity ≠ following.
- **No AoA-style direction indicators** — single antenna cannot produce AoA. Any UI that looks like AoA (e.g., a needle pointing at a device) must come from RSSI gradient, with motion freshness shown alongside.

## 9. File Layout and Module Boundaries

### 9.1 New files (all under `esp32_marauder/`)

LVGL renderer:

- `LvglDisplay.cpp` / `.h` — LVGL initialization, display flush callback bridging to TFT_eSPI, input device registration, top/sys layer setup. Wraps existing `Display.cpp` rather than replacing it for legacy boards.
- `LvglScreens.cpp` / `.h` — screen-creation factories: `make_home()`, `make_menu(menu_def_t*)`, `make_synthesis()`. Owns LVGL widget hierarchy per screen.
- `LvglStatusBar.cpp` / `.h` — the persistent top-layer status bar (clock, GPS, battery, RF, Watchtower mode).
- `LvglAlertOverlay.cpp` / `.h` — system-layer modal for CRITICAL alerts.

Watchtower core:

- `Watchtower.cpp` / `.h` — coordinator. State machine (ACTIVE/PAUSED/OFF), event ingest queue, dispatch to classifier, ring buffer, SD flush task. Subscribes to `WiFiScan` and BLE callbacks rather than driving them.
- `WatchtowerEvent.h` — `wt_event_t`, `wt_baseline_t`, `wt_fingerprint_t` struct definitions, reason codes, source kind enum.
- `WatchtowerRules.cpp` / `.h` — the 12 Tier 1 rules from §6. Each rule is a function `bool fire(const wt_event_t&, wt_context_t&, char* reason_out)` + a registration in a static rule table.
- `WatchtowerBaselines.cpp` / `.h` — Tier 2 H3-cell baselines, persistence to `/wt/baselines.db`, z-score computation.
- `WatchtowerFingerprints.cpp` / `.h` — Tier 3 LRU, CSI-signature compression, match confidence.
- `WatchtowerSpatial.cpp` / `.h` — RSSI-gradient bearing estimation, distance bands, Locate mode.
- `WatchtowerCSI.cpp` / `.h` — ESP-CSI integration, per-packet CSI callback, signature extraction.
- `WatchtowerLog.cpp` / `.h` — SD log writer, header/footer, sector-aligned flush.
- `WatchtowerOUIList.h` — surveillance-vendor OUI table for R7. Reviewed at every release.
- `WatchtowerPauseConditions.h` — list of attacks that pause Watchtower (per §3.2).

UI screens specific to Watchtower:

- `LvglWatchtowerSynthesis.cpp` / `.h` — the tabbed Synthesis screen (Map/List/Detail/Settings).
- `LvglWatchtowerMap.cpp` / `.h` — Map tab widget, GPS-anchored marker rendering.
- `LvglWatchtowerList.cpp` / `.h` — List tab, virtualized event list (LVGL `lv_table` or `lv_list`).
- `LvglWatchtowerDetail.cpp` / `.h` — Detail tab, reason-chain renderer, sparkline.
- `LvglWatchtowerSettings.cpp` / `.h` — Settings tab.
- `LvglAlertStrip.cpp` / `.h` — bottom alert strip (always-visible on Synthesis).

### 9.2 Existing files touched (minimal-surface principle)

- `configs.h` — add `HAS_WATCHTOWER`, `HAS_CSI`, `HAS_LVGL_UI` defines per board. V6.1/V6.2 enable all three; older boards leave them off and continue to use the existing TFT_eSPI renderer.
- `esp32_marauder.ino` — at boot, branch on `HAS_LVGL_UI`: initialize LVGL stack and Watchtower, or fall back to existing `Display::main()`. One added `#ifdef` block, ~15 lines.
- `MenuFunctions.cpp` — **menu data structure unchanged**. The existing `MenuNode` array (with the descriptions added on this branch) is the source of truth. LVGL renders it; we don't re-author menu definitions. No edits to add/remove menu items in this work.
- `WiFiScan.cpp` — add a packet-callback hook so Watchtower can subscribe to RX frames without owning the radio. New: `WiFiScan::registerWatchtowerCallback(cb)`. ~20 lines added; existing scan modes unchanged.
- `GpsInterface.cpp` — add a `getCurrentFix(wt_gps_fix_t*)` accessor used by Watchtower's per-event GPS tagging. Read-only; no behavior changes.
- `SDInterface.cpp` — expose a sector-aligned write helper for the Watchtower log writer. ~30 lines added.
- `LedInterface.cpp` — add tier→color/pattern mapping for Watchtower NeoPixel signaling. Existing patterns unchanged.

No other existing files are modified.

### 9.3 Rebase-friendly boundaries

The branch's coupling to upstream is concentrated in five well-known files (`configs.h`, `esp32_marauder.ino`, `WiFiScan.cpp`, `GpsInterface.cpp`, `SDInterface.cpp`, `LedInterface.cpp`). All edits are **additive**: new functions, new defines, new callback registrations. Nothing renames or restructures existing APIs.

When upstream releases a new version, the rebase strategy is:

1. Rebase Watchtower's new files cleanly (no conflicts — they're new).
2. Resolve conflicts only in the six touched files above, line-by-line. Each touched section is small and bounded.
3. Re-run the testing matrix from §11.

This is the same pattern the description-rendering branch already uses successfully — the same six-or-so files get touched, additively, and rebases against a fast-moving upstream remain tractable.

## 10. Implementation Phases

Six phases. Each ends with a hardware-test gate on a real V6.2 device — the next phase does not start until the gate passes.

### 10.1 Phase 1 — LVGL skeleton + navigation parity

**Goal:** the existing Marauder UI runs under LVGL with no behavior change visible to a user. Status bar appears; modal overlay path exists but unused.

**Deliverables:**
- `LvglDisplay`, `LvglScreens`, `LvglStatusBar`, `LvglAlertOverlay` modules.
- `HAS_LVGL_UI` define gates the new path; legacy `Display::main()` stays for non-V6 boards.
- Home grid renders as LVGL; tapping any tile enters the corresponding existing menu, also rendered as LVGL.
- All 191 menu items reachable by the same physical-button sequence as before.

**Exit criteria:**
- Cold-boot to home in ≤2s on V6.2.
- All four physical buttons behave identically to legacy build for a 30-tile traversal test.
- No regression in WiFi/BT scan modes (verify deauth detect, packet count, beacon spam still work — they don't depend on UI).
- Flash size delta ≤ +180 KB (LVGL core + widgets we use).

**Risk:** flash-size budget. LVGL is ~150 KB stripped; if we go over budget, drop animations and the unused widget set first.

### 10.2 Phase 2 — Watchtower event pipeline + SD logging

**Goal:** capture every nearby RF observation as a `wt_event_t`, ring-buffer it, write sector-aligned to SD. No classification yet — Tier field is always 0 (LOW).

**Deliverables:**
- `Watchtower` coordinator with ACTIVE/PAUSED/OFF state machine.
- `WiFiScan::registerWatchtowerCallback()` hook delivering RX frames.
- BLE advertisement subscription path (BLE callback already exists in upstream).
- 512-event SRAM ring buffer (PSRAM-preferred).
- `WatchtowerLog` writer producing `/wt/YYYYMMDD_HHMMSS.wtl` files with valid header/footer.
- Synthesis screen LIST tab renders live events from the ring (no filtering, just newest-first).
- Pause-on-attack hookup per §3.2 (deauth, beacon spam, evil portal pause; observe-only modes coexist).

**Exit criteria:**
- 30-minute walking session captures ≥500 events without dropped frames.
- Log files round-trip: written on device, copied off SD, parsed by a small Python script that confirms event counts and CRC.
- Watchtower correctly pauses when a deauth attack starts and resumes (with gap event recorded) when it ends.
- Free heap remains ≥30 KB throughout the session (no slow leak).

**Risk:** PSRAM allocation flakiness on V6.2. Some V6.2 units have intermittent PSRAM init issues; fall back to internal SRAM with reduced ring size (128 events) if PSRAM init fails.

### 10.3 Phase 3 — Tier 1 classification rules

**Goal:** the 12 starter rules from §6 fire correctly, set `event.tier`, and populate `rule_bitmap`. Alert output channels (NeoPixel, banner, modal) work per §3.3.

**Deliverables:**
- `WatchtowerRules` module with all 12 rules from §6.
- `WatchtowerOUIList.h` and `WatchtowerPauseConditions.h` populated.
- LVGL alert strip on Synthesis bottom shows highest-tier unacknowledged event.
- CRITICAL modal overlay actually displays and accepts dismissal.
- NeoPixel patterns implemented per §3.3 table.

**Exit criteria:**
- Synthetic test harness on a desk: another ESP32 transmits known patterns; Watchtower fires R1 (deauth), R3 (Karma), R8 (hidden SSID) reliably (≥95% of injected attacks detected within 10s).
- 30-minute coffee-shop walk produces ≤3 false-positive HIGH-tier alerts. CRITICAL false positives = 0 in this test.
- Reason strings render correctly in the Detail tab for at least one example of every rule.

**Risk:** rule false-positive rate too high in the wild. Mitigation: every rule has individual snooze; user can mute per-MAC; we measure FP rate during exit-criteria testing and tune thresholds before Phase 4.

### 10.4 Phase 4 — Tier 2 baselines + Tier 3 fingerprints

**Goal:** location baselines and device fingerprints are built and consulted on every event. Reason chain in Detail tab shows up to three lines per §7.2.

**Deliverables:**
- `WatchtowerBaselines` module: H3 cell aggregation, 30 min × 2 visit establishment rule, persistent `/wt/baselines.db`.
- `WatchtowerFingerprints` module: 1024-entry LRU, CSI-signature compression to 32 bytes, confidence scoring.
- `WatchtowerCSI` module: ESP-CSI subscription, per-packet signature extraction.
- Detail tab renders the three-line reason chain with confidence labels per §7.3.

**Exit criteria:**
- After a week of normal use at home/work, baselines for those two cells are "established" and Tier 2 z-scores are present on events captured there.
- Fingerprint LRU has ≥50 entries with ≥3 observations each after one week.
- A controlled test: device A advertises with random MACs every 30s for 5 min; Tier 3 confidence remains ≥60% across all rotations (proving R9-style re-identification works).
- Baselines and fingerprints survive a power cycle (persistence verified).

**Risk:** CSI extraction interferes with normal packet RX or destabilizes the WiFi stack. Mitigation: CSI is gated behind `HAS_CSI` and can be disabled at runtime if instability is observed; the fingerprint LRU degrades gracefully (uses only probe-pattern + timing features when CSI unavailable).

### 10.5 Phase 5 — Spatial inference (RSSI gradient + Locate mode)

**Goal:** the MAP tab actually shows useful spatial information. Locate mode works end-to-end. R4 (persistent tail) gets dramatically stronger via gradient corroboration.

**Deliverables:**
- `WatchtowerSpatial` module: per-MAC sliding window of (gps_point, rssi) pairs, gradient computation, bearing estimate with motion-freshness decay.
- MAP tab renders user GPS path, distance rings (5 / 25 / 100 m), bearing arrows for tracked devices.
- Locate mode reachable from Detail tab; channel-fixes the radio, increases sample rate, shows large RSSI dial + bearing arrow + range ring.
- R4 (persistent tail) consults the gradient module — if the target's RSSI stays roughly constant while the user has been moving, the tail score increases.

**Exit criteria:**
- Walk past a stationary BLE device while it advertises continuously: MAP tab shows it at approximately the right GPS position with ≤30 m drift after 60 s of walking.
- Walk with a friend carrying a paired BLE device: R4 fires within 30 minutes with the gradient corroboration line in the reason chain.
- Locate mode on a known-position device produces a "warmer / colder" trend that correctly indicates direction over a 30 s walk.

**Risk:** GPS reliability indoors. Mitigation: bearings are only computed and displayed when GPS fix quality ≥2D and the user's GPS track has ≥3 m of motion variance; otherwise the UI shows distance bands only, no bearing.

### 10.6 Phase 6 — UI polish, Triage mode, alerts tuning

**Goal:** ship-ready UI. Triage mode for past sessions works. Alert tier behavior tuned with real-world data from Phases 3–5.

**Deliverables:**
- Triage mode (per §4.6): reuses Synthesis layout, replaces alert strip with timeline scrubber, replaces SETTINGS with EXPORT.
- Settings tab fully functional: tier snooze (5/15/60 min), per-MAC mute list management, baseline reset, current rule-set version, log file size, export-session command.
- Alert thresholds tuned based on FP-rate measurements from earlier phases.
- Help text / inline descriptions on Settings (matches the description-rendering work already merged on this branch).
- README and CHANGELOG entries for the LVGL+Watchtower release.

**Exit criteria:**
- One week of dogfood use without a crash or a stuck-UI report.
- A stranger user can pick up the device, find Watchtower, see the alert chain on a flagged event, and dismiss it without instruction.
- All upstream Marauder scan/attack modes still work identically to legacy build (regression test).
- Final flash size delta documented and within budget.

**Risk:** scope creep. Phase 6 is a polish phase, not a new-feature phase. New rules, new tabs, new attack modes belong to a future cycle.

## 11. Testing Strategy and Risks

### 11.1 Per-phase verification on real V6.2 hardware

Every phase ends with a hardware-test gate. The gate is a checklist run on a real V6.2 unit with GPS antenna attached and SD card mounted. Before the gate is signed off, the build is not merged and the next phase does not start.

The reusable test environments are:

- **Bench harness:** another ESP32 (any dev board) running an open-source attack/scanner script that emits known patterns: deauth flood, beacon spam, fake Karma responses, MAC randomization probes. Lets us inject controlled inputs and verify rule firing.
- **Coffee-shop walk:** a 30-minute outdoor + indoor route through known WiFi/BLE-dense areas. Used for FP-rate measurement and baseline establishment.
- **Two-device tail test:** a friend carries a paired BLE device while the user walks the same route — the only reliable way to validate R4 and the RSSI-gradient corroboration end-to-end.
- **Power-cycle persistence test:** simple loop — run for an hour, hard power-cycle, confirm baselines and fingerprints survived.

### 11.2 Regression matrix

Every phase's gate also re-runs a fixed regression matrix to guarantee upstream Marauder modes still work:

| Subsystem | Regression test |
|---|---|
| WiFi scan | "AP Scan" reports ≥10 APs in a known-dense area |
| BLE scan | "BLE Scan" reports ≥5 advertisers in a known-dense area |
| Deauth | Targeted deauth on bench harness's AP succeeds |
| Beacon spam | Fake-AP beacon spam shows in another scanner |
| Evil portal | Captive portal serves the index page over device AP |
| Flock detection | Existing Flock OUI detection still flags injected Flock-pattern frames |
| GPS | Cold lock ≤120s outdoors, fix shown in status bar |
| SD | Wardrive logs still write successfully |
| Battery | Voltage reading and charging-icon behavior unchanged |

A regression failure blocks the gate even if the Watchtower-specific exit criteria pass.

### 11.3 Risk register

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R-01** | PSRAM init flaky on some V6.2 units (known issue with some batches) | Medium | Medium | Detect at boot; fall back to internal SRAM with reduced ring-buffer size (128 events). Surface "PSRAM unavailable" in Settings tab. |
| **R-02** | Flash size budget exceeded by LVGL + Watchtower | Medium | High | Drop LVGL animation module first (~25 KB). Drop unused widgets (canvas, calendar, msgbox we don't use). If still over, gate non-essential tabs (Settings detail) behind `HAS_FLASH_HEADROOM`. |
| **R-03** | Rebase against fast-moving upstream produces large conflicts | High | Medium | Coupling concentrated in 6 well-known files (§9.3); all edits are additive. Rebase frequently rather than letting drift accumulate. |
| **R-04** | Tier 1 false-positive rate too high in real-world use | Medium | High | Per-tier snooze; per-MAC mute; rule thresholds are tuned at Phase 3 gate against measured FP rate; CRITICAL FP rate must be 0 in coffee-shop test before phase passes. |
| **R-05** | GPS unreliable indoors degrades MAP tab and R4 (tail rule) | High | Low (because we plan for it) | Bearings only displayed when GPS fix quality ≥2D and user has motion variance; UI gracefully degrades to distance bands only. |
| **R-06** | CSI extraction destabilizes WiFi stack on some firmware revs | Medium | Medium | `HAS_CSI` define gates the feature; runtime kill switch disables CSI subscription; Tier 3 fingerprinting degrades to probe-pattern + timing only. |
| **R-07** | Persistent files (`/wt/baselines.db`, `/wt/fingerprints.db`) corrupt on unclean shutdown | Medium | Medium | Atomic write via tmp-file + rename. Header CRC validates on read; corrupt file is renamed `.bad` and a fresh empty file is created (user loses learned baselines but device still runs). |
| **R-08** | Single-antenna spatial inference oversold in UI | Low | High (trust) | §8.5 rules are firm: no fake bearings, no false-precision distance, no AoA-style indicators. Code-review checklist enforces these before Phase 5 gate. |
| **R-09** | Watchtower captures interfere with active attacks (radio contention) | Medium | High | Pause-on-attack list in `WatchtowerPauseConditions.h` reviewed each release. Gap events recorded so triage shows the gap honestly. Coexistence with passive scans validated at Phase 2 gate. |
| **R-10** | User confusion between Locate mode (active) and normal Watchtower (passive) | Low | Low | Locate mode shows distinct full-screen UI (large RSSI dial); status bar mode indicator changes to "LOCATE"; Locate ends automatically after 5 minutes of no motion. |

### 11.4 Definition of done (release criteria)

Watchtower v1 ships when:

- All six phases have passed their hardware-test gates.
- Regression matrix passes on V6.2 (the primary target) and at least one V6.1 unit.
- One week of dogfood use without a crash, a stuck UI, or a corrupted log file.
- README updated with Watchtower documentation; CHANGELOG entry written.
- Rebase against the latest upstream `justcallmekoko/ESP32Marauder` master at release time produces a clean build with all regression tests passing.
- This spec is updated with any deviations from plan (decisions that changed during implementation), so it remains the source of truth for the next cycle.
