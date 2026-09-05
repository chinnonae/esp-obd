# D13 - Bundle Honda and Honda-Civic OBDb Profiles

**Status:** Done

## Goal

Bundle Honda and Honda-Civic manufacturer-specific OBDb signalsets into the
dashboard as built-in profiles, so users can immediately access Honda/Civic
vehicle diagnostics without needing to upload profiles manually.

## Background

The dashboard currently ships with the generic SAEJ1979 profile by default.
Honda and Honda-Civic profiles (sourced from OBDb) provide Mode 22/23 (UDS)
commands for accessing battery diagnostics, engine parameters, transmission data,
climate control, and other Honda-specific signals across battery management,
powertrain, thermal systems, and electrical subsystems.

These profiles are valuable for:
- EV/Hybrid diagnostics (battery monitoring, SOC, cell voltages, temperatures)
- Engine and transmission monitoring
- Climate control systems
- Brake and tire monitoring

## Scope

1. **Download profiles:**
   - Fetch `default.json` from
     `https://github.com/OBDb/Honda/blob/main/signalsets/v3/default.json`
   - Fetch `default.json` from
     `https://github.com/OBDb/Honda-Civic/blob/main/signalsets/v3/default.json`
   - Store in `dashboard/data/`:
     - `dashboard/data/honda.json`
     - `dashboard/data/honda-civic.json`

2. **Attribution:**
   - Create `dashboard/data/honda.ATTRIBUTION.md` and
     `dashboard/data/honda-civic.ATTRIBUTION.md`
   - Reference source: `github.com/OBDb/Honda`, `github.com/OBDb/Honda-Civic`
   - License: CC BY-SA 4.0 (same as SAEJ1979)

3. **Config store update (`dashboard/js/config-store.js`):**
   - Extend the seeding logic to include Honda and Honda-Civic as built-in profiles
   - Add to initial store:
     ```javascript
     [BUILTIN_ID_HONDA]: {
       name: "Honda (OBDb)",
       isBuiltin: true,
       data: await fetch(url).json()
     },
     [BUILTIN_ID_CIVIC]: {
       name: "Honda Civic (OBDb)",
       isBuiltin: true,
       data: await fetch(url).json()
     }
     ```
   - Profiles are not deletable (like SAEJ1979)

4. **UI updates:**
   - Config tab now shows three built-in profiles
   - Profile selector displays all three (SAEJ1979, Honda, Honda-Civic)

5. **Documentation:**
   - Update `dashboard/README.md` to mention bundled profiles
   - Update [DESIGN_DECISIONS.md](../DESIGN_DECISIONS.md) if needed

## Acceptance Criteria

- [ ] `honda.json` and `honda-civic.json` are present in `dashboard/data/`
- [ ] Attribution markdown files are created with source and license
- [ ] Built-in profiles are seeded on first load alongside SAEJ1979
- [ ] User can select Honda or Honda-Civic from profile dropdown
- [ ] Switching to Honda/Civic profile shows appropriate signals
- [ ] Poll engine and decoder work correctly with Honda/Civic commands
- [ ] Built-in profiles cannot be deleted (matching SAEJ1979 behavior)
- [ ] All existing tests pass (no regression)

## Test Plan

1. **Manual - First load:**
   - Clear localStorage
   - Load dashboard
   - Verify three built-in profiles exist in config tab
   - Verify each profile name is correct

2. **Manual - Profile switching:**
   - Switch to Honda profile
   - Verify battery/engine/transmission signals appear in Current tab
   - Switch to Honda-Civic profile
   - Verify different set of Civic-specific signals
   - Verify console shows correct command headers/RAX values

3. **Manual - Polling:**
   - Connect to simulator
   - Select Honda profile
   - Verify Mode 22 commands are queued and polled
   - Check console for any NO DATA responses
   - Verify custom discovery (D12) works with Honda profile

## Dependencies

- D02 (Config store) - profile management layer
- D05 (Poll engine) - polling Mode 22/23 works
- D12 (Custom PID discovery) - beneficial for handling HC-specific NO DATA responses

## File Locations

| File | Size (approx) | Type |
|------|---------------|------|
| `dashboard/data/honda.json` | ~400KB | OBDb v3 signalset |
| `dashboard/data/honda-civic.json` | ~350KB | OBDb v3 signalset |
| `dashboard/data/honda.ATTRIBUTION.md` | ~0.5KB | Attribution |
| `dashboard/data/honda-civic.ATTRIBUTION.md` | ~0.5KB | Attribution |

## Implementation Notes

### Profile IDs
- Use consistent ID format: `builtin:honda` and `builtin:civic`
- Display names: "Honda (OBDb)" and "Honda Civic (OBDb)" to indicate source

### Fetch Strategy
- Bundle files directly (minified) rather than downloading at runtime
- Total dashboard size increase: ~750KB (compresses well, ~100-150KB gzipped)
- Alternative: lazy-load on profile select (adds complexity, saves startup time)

### Testing with Simulator
- Simulator's `fakeDataForPid()` currently only supports Mode 01 PIDs
- May need to extend simulator to fake some Mode 22 responses
- Or accept that Honda/Civic profiles won't return data in simulator
  (acceptable for now; hardware testing validates)

## Future Enhancements (out of scope)

- Lazy-load Honda/Civic profiles (only fetch when selected)
- Add more OBDb profiles (Nissan-Leaf, Toyota-Prius, etc.)
- UI toggle to show/hide manufacturer-specific profiles
- Analytics: track which profiles are most commonly used
