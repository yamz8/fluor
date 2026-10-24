---
name: fluor
description: |
  Plan fluorophore panels with Fluor: dyes over a cytometer's or microscope's
  lasers and filters, predicted spillover between detectors, and panel files.
  Use for ANY request to design, check, or change a flow cytometry or
  fluorescence panel, choose dyes or fluorochromes, or for any .fluor file.
---

# Fluor

A Fluor panel is one JSON `.fluor` file: the instrument (lasers and their
filters) and the dyes, each on a detector. You write the file with your
ordinary editing tools; the `fluor` command starts, places, and checks it. No
command needs a display.

Run `fluor help format` first. It prints the whole file format.

## Workflow

1. Start: `fluor new panel.fluor --preset vbyr --dye BV421 --dye FITC --dye PE`
   (`fluor presets` lists instruments; `fluor dyes 488` lists dyes near a
   wavelength, `fluor dyes cy7` by name).
2. Edit the JSON to add dyes or change the instrument, then run
   `fluor assign panel.fluor` to put new dyes on their best free detectors
   (`--all` places every dye again).
3. Check: `fluor check panel.fluor --json`. `spillover.matrix[a][b]` is dye
   a's signal in dye b's detector over its signal in its own. Issues come as
   errors (no detector, two dyes on one) and warnings (spill of 25% or more,
   lookalike signatures, a dye its laser barely excites), with notes from 10%.
4. Fix warnings by swapping a dye for one further away in the spectrum, or by
   pairing bright dyes with rarely expressed markers. Check again.

## Working with the person

- `fluor open panel.fluor` opens the planner for them; it needs their
  desktop, so do not wait on it.
- Spectra are modeled from peak wavelengths, so say the numbers are
  predictions to confirm with single-stain controls.
- Every command takes `--json`, exits 0 on success and 1 on failure, and
  writes errors to stderr. `fluor check` exits 1 when the panel has errors.
