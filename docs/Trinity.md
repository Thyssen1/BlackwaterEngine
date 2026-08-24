# Trinity — Game Design

> *Trinity* (working name) is the game built on the **Blackwater** engine.
> Engine architecture lives in [../DESIGN.md](../DESIGN.md).
>
> **Rule:** Trinity links Blackwater. Blackwater never knows Trinity exists.

---

## 1. Premise

A **single-player 3D RPG** where you control a **party of three heroes**.

The lineage is **Warcraft III** and **World of Warcraft** — but single-player, and
built around a small fixed party rather than an army or a raid group. The feel to aim for
is *chill exploration*: roam a world, meet NPCs, take on quests, build up gear, and make
choices that shape the story.

Tone: **dark and grounded.** Not grimdark, but not heroic-bright either.

---

## 2. Camera & controls

| | |
|---|---|
| **Camera** | WC3-style: 3D, fixed-ish downward pitch, free rotate and zoom. Reads as isometric without being 2D |
| **Move** | **Right-click** the ground to move (Diablo / WC3 convention) |
| **Select** | Left-click a hero; click-drag to band-select; `Tab` to cycle |
| **Abilities** | **`1` `2` `3`** and **`Q` `W` `E` `D`** hotbar |
| **Target** | Right-click an enemy to attack; ground-targeted abilities use a cursor indicator |

Party control is the central interaction question: the two heroes you aren't driving need
sane follow-and-fight behaviour, or the game becomes micromanagement.

---

## 3. Systems

### Party of three
- Three distinct heroes with their own stats, abilities, and gear.
- Switch control freely; unselected heroes follow and fight via simple AI.
- Class identity matters — the trio should cover complementary roles.

### Combat
- Real-time with ability cooldowns (WC3/WoW flavoured, not turn-based).
- Auto-attack plus a small hotbar of active abilities per hero.
- Enemies with health, aggro, and basic ability use.

### Progression & gear
- Levels, stats, and ability unlocks per hero.
- Loot drops with rarity tiers; equipment slots that visibly change stats.
- Gear should feel like the primary reward loop for exploration.

### Story & choices
- **Branching narrative** — dialogue choices that change outcomes.
- Backed by a **flag/variable store**: dialogue and quests read and write named state.
  That single mechanism is enough to make choices persist and matter.
- NPCs are interactable world objects, not menus.

---

## 4. Content the engine must support

Everything here is **data**, authored in the editor, consumed by the runtime:

| Content | Editor | Engine milestone |
|---|---|---|
| Terrain & scenes | Terrain / Scene editor | M3 |
| Hero, NPC, enemy models + animations | (imported glTF) | M3–M4 |
| Entity & spawn placement | Entity placer | M6 |
| Abilities (cost, cooldown, effect) | Ability editor | M7 |
| Items, loot tables, affixes | Item editor | M8 |
| Dialogue trees + conditions | Dialogue editor | M9 |
| Quests, objectives, story flags | Quest editor | M9 |
| Triggers / scripted events | Trigger editor (→ Lua) | M6+ |

---

## 5. Open questions

- [ ] The three heroes: which classes/roles, and is the party fixed or recruited?
- [ ] Setting and world — what is this world, and why is it dark?
- [ ] World structure: one continuous map, or discrete zones?
- [ ] How much of WoW's mechanical vocabulary (threat, resources, GCD) to actually adopt
- [ ] Final name
