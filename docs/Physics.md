# Blackwater.Physics — Collision & Movement

> Owner: *(friend)* · Target: `Blackwater.Physics` (static lib, not created yet) · Planned: M7

## Scope: collision, not a physics engine

Trinity needs characters that move, stop at walls, don't overlap each other, and get hit
by projectiles. It does **not** need rigid bodies, rotation, stacking, ragdolls or
vehicles. If that ever changes, we integrate **Jolt Physics** rather than write one.

## The big simplification: it's 2D

The game is 2.5D: everything walks on the ground. So collision runs in **2D on the XZ
plane** — circles and grid cells, no Y. Rendering stays 3D; physics never needs to know.

## What it needs to do

| Feature | Shape | Used for |
|---|---|---|
| Ray vs ground | ray / plane | Click to move — *exists already* in `Camera.h` (`IntersectGroundPlane`); move it here |
| Walkability grid | cells | Walls, obstacles; also the input to A\* pathfinding |
| Character vs world | circle vs grid | Slide along walls instead of stopping dead |
| Character vs character | circle vs circle | Push apart so units never overlap |
| Queries | ray, circle overlap | Projectile hits, "enemies within range", click on a unit |

## Rules from the engine

- **Runs only in `OnUpdate`, at the fixed 60 Hz step.** Deterministic: same inputs, same
  results, regardless of frame rate.
- **Never sees `alpha`.** Interpolation is drawing only; physics works on current state.
- Plain data in, plain data out: positions as floats, no D3D or engine types in the API.
- Unit-testable without a window — this module should have the most tests in the engine.

## API sketch (to agree before starting)

```cpp
struct Circle { float x, z, radius; };

class PhysicsWorld {
public:
    BodyId AddBody(Circle shape);
    void   SetDesiredVelocity(BodyId, float vx, float vz);
    void   Step(float dt);                       // resolve walls, then separation
    Circle GetBody(BodyId) const;

    bool   Raycast(Ray, RayHit&) const;
    void   OverlapCircle(Circle, std::vector<BodyId>& out) const;

    void   SetWalkable(int cellX, int cellZ, bool walkable);
};
```
