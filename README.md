# Jacuzzi-Bathtab-Controller

## Build

### Debug (with serial debug output)
```
pio run -e esp12e
pio run -e esp12e -t upload
```

### Production (no serial debug, smaller footprint)
```
pio run -e esp12e-prod
pio run -e esp12e-prod -t upload
```

**RAM:** ~29KB / 80KB (36%) | **Flash:** ~306KB / 1020KB (29%)
