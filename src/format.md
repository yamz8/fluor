# The Fluor panel format

A panel is one JSON file, usually ending in `.fluor`:

```json
{
  "fluor": 1,
  "title": "T cell panel",
  "lasers": [
    { "nm": 405, "filters": ["450/50", "525/50", "610/20"] },
    { "nm": 488, "filters": ["530/30", "695/40"] }
  ],
  "dyes": [
    { "name": "BV421", "laser": 405, "filter": "450/50" },
    { "name": "FITC", "laser": 488, "filter": "530/30" },
    { "name": "My dye", "ex": 500, "em": 540, "brightness": 3 }
  ]
}
```

- `lasers` is the instrument: each laser's wavelength in nm and the bandpass
  filters of the detectors behind it, written center/width (`530/30` passes
  515 to 545 nm). Wavelengths run from 300 to 900 nm.
- `dyes` is the panel. `name` is a dye from `fluor dyes`, matched without
  regard to case. A dye Fluor doesn't know needs `ex` and `em` peaks in nm
  and an optional `brightness` from 1 (dim) to 5 (very bright, default 3).
- `laser` and `filter` put a dye on a detector, and must name one of the
  instrument's. Leave them out and `fluor assign` places the dye on the best
  free detector.
- Each dye appears once, and two dyes on one detector is an error.

Spectra are modeled from each dye's peaks, not measured: they are right about
which dyes collide and roughly how badly, and should be confirmed with single
stain controls before a panel is run.
