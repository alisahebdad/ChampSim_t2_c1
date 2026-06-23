# Technical Report: The `extra_cycle` Mechanism

**Project:** ChampSim_t2_c1
**Scope:** Custom replacement-policy hook that adds variable extra latency to a cache
based on runtime flow.
**Date:** 2026-06-15

---

## 1. Purpose

The goal of `extra_cycle` is to let a **replacement policy** inject a *variable* number of
extra clock cycles into a cache's access latency, computed at runtime from the access
pattern (typically the "reuse distance"/position movement of the touched way). Instead of a
fixed `HIT_LATENCY`, the effective tag-check latency becomes:

```
effective_latency = HIT_LATENCY + clock_period * impl_extra_cycle()
```

This models the idea that some replacement organizations (RT-LRU and its variants) cost more
cycles to resolve when the accessed line is "far" from its expected position.

---

## 2. Architecture

The feature is wired through ChampSim's module system in three layers.

### 2.1 Detection trait — `inc/modules.h:169-176`

A SFINAE trait detects whether a replacement policy provides an `extra_cycle()` method:

```cpp
template <typename T,typename... Args>
static auto extra_cycle_member_impl(int)
    -> decltype(std::declval<T>().extra_cycle(std::declval<Args>()...), std::true_type{});
template <typename,typename...>
static auto extra_cycle_member_impl(long) -> std::false_type;

template <typename T,typename... Args>
constexpr static bool has_extra_cycle =
    decltype(extra_cycle_member_impl<T, Args...>(0))::value;
```

This makes the hook **optional** — policies without `extra_cycle()` (e.g. `lru`) contribute 0.

### 2.2 Virtual interface — `inc/cache.h`

- `replacement_module_concept::impl_extra_cycle()` — pure virtual (`cache.h:254`)
- `replacement_module_model<Rs...>::impl_extra_cycle()` — `final` override (`cache.h:299`)
- `CACHE::impl_extra_cycle() const` — public cache-level accessor (`cache.h:324`)

### 2.3 Template dispatch — `inc/cache.h:447-461`

```cpp
template <typename... Rs>
long CACHE::replacement_module_model<Rs...>::impl_extra_cycle(){
  using return_type = long;
  auto process_one = [&](auto &r){
    using namespace champsim::modules;
    if constexpr (replacement::has_extra_cycle<decltype(r)>)
      return r.extra_cycle();
    return return_type{};
  };
  if constexpr (sizeof...(Rs) > 0)
    return std::apply([&](auto&... r) { return (..., process_one(r)); }, intern_);
  return return_type{};
}
```

For each configured replacement policy `r`, if it has `extra_cycle()`, the policy's value is
used; otherwise 0. The cache-level wrapper (`src/cache.cc:864`) simply forwards to the pimpl.

---

## 3. Runtime flow (where the cycle is actually added)

The **only live** injection point is in tag-check initiation:

`src/cache.cc:396-404` — `initiate_tag_check()`:

```cpp
return [time = current_time + (warmup ? 0 : HIT_LATENCY), ul, this](const auto& entry) {
  CACHE::tag_lookup_type retval{entry};
  if (!warmup)
    retval.event_cycle = time + champsim::chrono::clock::duration{this->clock_period*impl_extra_cycle()};
  else
    retval.event_cycle = time;
  ...
```

The tag check's `event_cycle` is the moment its result becomes available. It is gated by
`is_ready` (`src/cache.cc:435-436`):

```cpp
auto is_ready = [time = current_time](const auto& entry) { return entry.event_cycle <= time; };
```

In `operate()` (`src/cache.cc:524-531`), only ready entries are passed to `try_hit` /
`handle_miss`. So adding `clock_period * impl_extra_cycle()` to `event_cycle` genuinely
**delays hit/miss resolution** for that access by that many cycles — for both hits and misses
(the delay is on the tag-check phase, which precedes both paths). Dimensionally this is
consistent with the existing `HIT_LATENCY = hit_latency * clock_period` convention.

### Value production

The returned value is a member variable (`extra_cycle_` / `extra_cycle_w`) that each policy
recomputes inside `update_replacement_state()` (and in some variants `find_victim` /
`cal_avg`). Example — `replacement/rtlru/rtlru.cc:47-65`:

```cpp
long distance = rt_position[set] - way;
rt_position[set] = way;
extra_cycle_ = std::abs(distance);   // reuse-distance magnitude
```

`extra_cycle()` (`rtlru.cc:83-86`) just returns the last computed `extra_cycle_`.

### Other call sites are disabled

Every other use is **commented out**, so fills/writes are *not* penalized:

- `src/cache.cc:387` — fill latency in `handle_write` → commented
- `src/cache.cc:648` — fill latency in `handle_fill` → commented
- `src/cache.cc:471` — tag-check bandwidth term → commented

So the mechanism currently affects **only the tag-check (hit-detection) latency**, not fill
latency and not tag-check bandwidth.

---

## 4. Assessment — does it "add extra cycle based on runtime flow"?

**Yes, mechanically it works:** the value is computed at runtime from the access pattern and
does delay tag-check completion by that many cycles. The plumbing (trait → virtual → template
fold → `event_cycle`) is sound and dimensionally correct. With `lru` (no `extra_cycle()`) the
contribution is 0, so the baseline is unaffected.

However, there are several **correctness/modeling concerns** worth flagging.

### 4.1 Causality / staleness (most important)

`impl_extra_cycle()` is read in `initiate_tag_check`, **before** this access's own
`update_replacement_state()` runs (that happens later, inside `try_hit` once the entry is
ready — `src/cache.cc:276`). Therefore the penalty applied to access *N* is the
`extra_cycle_` value left over from **the most recent access that updated it** — generally a
*different* access, often a *different set and address*. The latency charged to an access is
not derived from that access's own reuse distance.

### 4.2 Single global scalar, not per-set / per-access

`extra_cycle_` is one scalar shared across all sets and all concurrent in-flight tag checks of
the cache. Multiple outstanding accesses to different sets all read the same global value.
There is no association between the penalty and the specific line/set being accessed.

### 4.3 Updated only on hits

`update_replacement_state()` is invoked only from `try_hit` (`src/cache.cc:276`); misses do
not refresh `extra_cycle_` (`find_victim` in `rtlru` does not set it). So the global value is
"reuse distance of the last cache **hit**," applied to whatever access enters tag-check next.

### 4.4 Signed values in some variants → negative latency

`rtlru`, `flru`, `pairrtlru`, `avgrtlru*` use `std::abs(...)`, but **`modertlru`
(`modertlru.cc:81`) and `deltartlru` (`deltartlru.cc:96`)** assign a possibly-signed
`distance` (the inner term is abs'd in the current code, but the stored value is not clamped).
If any path yields a negative value, `event_cycle` would be set *earlier* than
`current_time + HIT_LATENCY`, i.e. a negative latency contribution. Values should be clamped
to `>= 0`.

### 4.5 Multi-policy fold returns only the last policy

In `impl_extra_cycle` the fold `(..., process_one(r))` uses the comma operator, so with more
than one replacement policy configured only the **last** policy's value is returned (earlier
ones are discarded). Fine for the single-policy configurations used here, but surprising if
ever combined.

### 4.6 Minor: dead statement in `process_one`

When `has_extra_cycle` is true, `return r.extra_cycle();` always returns, leaving the trailing
`return return_type{};` as dead code. Harmless, but it relies on the early return; worth a
comment.

### 4.7 Stats side-effect

Each policy also accumulates `hit_cycle[type]` / `miss_cycle[type]` (e.g. `rtlru.cc:57-60`)
and prints them in `replacement_final_stats`. These count the *computed* distances, not the
cycles actually charged to the pipeline (which, per 4.1, are the previous access's value), so
the stats and the simulated penalty can diverge.

---

## 5. Recommendations

1. **Fix causality:** if the intent is "this access pays for its own reuse distance," compute
   the penalty in `initiate_tag_check` from current state, or move the `event_cycle`
   adjustment to *after* `update_replacement_state` (e.g. carry the value on the
   `tag_lookup_type` and apply it where the result is finalized).
2. **Make the value per-access/per-set**, or at minimum document that it is a global "last
   hit" approximation.
3. **Clamp to non-negative**: `extra_cycle_ = std::max(0L, value);` in `modertlru` /
   `deltartlru`.
4. **Decide on fills/writes:** either keep them excluded (document it) or re-enable the
   commented sites at `cache.cc:387,648` consistently.
5. If multiple policies are ever stacked, replace the comma-fold with an explicit reduction
   (sum or max) instead of "last wins."
6. Reconcile `hit_cycle`/`miss_cycle` stats with the value actually charged to the pipeline.

---

## 6. File index

| Concern | Location |
|---|---|
| Detection trait | `inc/modules.h:169-176` |
| Virtual interface | `inc/cache.h:254`, `:299`, `:324` |
| Template dispatch | `inc/cache.h:447-461` |
| Cache-level forwarder | `src/cache.cc:864-866` |
| **Live injection point** | `src/cache.cc:402` (`initiate_tag_check`) |
| Readiness gate | `src/cache.cc:435-436` |
| Disabled sites | `src/cache.cc:387`, `:471`, `:648` |
| Value computation (example) | `replacement/rtlru/rtlru.cc:47-86` |
| Signed-value variants | `replacement/modertlru/modertlru.cc:81`, `replacement/deltartlru/deltartlru.cc:96` |
