# Laser Tag WebSocket Protocol Specification

**Version:** 2.2  
**Transport:** WebSocket (TCP)  
**Format:** JSON  
**Architecture:** Attribute-Driven (ESP32 is stateless regarding "Game Modes")

## 1. Overview & Conventions

To ensure reliability between the Browser (Client) and the ESP32 (Server), this protocol enforces the following conventions:

1.  **OpCodes:** Every message includes an integer `op` code for efficient C `switch` statements.
2.  **Attribute-Driven Logic:** The ESP32 does not know about "Game Modes" (e.g., Deathmatch). It only holds attributes. The Browser configures the game by setting attributes like `max_hearts`, `max_ammo`, `game_duration_s`, etc.
3.  **Defaults & Infinite:**
    - On boot, the ESP32 loads default values.
    - Numeric values of `-1` denote **Infinity** (e.g., Infinite Ammo, Infinite Health).
4.  **Reliability & Deduplication:**
    - Control messages include a `req_id`. The server echoes this back in an `ack` message.
    - Critical events (Shots/Hits) include a rolling `seq_id` (0-255) to prevent double-counting if packets are re-sent.
    - Events include `timestamp_ms` (device uptime) to resolve timing conflicts (e.g., mutual kills).
5.  **Strict Typing:** IDs are `uint8_t`. Colors are `uint32_t`. Time is in seconds (or ms where specified).

---

## 2. OpCode Registry

**Client (Browser) → ESP32**
| OpCode | Type String | Description |
| :--- | :--- | :--- |
| `1` | `get_status` | Request an immediate status update |
| `2` | `heartbeat` | Keep-alive ping from browser |
| `3` | `config_update` | Change settings (Identity, Colors, Rules, Audio) |
| `4` | `game_command` | Start/Stop/Reset the loop |
| `5` | `hit_forward` | Debug: Simulate a hit on this device |
| `6` | `kill_confirmed` | Admin: Confirm a kill manually |
| `7` | `remote_sound` | Admin: Force device to play a specific sound |

**ESP32 → Client (Browser)**
| OpCode | Type String | Description |
| :--- | :--- | :--- |
| `10` | `status` | Full device state report (Config + Live Stats) |
| `11` | `heartbeat_ack` | Lightweight pong with signal strength |
| `12` | `shot_fired` | Triggered when trigger is pulled |
| `13` | `hit_report` | Triggered when IR sensor receives a hit |
| `14` | `respawn` | Triggered when player respawns |
| `15` | `reload_event` | Triggered when player reloads |
| `16` | `game_over` | Auto-triggered when `game_duration_s` expires |
| `20` | `ack` | Generic confirmation of a command |

---

## 3. Message Definitions: Client → ESP32

### 3.1 Base Message Structure

```typescript
interface ClientMessage {
  op: number;
  type: string;
  req_id?: string; // Optional UUID
}
```

### 3.2 System Messages

**Get Status (Op 1)**

```json
{ "op": 1, "type": "get_status" }
```

**Heartbeat (Op 2)**

```json
{ "op": 2, "type": "heartbeat" }
```

### 3.3 Configuration (The "Game Mode" Logic)

**Config Update (Op 3)**
Sets the rules of the game. All fields are optional.

```json
{
  "op": 3,
  "type": "config_update",
  "req_id": "cfg-101",

  "reset_to_defaults": false, // If true, reset to factory before applying changes

  // Identity
  "device_id": 1,
  "player_id": 5,
  "team_id": 2, // 0=Solo/FFA, 255=Admin, 1..N=Teams

  // Visuals & Hardware
  "color_rgb": 16711680,
  "volume": 80, // 0-100
  "sound_profile": 0, // 0=SciFi, 1=Realistic, 2=Silenced
  "haptic_enabled": true,

  // Health Mechanics
  "enable_hearts": true,
  "spawn_hearts": 3,
  "max_hearts": 5, // -1 for Infinite
  "respawn_time_s": 10,
  "damage_in": 1,
  "friendly_fire": false,

  // Ammo Mechanics
  "max_ammo": 30, // -1 for Infinite
  "reload_time_ms": 2500,
  "damage_out": 1,

  // Game Timer (Safety)
  "game_duration_s": 600 // Auto-STOP after 10 mins. 0 = Manual Stop only.
}
```

### 3.4 Game Commands

**Game Command (Op 4)**
Controls the state of the game loop.

**Command Enum:**
| Value | Name | Description |
| :--- | :--- | :--- |
| `0` | `STOP` | Disable firing, lights off, reset timer |
| `1` | `START` | Enable firing, sensors, start game timer |
| `2` | `RESET` | Reset Hearts/Ammo to Max, Kills/Deaths to 0 |
| `3` | `PAUSE` | Disable firing, pause timer, keep stats |
| `4` | `UNPAUSE` | Continue current game |

```json
{
  "op": 4,
  "type": "game_command",
  "req_id": "cmd-882",
  "command": 1
}
```

### 3.5 Debug/Action Messages

**Hit Forward (Op 5)**
Simulate incoming damage (Admin/Debug).

```json
{ "op": 5, "type": "hit_forward", "shooter_id": 10 }
```

**Remote Sound (Op 7)**
Force the speaker to play a specific sound asset.
_Sound IDs:_ 0=Whistle, 1=Horn, 2=Siren, 3=Voice"Game Over"

```json
{ "op": 7, "type": "remote_sound", "sound_id": 0 }
```

---

## 4. Message Definitions: ESP32 → Client

### 4.1 Device Status (Op 10)

```json
{
  "op": 10,
  "type": "status",
  "uptime_ms": 154000,

  // Current Configuration
  "config": {
    "device_id": 1,
    "player_id": 5,
    "team_id": 2,
    "color_rgb": 16711680,
    "max_hearts": 5,
    "spawn_hearts": 3,
    "max_ammo": 30,
    "game_duration_s": 600,
    "friendly_fire": false
  },

  // Live Stats
  "stats": {
    "shots": 120,
    "enemy_kills": 2,
    "friendly_kills": 1,
    "deaths": 1
  },

  // Live State
  "state": {
    "current_hearts": 3,
    "current_ammo": 15,
    "is_respawning": false,
    "is_reloading": false,
    "remaining_time_s": 446 // If game_duration_s > 0
  }
}
```

### 4.2 Heartbeat Ack (Op 11)

Includes RSSI to detect players leaving WiFi range.

```json
{
  "op": 11,
  "type": "heartbeat_ack",
  "batt_voltage": 3.7,
  "rssi": -55 // dBm
}
```

### 4.3 Events

**Shot Fired (Op 12)**

```json
{
  "op": 12,
  "type": "shot_fired",
  "timestamp_ms": 154020,
  "seq_id": 45
}
```

**Hit Report (Op 13)**

```json
{
  "op": 13,
  "type": "hit_report",
  "timestamp_ms": 154050,
  "seq_id": 46,
  "shooter_id": 4,
  "damage": 1,
  "fatal": false // True if this hit killed the player
}
```

**Respawn (Op 14)**

```json
{ "op": 14, "type": "respawn", "timestamp_ms": 164000 }
```

**Reload Event (Op 15)**

```json
{ "op": 15, "type": "reload_event", "current_ammo": 30 }
```

**Game Over (Op 16)**
Sent when `game_duration_s` timer hits 0.

```json
{ "op": 16, "type": "game_over" }
```

### 4.4 Acknowledgment (Op 20)

```json
{
  "op": 20,
  "type": "ack",
  "reply_to": "cfg-101",
  "success": true
}
```

---

## 5. TypeScript Definitions

```typescript
export enum OpCode {
  GET_STATUS = 1,
  HEARTBEAT = 2,
  CONFIG_UPDATE = 3,
  GAME_COMMAND = 4,
  HIT_FORWARD = 5,
  KILL_CONFIRMED = 6,
  REMOTE_SOUND = 7,

  STATUS = 10,
  HEARTBEAT_ACK = 11,
  SHOT_FIRED = 12,
  HIT_REPORT = 13,
  RESPAWN = 14,
  RELOAD_EVENT = 15,
  GAME_OVER = 16,
  ACK = 20,
}

export interface ConfigUpdateMessage {
  op: OpCode.CONFIG_UPDATE;
  type: "config_update";
  req_id?: string;
  reset_to_defaults?: boolean;

  // Identity
  device_id?: number;
  player_id?: number;
  team_id?: number;

  // Hardware / AV
  color_rgb?: number;
  ir_power?: number;
  volume?: number;
  sound_profile?: number;
  haptic_enabled?: boolean;

  // Rules
  spawn_hearts?: number;
  max_hearts?: number; // -1 Infinite
  respawn_time_s?: number;
  damage_in?: number;
  damage_out?: number;
  friendly_fire?: boolean;

  // Ammo & Time
  max_ammo?: number; // -1 Infinite
  reload_time_ms?: number;
  game_duration_s?: number; // 0 = Manual Stop
}
```

---

## 6. C Implementation Notes (ESP32)

**Handling Config Update with Safety Logic:**

```cpp
void handleConfigUpdate(JsonObject doc) {
    // 1. Optional Full Reset
    if (doc["reset_to_defaults"] == true) {
        loadDefaultConfig();
    }

    // 2. Identity & HW
    if (doc.containsKey("team_id")) config.teamId = doc["team_id"];
    if (doc.containsKey("color_rgb")) config.color = doc["color_rgb"];
    if (doc.containsKey("volume")) setVolume(doc["volume"]);

    // 3. Gameplay Mechanics & Safety Clamping
    if (doc.containsKey("max_hearts")) {
        config.maxHearts = doc["max_hearts"];
        // If live and max is lowered below current, clamp it.
        // Do NOT auto-heal if max is raised (wait for Respawn or Reset).
        if (gameState.currentHearts > config.maxHearts && config.maxHearts != -1) {
            gameState.currentHearts = config.maxHearts;
        }
    }

    if (doc.containsKey("max_ammo")) config.maxAmmo = doc["max_ammo"];

    // 4. Game Timer Logic ("Zombie" Prevention)
    if (doc.containsKey("game_duration_s")) {
        config.gameDuration = doc["game_duration_s"];
        // If game is currently running, calculate new end time
        if (gameState.isRunning && config.gameDuration > 0) {
            gameState.endTime = millis() + (config.gameDuration * 1000);
        } else if (config.gameDuration == 0) {
            gameState.endTime = 0; // Disable timer
        }
    }

    saveConfigToNVS();
    broadcastStatus();
}
```
