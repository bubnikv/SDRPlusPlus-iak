# Legacy band ID conversion audit

Generated from `root/res/bandplans/*.json` using the current `classifyLegacyBand()` and `findLegacyBandMapping()` implementation.

- Legacy files: 21
- Legacy rows: 1654
- Rows assigned a stable band ID: 1163
- Rows without a stable band ID: 491

## Deliberate mapping revisions

- Removed `band:time-standard:lf`: it grouped isolated 20 and 77.5 kHz channels rather than an enclosing band.
- Removed `band:time-standard:hf`: it grouped isolated channels from 2.5 through 25 MHz with dissimilar propagation.
- Removed `band:navigation:marker-75mhz`: the legacy rows describe the 75 MHz marker channel/window, not a channelized navigation band.
- Replaced `band:aviation:adsb-dme-tacan` with `band:aviation:l-band`; its probes identify the enclosing L-band and deliberately do not turn narrow ADS-B channel rows into bands.
- Split `band:aviation:hf:3mhz` into the distinct `band:aviation:hf:3.4mhz` and `band:aviation:hf:3.8mhz` bands.
- Removed generic 5 GHz ISM IDs: neither the compared KiwiSDR / OpenWebRX+ band tables nor the legacy rows define those broad ranges as ISM bands. Legacy Wi-Fi ranges remain classified under their narrower RLAN IDs.
- Classified television/DVB separately from sound broadcasting and added VHF-low, VHF-high, and UHF television band IDs.
- Classified Wi-Fi as RLAN rather than ISM, while retaining ISM as the shared-spectrum allocation family.
- Classified GSM and LTE into technology-qualified cellular families so overlapping operating bands do not resolve against each other.
- Classified bare L/S/C/X rows as service-independent spectrum ranges; contextual amateur, satellite, cellular, and RLAN rows retain their owning service.

## Summary by classified service

| Service | Assigned | Without ID |
|---|---:|---:|
| `amateur` | 630 | 8 |
| `aviation` | 89 | 31 |
| `broadcast` | 269 | 16 |
| `cellular` | 33 | 78 |
| `ism` | 17 | 3 |
| `land-mobile` | 0 | 133 |
| `maritime` | 51 | 61 |
| `meteorological` | 0 | 3 |
| `navigation` | 18 | 6 |
| `other` | 0 | 81 |
| `personal-radio` | 34 | 0 |
| `rlan` | 10 | 2 |
| `satellite` | 12 | 45 |
| `time-standard` | 0 | 24 |

## Summary by classified family

| Family | Assigned | Without ID |
|---|---:|---:|
| `amateur` | 630 | 8 |
| `aviation-communication` | 88 | 29 |
| `aviation-surveillance` | 1 | 2 |
| `cellular-gsm` | 17 | 2 |
| `cellular-lte` | 16 | 6 |
| `cellular-other` | 0 | 70 |
| `ism` | 17 | 3 |
| `land-mobile` | 0 | 133 |
| `maritime` | 51 | 61 |
| `meteorological` | 0 | 3 |
| `navigation` | 18 | 6 |
| `personal-radio` | 34 | 0 |
| `rlan` | 10 | 2 |
| `satellite` | 12 | 45 |
| `sound-broadcast` | 239 | 16 |
| `spectrum` | 0 | 19 |
| `television-broadcast` | 27 | 0 |
| `time-standard` | 0 | 24 |
| `unknown` | 0 | 62 |
| `weather-broadcast` | 3 | 0 |

## Summary by reason

| Reason | Rows |
|---|---:|
| composite span crosses multiple stable bands | 2 |
| individual channel or narrow channel window; not a band | 1 |
| individual channel/bookmark; not a band | 27 |
| invalid reversed frequency span | 13 |
| no stable band mapping | 239 |
| service has no stable frequency-band catalog | 191 |
| service-independent spectrum range; not a service band | 18 |

## Legacy rows assigned by stable band ID

This is the exhaustive legacy-side provenance for the canonical decisions in `band_mapping.cpp`. Re-run this audit whenever a probe or classifier changes.

### `band:amateur:10m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 10m Ham Band | `amateur` | 28 MHz - 29.7 MHz | `band` | `amateur` | `amateur` |
| `austria.json` | 10m | `amateur` | 28 MHz - 29.7 MHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 10m - Amateur | `amateur` | 28 MHz - 29.7 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | \|10m Ham Band CW | `amateur` | 28 MHz - 28.07 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, Digital | `amateur1` | 28.07 MHz - 28.19 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW - Pilot Emissions | `amateur` | 28.19 MHz - 28.199 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW IBP | `amateur1` | 28.199 MHz - 28.201 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW - Pilot Emissions | `amateur` | 28.201 MHz - 28.225 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, Digital - Pilot Emissions | `amateur1` | 28.225 MHz - 28.3 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB, DV, Digital | `amateur` | 28.3 MHz - 29 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | All Modes | `amateur1` | 29 MHz - 29.3 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | All Modes - Satellites | `amateur` | 29.3 MHz - 29.51 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | All Modes | `amateur1` | 29.51 MHz - 29.52 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | FM, DV - Repeater input | `amateur` | 29.52 MHz - 29.59 MHz | `segment` | `amateur` | `amateur` |
| `brazil.json` | CW, FM, DV - FM calling freq: 29.600 kHz | `amateur1` | 29.59 MHz - 29.62 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | FM, DV - Repeater output 10m Ham Band\| | `amateur` | 29.62 MHz - 29.7 MHz | `segment` | `amateur` | `amateur` |
| `canada.json` | 10m Ham Band | `amateur` | 28 MHz - 29.75 MHz | `band` | `amateur` | `amateur` |
| `china.json` | 10m Ham Band | `amateur` | 28 MHz - 29.7 MHz | `band` | `amateur` | `amateur` |
| `france.json` | 10m - Radioamateur | `amateur` | 28 MHz - 29.7 MHz | `band` | `amateur` | `amateur` |
| `general.json` | 10m Ham Band | `amateur` | 28 MHz - 29.75 MHz | `band` | `amateur` | `amateur` |
| `germany.json` | 10m-Amateur | `amateur` | 28 MHz - 29.7 MHz | `band` | `amateur` | `amateur` |
| `ireland.json` | 10m ham band | `amateur` | 28 MHz - 29.7 MHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 10m | `amateur` | 28 MHz - 29.7 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | \|10m Ham Band  CW | `amateur` | 28 MHz - 28.07 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | NB modes digimodes | `amateur1` | 28.07 MHz - 28.12 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | NB modes digimodes stations (unatt.) | `amateur` | 28.12 MHz - 28.15 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | NB Modes | `amateur1` | 28.15 MHz - 28.19 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | IBP, regional time shared beacons | `amateur` | 28.19 MHz - 28.199 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | IBP, worldwide time shared beacons | `amateur1` | 28.199 MHz - 28.201 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | IBP, continuous duty beacons | `amateur` | 28.201 MHz - 28.225 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | All modes beacons | `amateur1` | 28.225 MHz - 28.3 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | All modes - digimodes stations (unatt.) | `amateur` | 28.3 MHz - 28.32 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | All modes | `amateur1` | 28.32 MHz - 29.1 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | All modes - FM simplex – 10 kHz channel | `amateur` | 29.1 MHz - 29.2 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | All modes - digimodes stations (unatt.) | `amateur1` | 29.2 MHz - 29.3 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Satellite downlink | `amateur` | 29.3 MHz - 29.51 MHz | `segment` | `amateur` | `amateur` |
| `netherlands.json` | Guard channel | `amateur1` | 29.51 MHz - 29.52 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | All modes - FM repeater input (RH1 - RH8) | `amateur` | 29.52 MHz - 29.6 MHz | `segment` | `amateur` | `amateur` |
| `netherlands.json` | All modes - FM calling channel | `amateur1` | 29.6 MHz - 29.61 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | All modes - FM simplex repeater (parrot - input and output) | `amateur` | 29.61 MHz - 29.62 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | All modes - FM repeater outputs (RH1 – RH8) 10m Ham Band\| | `amateur1` | 29.62 MHz - 29.7 MHz | `segment` | `amateur` | `amateur` |
| `republic-of-korea.json` | Amateur Station | `amateur` | 28 MHz - 29.7 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m CW | `amateur` | 28 MHz - 28.07 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m NB Digital | `amateur` | 28.07 MHz - 28.15 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m NB | `amateur` | 28.15 MHz - 28.19 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m Regional Beacons | `amateur` | 28.19 MHz - 28.199 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m International Beacons | `amateur` | 28.199 MHz - 28.201 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m Beacons | `amateur` | 28.201 MHz - 28.225 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m Wide Beacons | `amateur` | 28.225 MHz - 28.3 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m Digi | `amateur` | 28.3 MHz - 28.32 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m | `amateur` | 28.32 MHz - 29 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m Wide | `amateur` | 29 MHz - 29.1 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m FM Simplex | `amateur` | 29.1 MHz - 29.2 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m Wide Digi | `amateur` | 29.2 MHz - 29.3 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m Sat | `amateur` | 29.3 MHz - 29.51 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m Guard Band | `amateur` | 29.51 MHz - 29.52 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m FM Repeater IN | `amateur` | 29.52 MHz - 29.59 MHz | `segment` | `amateur` | `amateur` |
| `russia.json` | 10m FM Call | `amateur` | 29.59 MHz - 29.61 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m FM Simplex Repeater | `amateur` | 29.61 MHz - 29.62 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 10m FM Repeater OUT | `amateur` | 29.62 MHz - 29.7 MHz | `segment` | `amateur` | `amateur` |
| `slovakia.json` | 10m | `amateur` | 28 MHz - 29.7 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 10m | `amateur` | 28 MHz - 29.7 MHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 10m Ham Band | `amateur` | 28 MHz - 29.7 MHz | `band` | `amateur` | `amateur` |
| `usa.json` | 10m Ham Band | `amateur` | 28 MHz - 29.7 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:125cm`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `brazil.json` | 1.3m Ham Band | `amateur` | 220 MHz - 225 MHz | `band` | `amateur` | `amateur` |
| `canada.json` | 1.25m Ham Band | `amateur` | 222 MHz - 225 MHz | `band` | `amateur` | `amateur` |
| `general.json` | 1.25m Ham Band | `amateur` | 222 MHz - 225 MHz | `band` | `amateur` | `amateur` |
| `usa.json` | 1.25m Band (lower) | `amateur` | 219 MHz - 220 MHz | `band` | `amateur` | `amateur` |
| `usa.json` | 1.25m Band (upper) | `amateur` | 222 MHz - 225 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:12m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 12m Ham Band | `amateur` | 24.89 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `austria.json` | 12m | `amateur` | 24.89 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 12m - Amateur | `amateur` | 24.89 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | \|12m Ham Band CW | `amateur` | 24.89 MHz - 24.915 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, Digital | `amateur1` | 24.915 MHz - 24.929 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW IBP | `amateur` | 24.929 MHz - 24.931 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB, DV, Digital 12m Ham Band\| | `amateur1` | 24.931 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `canada.json` | 12m Ham Band | `amateur` | 24.89 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `china.json` | 12m Ham Band | `amateur` | 24.89 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `france.json` | 12m - Radioamateur | `amateur` | 24.89 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `general.json` | 12m Ham Band | `amateur` | 24.89 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `germany.json` | 12m-Amateur | `amateur` | 24.89 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `ireland.json` | 12m ham band | `amateur` | 24.89 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 12m | `amateur` | 24.89 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 12m Ham Band | `amateur` | 24.89 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `republic-of-korea.json` | Amateur Station | `amateur` | 24.89 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 12m CW | `amateur` | 24.89 MHz - 24.915 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 12m NB | `amateur` | 24.915 MHz - 24.929 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 12m Beacons | `amateur` | 24.929 MHz - 24.931 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 12m | `amateur` | 24.931 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `slovakia.json` | 12m | `amateur` | 24.89 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 12m | `amateur` | 24.89 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 12m Ham Band | `amateur` | 24.89 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |
| `usa.json` | 12m Ham Band | `amateur` | 24.89 MHz - 24.99 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:12mm`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 12mm Ham Band | `amateur` | 24 GHz - 24.25 GHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 1.25cm - Amateur | `amateur` | 24 GHz - 24.25 GHz | `band` | `amateur` | `amateur` |
| `brazil.json` | 1.2cm Ham Band | `amateur` | 24 GHz - 24.25 GHz | `band` | `amateur` | `amateur` |
| `china.json` | 1.2cm Ham Band | `amateur` | 24 GHz - 24.25 GHz | `band` | `amateur` | `amateur` |
| `france.json` | 12mm - Radioamateur | `amateur` | 24 GHz - 24.25 GHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 1,5cm | `amateur` | 24 GHz - 24.05 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 1.2cm SAT | `amateur` | 24 GHz - 24.048 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 1.2cm Digi | `amateur` | 24.048 GHz - 24.05 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 1.2cm | `amateur` | 24.05 GHz - 24.25 GHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 24GHz | `amateur` | 24 GHz - 24.05 GHz | `band` | `amateur` | `amateur` |

### `band:amateur:13cm`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 13cm Ham Band | `amateur` | 2.3 GHz - 2.302 GHz | `band` | `amateur` | `amateur` |
| `australia.json` | 13cm Ham Band | `amateur` | 2.4 GHz - 2.45 GHz | `band` | `amateur` | `amateur` |
| `austria.json` | 13cm | `amateur` | 2.304 GHz - 2.31 GHz | `band` | `amateur` | `amateur` |
| `austria.json` | 13cm | `amateur` | 2.32 GHz - 2.322 GHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 13cm - Amateur | `amateur` | 2.3 GHz - 2.45 GHz | `band` | `amateur` | `amateur` |
| `brazil.json` | 13cm Ham Band | `amateur` | 2.33 GHz - 2.45 GHz | `band` | `amateur` | `amateur` |
| `canada.json` | 13cm Ham Band | `amateur` | 2.3 GHz - 2.45 GHz | `band` | `amateur` | `amateur` |
| `china.json` | 13cm Ham Band | `amateur` | 2.3 GHz - 2.45 GHz | `band` | `amateur` | `amateur` |
| `france.json` | 13cm - Radioamateur | `amateur` | 2.3 GHz - 2.45 GHz | `band` | `amateur` | `amateur` |
| `general.json` | 13cm Ham Band | `amateur` | 2.3 GHz - 2.31 GHz | `band` | `amateur` | `amateur` |
| `general.json` | 13cm Ham Band | `amateur` | 2.39 GHz - 2.45 GHz | `band` | `amateur` | `amateur` |
| `germany.json` | 13cm-Amateur | `amateur` | 2.32 GHz - 2.45 GHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 13cm | `amateur` | 2.3 GHz - 2.45 GHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 13 cm Ham Band | `amateur1` | 2.32 GHz - 2.345 GHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 13cm Ham Band Analog and Digital | `amateur` | 2.345 GHz - 2.39375 GHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Experimental | `amateur1` | 2.39375 GHz - 2.39475 GHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Analog and Digital 13cm HAM Band | `amateur` | 2.39475 GHz - 2.4 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 13cm EME / S-Band | `amateur` | 2.32 GHz - 2.32015 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 13cm SAT / 2.4GHz WiFi / S-Band | `amateur` | 2.4 GHz - 2.45 GHz | `band` | `amateur` | `amateur` |
| `slovakia.json` | 13cm | `amateur` | 2.3 GHz - 2.45 GHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 13cm Ham Band | `amateur` | 2.3 GHz - 2.302 GHz | `band` | `amateur` | `amateur` |
| `usa.json` | 13cm Ham Band | `amateur` | 2.3 GHz - 2.31 GHz | `band` | `amateur` | `amateur` |
| `usa.json` | 13cm Ham Band | `amateur` | 2.39 GHz - 2.45 GHz | `band` | `amateur` | `amateur` |

### `band:amateur:15m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 15m Ham Band | `amateur` | 21 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `austria.json` | 15m | `amateur` | 21 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 15m - Amateur | `amateur` | 21 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | \|15m Ham Band CW | `amateur` | 21 MHz - 21.07 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, Digital | `amateur1` | 21.07 MHz - 21.149 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, IBP | `amateur` | 21.149 MHz - 21.151 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB, DV, Digital | `amateur1` | 21.151 MHz - 21.38 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB, AM, DV, Digital 15m Ham Band\| | `amateur` | 21.38 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `canada.json` | 15m Ham Band | `amateur` | 21 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `china.json` | 14m Ham Band | `amateur` | 21 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `france.json` | 15m - Radioamateur | `amateur` | 21 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `general.json` | 15m Ham Band | `amateur` | 21 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `germany.json` | 15m-Amateur | `amateur` | 21 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `ireland.json` | 15m ham band | `amateur` | 21 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 15m | `amateur` | 21 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 15m Ham Band | `amateur` | 21 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `republic-of-korea.json` | Amateur Station | `amateur` | 21 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 15m CW | `amateur` | 21 MHz - 21.07 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 15m NB Digi | `amateur` | 21.07 MHz - 21.11 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 15m Digital | `amateur` | 21.11 MHz - 21.12 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 15m NB | `amateur` | 21.12 MHz - 21.149 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 15m Beacons | `amateur` | 21.149 MHz - 21.151 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 15m | `amateur` | 21.151 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `slovakia.json` | 15m | `amateur` | 21 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 15m | `amateur` | 21 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 15m Ham Band | `amateur` | 21 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |
| `usa.json` | 15m Ham Band | `amateur` | 21 MHz - 21.45 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:160m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 160m Ham Band | `amateur` | 1.8 MHz - 1.875 MHz | `band` | `amateur` | `amateur` |
| `austria.json` | 160m | `amateur` | 1.81 MHz - 1.95 MHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 160m - Amateur | `amateur` | 1.81 MHz - 2 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | \|160m Ham Band CW, Digital | `amateur` | 1.8 MHz - 1.81 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW | `amateur1` | 1.81 MHz - 1.839 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, Digital | `amateur` | 1.839 MHz - 1.84 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB, Digital | `amateur1` | 1.84 MHz - 1.843 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB | `amateur` | 1.843 MHz - 1.85 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB, AM, DV, Digital 160 Ham Band\| | `amateur1` | 1.85 MHz - 2 MHz | `band` | `amateur` | `amateur` |
| `canada.json` | 160m Ham Band | `amateur` | 1.8 MHz - 2 MHz | `band` | `amateur` | `amateur` |
| `china.json` | 160m Ham Band | `amateur` | 1.8 MHz - 2 MHz | `band` | `amateur` | `amateur` |
| `france.json` | 160m - Radioamateur | `amateur` | 1.81 MHz - 1.85 MHz | `band` | `amateur` | `amateur` |
| `general.json` | 160m Ham Band | `amateur` | 1.8 MHz - 2 MHz | `band` | `amateur` | `amateur` |
| `germany.json` | 160m-Amateur | `amateur` | 1.81 MHz - 2 MHz | `band` | `amateur` | `amateur` |
| `ireland.json` | 160m ham band | `amateur` | 1.81 MHz - 2 MHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 160m | `amateur` | 1.83 MHz - 1.85 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 160m Ham Band | `amateur` | 1.81 MHz - 1.88 MHz | `band` | `amateur` | `amateur` |
| `republic-of-korea.json` | Amateur Station | `amateur` | 1.8 MHz - 1.825 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 160m CW | `amateur` | 1.81 MHz - 1.838 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 160m NB | `amateur` | 1.838 MHz - 1.84 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 160m | `amateur` | 1.84 MHz - 2 MHz | `band` | `amateur` | `amateur` |
| `slovakia.json` | 160m | `amateur` | 1.81 MHz - 2 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 160m | `amateur` | 1.81 MHz - 1.85 MHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 160m Ham Band | `amateur` | 1.81 MHz - 2 MHz | `band` | `amateur` | `amateur` |
| `usa.json` | 160m Ham Band | `amateur` | 1.8 MHz - 2 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:17m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 17m Ham Band | `amateur` | 18.068 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |
| `austria.json` | 17m | `amateur` | 18.068 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 17m - Amateur | `amateur` | 18.068 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | \|17m Ham Band CW | `amateur` | 18.068 MHz - 18.095 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, Digital | `amateur1` | 18.095 MHz - 18.109 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW IBP | `amateur` | 18.109 MHz - 18.111 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB, DV, Digital 17m Ham Band\| | `amateur1` | 18.111 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |
| `canada.json` | 17m Ham Band | `amateur` | 18.068 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |
| `china.json` | 17m Ham Band | `amateur` | 18.068 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |
| `france.json` | 17m - Radioamateur | `amateur` | 18.068 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |
| `general.json` | 17m Ham Band | `amateur` | 18.068 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |
| `germany.json` | 17m-Amateur | `amateur` | 18.068 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |
| `ireland.json` | 17m ham band | `amateur` | 18.068 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 17m | `amateur` | 18.069 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 17m Ham Band | `amateur` | 18.068 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 17m CW | `amateur` | 18.068 MHz - 18.095 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 17m NB | `amateur` | 18.095 MHz - 18.109 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 17m Beacons | `amateur` | 18.109 MHz - 18.111 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 17m Digi | `amateur` | 18.111 MHz - 18.12 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 17m | `amateur` | 18.12 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |
| `slovakia.json` | 17m | `amateur` | 18.068 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 17m | `amateur` | 18.068 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 17m Ham Band | `amateur` | 18.068 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |
| `usa.json` | 17m Ham Band | `amateur` | 18.068 MHz - 18.168 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:1mm`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 241-250GHz Ham Band | `amateur` | 241 GHz - 250 GHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 1mm - Amateur | `amateur` | 241 GHz - 250 GHz | `band` | `amateur` | `amateur` |
| `brazil.json` | 1mm Ham Band | `amateur` | 241 GHz - 250 GHz | `band` | `amateur` | `amateur` |
| `france.json` | 1mm - Radioamateur | `amateur` | 241 GHz - 250 GHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 1mm | `amateur` | 241 GHz - 250 GHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Radio Ham 1 mm band | `amateur` | 241 GHz - 250 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 1mm | `amateur` | 241 GHz - 250 GHz | `band` | `amateur` | `amateur` |

### `band:amateur:20m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 20m Ham Band | `amateur` | 14 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `austria.json` | 20m | `amateur` | 14 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 20m - Amateur | `amateur` | 14 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | \|20m Ham Band CW | `amateur` | 14 MHz - 14.07 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, Digital | `amateur1` | 14.07 MHz - 14.099 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW IBP | `amateur` | 14.099 MHz - 14.101 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB, DV, Digital | `amateur1` | 14.101 MHz - 14.282 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB, AM, DV, Digital 20m Ham Band\| | `amateur` | 14.285 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `canada.json` | 20m Ham Band | `amateur` | 14 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `china.json` | 20m Ham Band | `amateur` | 14 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `france.json` | 20m - Radioamateur | `amateur` | 14 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `general.json` | 20m Ham Band | `amateur` | 14 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `germany.json` | 20m-Amateur | `amateur` | 14 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `ireland.json` | 20m ham band | `amateur` | 14 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 20m | `amateur` | 14 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | \|20m Ham Band | `amateur` | 14 MHz - 14.07 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | RTTY | `amateur1` | 14.07 MHz - 14.095 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Packet | `amateur` | 14.095 MHz - 14.0995 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | NCDXF Beacons | `amateur1` | 14.0995 MHz - 14.1005 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Packet | `amateur` | 14.1005 MHz - 14.112 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 20m Ham Band | `amateur1` | 14.112 MHz - 14.228 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | SSTV | `amateur` | 14.228 MHz - 14.232 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 20m Ham Band | `amateur1` | 14.232 MHz - 14.284 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | AM calling frequency | `amateur` | 14.284 MHz - 14.288 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 20m Ham Band\| | `amateur1` | 14.288 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `republic-of-korea.json` | Amateur Station | `amateur` | 14 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 20m CW | `amateur` | 14 MHz - 14.07 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 20m NB | `amateur` | 14.07 MHz - 14.099 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 20m Beacons | `amateur` | 14.099 MHz - 14.101 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 20m Digi | `amateur` | 14.101 MHz - 14.112 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 20m | `amateur` | 14.112 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `slovakia.json` | 20m | `amateur` | 14 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 20m | `amateur` | 14 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 20m Ham Band | `amateur` | 14 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |
| `usa.json` | 20m Ham Band | `amateur` | 14 MHz - 14.35 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:2200m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 2200m Ham Band | `amateur` | 135.7 kHz - 137.8 kHz | `band` | `amateur` | `amateur` |
| `austria.json` | LW | `amateur` | 135.7 kHz - 137.8 kHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 2200m - Amateur | `amateur` | 135.7 kHz - 137.8 kHz | `band` | `amateur` | `amateur` |
| `brazil.json` | 2200m Ham Band CW, Digital | `amateur` | 135.7 kHz - 137.8 kHz | `band` | `amateur` | `amateur` |
| `canada.json` | 2200m Ham Band | `amateur` | 135.7 kHz - 137.8 kHz | `band` | `amateur` | `amateur` |
| `china.json` | 2200m Ham Band | `amateur` | 135.7 kHz - 137.8 kHz | `band` | `amateur` | `amateur` |
| `france.json` | 137KHz - Radioamateur | `amateur` | 135.5 kHz - 137.8 kHz | `band` | `amateur` | `amateur` |
| `germany.json` | LW-Amateur | `amateur` | 135.7 kHz - 137.8 kHz | `band` | `amateur` | `amateur` |
| `ireland.json` | 2200m Ham Band | `amateur` | 135.7 kHz - 137.8 kHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 137kHz | `amateur` | 135.5 kHz - 137.8 kHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | \|2200m Ham Band  Tests, Transatlantic Window | `amateur` | 135.7 kHz - 136 kHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 2200m Telegraphy | `amateur1` | 136 kHz - 137.4 kHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Digital | `amateur` | 137.4 kHz - 137.6 kHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Slow Telegraphy   2200m Ham Band\| | `amateur1` | 137.6 kHz - 137.8 kHz | `band` | `amateur` | `amateur` |
| `russia.json` | 2200m | `amateur` | 135.7 kHz - 137.8 kHz | `band` | `amateur` | `amateur` |
| `slovakia.json` | LW | `amateur` | 135.7 kHz - 137.8 kHz | `band` | `amateur` | `amateur` |
| `turkey.json` | LW | `amateur` | 135.7 kHz - 137.8 kHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 2200m Ham Band | `amateur` | 135.7 kHz - 137.8 kHz | `band` | `amateur` | `amateur` |
| `usa.json` | 2200m Band | `amateur` | 135.7 kHz - 137.8 kHz | `band` | `amateur` | `amateur` |

### `band:amateur:23cm`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 23cm Ham Band | `amateur` | 1.24 GHz - 1.3 GHz | `band` | `amateur` | `amateur` |
| `austria.json` | 23cm | `amateur` | 1.24 GHz - 1.3 GHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 23cm - Amateur | `amateur` | 1.24 GHz - 1.3 GHz | `band` | `amateur` | `amateur` |
| `brazil.json` | 23cm Ham Band | `amateur` | 1.24 GHz - 1.3 GHz | `band` | `amateur` | `amateur` |
| `canada.json` | 23cm Ham Band | `amateur` | 1.24 GHz - 1.3 GHz | `band` | `amateur` | `amateur` |
| `china.json` | 23cm Ham Band | `amateur` | 1.24 GHz - 1.3 GHz | `band` | `amateur` | `amateur` |
| `france.json` | 23cm - Radioamateur | `amateur` | 1.24 GHz - 1.3 GHz | `band` | `amateur` | `amateur` |
| `general.json` | 23cm Ham Band | `amateur` | 1.24 GHz - 1.3 GHz | `band` | `amateur` | `amateur` |
| `germany.json` | 23cm-Amateur | `amateur` | 1.24 GHz - 1.3 GHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 23cm | `amateur` | 1.24 GHz - 1.245 GHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 23cm | `amateur` | 1.267 GHz - 1.298 GHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 23cm Ham Band | `amateur` | 1.24 GHz - 1.3 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 23cm SAT / L-Band | `amateur` | 1.26 GHz - 1.27 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 23cm / L-Band | `amateur` | 1.27 GHz - 1.290994 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 23cm FM Repeater IN / L-Band | `amateur` | 1.290994 GHz - 1.291481 GHz | `segment` | `amateur` | `amateur` |
| `russia.json` | 23cm / L-Band | `amateur` | 1.291481 GHz - 1.296 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 23cm CW+Digi / L-Band | `amateur` | 1.296025 GHz - 1.29615 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 23cm / L-Band | `amateur` | 1.29615 GHz - 1.2968 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 23cm Beacons (CW+Digi) / L-Band | `amateur` | 1.2968 GHz - 1.296994 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 23cm FM Repeater OUT / L-Band | `amateur` | 1.296994 GHz - 1.29749 GHz | `segment` | `amateur` | `amateur` |
| `russia.json` | 23cm FM / L-Band | `amateur` | 1.29749 GHz - 1.298 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 23cm / L-Band | `amateur` | 1.298 GHz - 1.3 GHz | `band` | `amateur` | `amateur` |
| `slovakia.json` | 23cm | `amateur` | 1.24 GHz - 1.3 GHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 23cm | `amateur` | 1.24 GHz - 1.3 GHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 23cm Ham Band | `amateur` | 1.24 GHz - 1.325 GHz | `band` | `amateur` | `amateur` |
| `usa.json` | 23cm Ham Band | `amateur` | 1.24 GHz - 1.3 GHz | `band` | `amateur` | `amateur` |

### `band:amateur:25mm`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 122-123GHz Ham Band | `amateur` | 122.25 GHz - 123 GHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 2.5mm - Amateur | `amateur` | 122.25 GHz - 123 GHz | `band` | `amateur` | `amateur` |
| `brazil.json` | 2.5mm Ham Band | `amateur` | 122.25 GHz - 123 GHz | `band` | `amateur` | `amateur` |
| `france.json` | 2,5mm - Radioamateur | `amateur` | 122.25 GHz - 123 GHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 2,4mm | `amateur` | 122.5 GHz - 123 GHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Radio Ham 2.5 mm band | `amateur` | 122.25 GHz - 123 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 2.5mm | `amateur` | 122.251 GHz - 123 GHz | `band` | `amateur` | `amateur` |

### `band:amateur:2m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 2m Ham Band | `amateur` | 144 MHz - 148 MHz | `band` | `amateur` | `amateur` |
| `austria.json` | 2m | `amateur` | 144 MHz - 146 MHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 2m - Amateur | `amateur` | 144 MHz - 146 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | \|2m Ham Band All Modes - Satellites | `amateur` | 144 MHz - 144.025 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW - EME | `amateur1` | 144.025 MHz - 144.11 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, Digital - EME | `amateur` | 144.11 MHz - 144.15 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB, Digital | `amateur1` | 144.15 MHz - 144.18 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB - Calling freq: 144.2 MHz | `amateur` | 144.18 MHz - 144.275 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW - Pilot Emissions | `amateur1` | 144.275 MHz - 144.3 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB - Calling freq: 144.2 MHz | `amateur` | 144.3 MHz - 144.36 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB, Digital | `amateur1` | 144.36 MHz - 144.4 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | All Modes | `amateur` | 144.4 MHz - 144.6 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | FM, DV - Repeater input | `amateur1` | 144.6 MHz - 144.9 MHz | `segment` | `amateur` | `amateur` |
| `brazil.json` | CW, FM, DV, Digital | `amateur` | 144.9 MHz - 145 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | All Modes, IVG | `amateur1` | 145 MHz - 145.2 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | FM, DV - Repeater output | `amateur` | 145.2 MHz - 145.5 MHz | `segment` | `amateur` | `amateur` |
| `brazil.json` | All Modes | `amateur1` | 145.5 MHz - 145.565 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | APRS | `amateur` | 145.565 MHz - 145.575 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | All Modes | `amateur1` | 145.575 MHz - 145.79 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | Guard Band | `amateur` | 145.79 MHz - 145.8 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | All Modes - Satellites | `amateur1` | 145.8 MHz - 146 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | FM, DV - Repeater input | `amateur` | 146 MHz - 146.39 MHz | `segment` | `amateur` | `amateur` |
| `brazil.json` | CW, FM, DV - Calling freq: 146.52 MHz | `amateur1` | 146.39 MHz - 146.6 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | FM, DV - Repeater output | `amateur` | 146.6 MHz - 146.99 MHz | `segment` | `amateur` | `amateur` |
| `brazil.json` | FM, DV - Repeater input | `amateur1` | 146.99 MHz - 147.4 MHz | `segment` | `amateur` | `amateur` |
| `brazil.json` | CW, FM, DV | `amateur` | 147.4 MHz - 147.59 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | FM, DV - Repeater output 2m Ham Band\| | `amateur1` | 147.59 MHz - 148 MHz | `segment` | `amateur` | `amateur` |
| `canada.json` | 2m Ham Band | `amateur` | 144 MHz - 148 MHz | `band` | `amateur` | `amateur` |
| `china.json` | 2m Ham Band | `amateur` | 144 MHz - 148 MHz | `band` | `amateur` | `amateur` |
| `france.json` | 2m - Radioamateur | `amateur` | 144 MHz - 146 MHz | `band` | `amateur` | `amateur` |
| `general.json` | 2m Ham Band | `amateur` | 144 MHz - 148 MHz | `band` | `amateur` | `amateur` |
| `germany.json` | 2m-Amateur | `amateur` | 144 MHz - 146 MHz | `band` | `amateur` | `amateur` |
| `ireland.json` | 2m ham band | `amateur` | 144 MHz - 146 MHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 2m | `amateur` | 144 MHz - 146 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | \|2m Ham Band EME | `amateur` | 144 MHz - 144.035 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | CW | `amateur1` | 144.035 MHz - 144.15 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | SSB | `amateur` | 144.15 MHz - 144.4 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Beacons | `amateur1` | 144.4 MHz - 144.48 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | All modes | `amateur1` | 144.5 MHz - 144.8 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Digital comm | `amateur` | 144.8 MHz - 144.99 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Voice | `amateur1` | 144.99 MHz - 145.57 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Repeaters Voice | `amateur` | 145.57 MHz - 145.8 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 2m Band (Not NL) | `amateur` | 146 MHz - 148 MHz | `band` | `amateur` | `amateur` |
| `republic-of-korea.json` | Amateur Station | `amateur` | 144 MHz - 146 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 2m CW | `amateur` | 144.035 MHz - 144.11 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 2m CW+Digi | `amateur` | 144.11 MHz - 144.18 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 2m CW+SSB | `amateur` | 144.18 MHz - 144.36 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 2m CW+SSB+Digi | `amateur` | 144.36 MHz - 144.4 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 2m Beacons (CW+Digi) | `amateur` | 144.4 MHz - 144.49 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 2m Wide Digi | `amateur` | 144.5 MHz - 144.794 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 2m Digi | `amateur` | 144.794 MHz - 144.99 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 2m FM Repeater IN | `amateur` | 144.99 MHz - 145.194 MHz | `segment` | `amateur` | `amateur` |
| `russia.json` | 2m FM Sat | `amateur` | 145.194 MHz - 145.206 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 2m FM | `amateur` | 145.206 MHz - 145.594 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 2m FM Repeater OUT | `amateur` | 145.594 MHz - 145.7935 MHz | `segment` | `amateur` | `amateur` |
| `russia.json` | 2m FM Sat | `amateur` | 145.7935 MHz - 145.806 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 2m Sat | `amateur` | 145.806 MHz - 146 MHz | `band` | `amateur` | `amateur` |
| `slovakia.json` | 2m | `amateur` | 144 MHz - 146 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 2m | `amateur` | 144 MHz - 146 MHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 2m Ham Band | `amateur` | 144 MHz - 146 MHz | `band` | `amateur` | `amateur` |
| `usa.json` | 2m Ham Band | `amateur` | 144 MHz - 148 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:2mm`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 134-141GHz Ham Band | `amateur` | 134 GHz - 141 GHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 2mm - Amateur | `amateur` | 142 GHz - 149 GHz | `band` | `amateur` | `amateur` |
| `brazil.json` | 2mm Ham Band | `amateur` | 134 GHz - 141 GHz | `band` | `amateur` | `amateur` |
| `france.json` | 2mm - Radioamateur | `amateur` | 134 GHz - 141 GHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 2,23mm | `amateur` | 134 GHz - 141 GHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 2,1mm | `amateur` | 142 GHz - 144 GHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Radio Ham 2 mm band | `amateur` | 134 GHz - 141 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 2mm | `amateur` | 134.001 GHz - 141 GHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 134GHz | `amateur` | 134 GHz - 142 GHz | `band` | `amateur` | `amateur` |

### `band:amateur:30m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 30m Ham Band | `amateur` | 10.1 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `austria.json` | 30m | `amateur` | 10.1 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 30m - Amateur | `amateur` | 10.1 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | \|30m Ham Band | `amateur` | 10.1 MHz - 10.13 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, Digital 30m Ham Band\| | `amateur1` | 10.13 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `canada.json` | 30m Ham Band | `amateur` | 10.1 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `china.json` | 30m Ham Band | `amateur` | 10.1 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `france.json` | 30m - Radioamateur | `amateur` | 10.1 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `general.json` | 30m Ham Band | `amateur` | 10.1 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `germany.json` | 30m-Amateur | `amateur` | 10.1 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `ireland.json` | 30m ham band | `amateur` | 10.1 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 30m | `amateur` | 10.1 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | \|30m Ham Band | `amateur` | 10.1 MHz - 10.13 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | RTTY | `amateur1` | 10.13 MHz - 10.14 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Packet 30m Ham Band\| | `amateur` | 10.14 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `republic-of-korea.json` | Amateur Station | `amateur` | 10.1 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 30m CW | `amateur` | 10.1 MHz - 10.13 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 30m NB | `amateur` | 10.13 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `slovakia.json` | 30m | `amateur` | 10.1 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 30m | `amateur` | 10.1 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 30m Ham Band | `amateur` | 10.1 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |
| `usa.json` | 30m Ham Band | `amateur` | 10.1 MHz - 10.15 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:33cm`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `brazil.json` | 33cm Ham Band | `amateur` | 902 MHz - 928 MHz | `band` | `amateur` | `amateur` |
| `canada.json` | 33cm Ham Band | `amateur` | 902 MHz - 928 MHz | `band` | `amateur` | `amateur` |
| `general.json` | 33cm Ham Band | `amateur` | 902 MHz - 928 MHz | `band` | `amateur` | `amateur` |
| `usa.json` | 33cm Ham Band | `amateur` | 902 MHz - 928 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:3cm`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 3cm Ham Band | `amateur` | 10 GHz - 10.5 GHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 3cm - Amateur | `amateur` | 10 GHz - 10.5 GHz | `band` | `amateur` | `amateur` |
| `brazil.json` | 3cm Ham Band | `amateur` | 10 GHz - 10.5 GHz | `band` | `amateur` | `amateur` |
| `china.json` | 3cm Ham Band | `amateur` | 10 GHz - 10.5 GHz | `band` | `amateur` | `amateur` |
| `france.json` | 3cm - Radioamateur | `amateur` | 10 GHz - 10.5 GHz | `band` | `amateur` | `amateur` |
| `germany.json` | 3cm-Amateur | `amateur` | 10 GHz - 10.5 GHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 3cm | `amateur` | 10.3 GHz - 10.5 GHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Radio Ham 3 cm band | `amateur` | 10 GHz - 10.5 GHz | `band` | `amateur` | `amateur` |
| `qo-100.json` | CW | `amateur` | 10.489505 GHz - 10.48954 GHz | `band` | `amateur` | `amateur` |
| `qo-100.json` | NB Digi | `amateur` | 10.48954 GHz - 10.48958 GHz | `band` | `amateur` | `amateur` |
| `qo-100.json` | Digi | `amateur` | 10.48958 GHz - 10.48965 GHz | `band` | `amateur` | `amateur` |
| `qo-100.json` | SSB | `amateur` | 10.48965 GHz - 10.489745 GHz | `band` | `amateur` | `amateur` |
| `qo-100.json` | SSB | `amateur` | 10.489755 GHz - 10.48985 GHz | `band` | `amateur` | `amateur` |
| `qo-100.json` | Emergency | `amateur` | 10.48985 GHz - 10.48987 GHz | `band` | `amateur` | `amateur` |
| `qo-100.json` | Mixed/Contest | `amateur` | 10.48987 GHz - 10.48999 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 3cm CW+Digi / X-Band | `amateur` | 10 GHz - 10.15 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 3cm / X-Band | `amateur` | 10.15 GHz - 10.25 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 3cm CW+Digi / X-Band | `amateur` | 10.25 GHz - 10.35 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 3cm / X-Band | `amateur` | 10.35 GHz - 10.368 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 3cm CW+Digi / X-Band | `amateur` | 10.368 GHz - 10.37 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 3cm / X-Band | `amateur` | 10.37 GHz - 10.45 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 3cm SAT / X-Band | `amateur` | 10.45 GHz - 10.5 GHz | `band` | `amateur` | `amateur` |

### `band:amateur:40m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 40m Ham Band | `amateur` | 7 MHz - 7.3 MHz | `band` | `amateur` | `amateur` |
| `austria.json` | 40m | `amateur` | 7 MHz - 7.2 MHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 40m - Amateur | `amateur` | 7 MHz - 7.2 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | \|40m Ham Band CW | `amateur` | 7 MHz - 7.04 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, Digital | `amateur1` | 7.04 MHz - 7.047 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB, Digital | `amateur` | 7.047 MHz - 7.05 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB, DV, Digital | `amateur1` | 7.05 MHz - 7.1 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB, AM, DV, Digital 40m Ham Band\| | `amateur` | 7.1 MHz - 7.3 MHz | `band` | `amateur` | `amateur` |
| `canada.json` | 40m Ham Band | `amateur` | 7 MHz - 7.3 MHz | `band` | `amateur` | `amateur` |
| `china.json` | 40m Ham Band | `amateur` | 7 MHz - 7.2 MHz | `band` | `amateur` | `amateur` |
| `france.json` | 40m - Radioamateur | `amateur` | 7 MHz - 7.2 MHz | `band` | `amateur` | `amateur` |
| `general.json` | 40m Ham Band | `amateur` | 7 MHz - 7.2 MHz | `band` | `amateur` | `amateur` |
| `germany.json` | 40m-Amateur | `amateur` | 7 MHz - 7.2 MHz | `band` | `amateur` | `amateur` |
| `ireland.json` | 40m ham band | `amateur` | 7 MHz - 7.2 MHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 40m | `amateur` | 7 MHz - 7.2 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | \|40m Ham Band | `amateur` | 7 MHz - 7.08 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | RTTY/Data | `amateur1` | 7.08 MHz - 7.169 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | SSTV | `amateur` | 7.169 MHz - 7.173 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 40m Ham Band\| | `amateur1` | 7.173 MHz - 7.2 MHz | `band` | `amateur` | `amateur` |
| `republic-of-korea.json` | Amateur Station | `amateur` | 7 MHz - 7.1 MHz | `band` | `amateur` | `amateur` |
| `republic-of-korea.json` | Amateur Station | `amateur` | 7.1 MHz - 7.2 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 40m CW | `amateur` | 7 MHz - 7.04 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 40m NB, Digi 500Hz | `amateur` | 7.04 MHz - 7.05 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 40m Digi | `amateur` | 7.05 MHz - 7.053 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 40m | `amateur` | 7.053 MHz - 7.2 MHz | `band` | `amateur` | `amateur` |
| `slovakia.json` | 40m | `amateur` | 7 MHz - 7.2 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 40m | `amateur` | 7 MHz - 7.2 MHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 40m Ham Band | `amateur` | 7 MHz - 7.2 MHz | `band` | `amateur` | `amateur` |
| `usa.json` | 40m Ham Band | `amateur` | 7 MHz - 7.3 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:4m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `belgium.json` | 4m - Amateur | `amateur` | 69.945 MHz - 69.955 MHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 4m - Amateur | `amateur` | 70.19 MHz - 70.4125 MHz | `band` | `amateur` | `amateur` |
| `germany.json` | 4m-Amateur | `amateur` | 70.15 MHz - 70.2 MHz | `band` | `amateur` | `amateur` |
| `ireland.json` | 4m ham band | `amateur` | 70 MHz - 70.5 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 4m Ham Band (not NL) | `amateur` | 70 MHz - 70.5 MHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 4m Ham Band | `amateur` | 70 MHz - 70.5 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:4mm`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 4mm Ham Band | `amateur` | 76 GHz - 81 GHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 4mm - Amateur | `amateur` | 75.5 GHz - 81 GHz | `band` | `amateur` | `amateur` |
| `china.json` | 4mm Ham Band | `amateur` | 76 GHz - 81 GHz | `band` | `amateur` | `amateur` |
| `france.json` | 4mm - Radioamateur | `amateur` | 75.5 GHz - 81.5 GHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 4mm | `amateur` | 75.5 GHz - 81.5 GHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Radio Ham 4 mm band | `amateur` | 76.5 GHz - 81.5 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 4mm | `amateur` | 76 GHz - 78 GHz | `band` | `amateur` | `amateur` |

### `band:amateur:5cm`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 6cm Ham Band | `amateur` | 5.65 GHz - 5.85 GHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 6cm - Amateur | `amateur` | 5.65 GHz - 5.85 GHz | `band` | `amateur` | `amateur` |
| `brazil.json` | 5cm Ham Band | `amateur` | 5.65 GHz - 5.925 GHz | `band` | `amateur` | `amateur` |
| `china.json` | 5cm Ham Band | `amateur` | 5.65 GHz - 5.85 GHz | `band` | `amateur` | `amateur` |
| `france.json` | 6cm - Radioamateur | `amateur` | 5.65 GHz - 5.85 GHz | `band` | `amateur` | `amateur` |
| `france.json` | 5,650GHz - Radioamateur | `amateur` | 5.65 GHz - 5.85 GHz | `band` | `amateur` | `amateur` |
| `germany.json` | 6cm-Amateur | `amateur` | 5.65 GHz - 5.85 GHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 5cm | `amateur` | 5.65 GHz - 5.67 GHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 5cm | `amateur` | 5.76 GHz - 5.77 GHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 5cm | `amateur` | 5.83 GHz - 5.85 GHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Radio Ham 6 cm band | `amateur` | 5.65 GHz - 5.85 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 6cm CW+Digi / 5GHz WiFi / C-Band | `amateur` | 5.65 GHz - 5.67 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 6cm Digi / 5GHz WiFi / C-Band | `amateur` | 5.725 GHz - 5.76 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 6cm Digi / 5GHz WiFi / C-Band | `amateur` | 5.762 GHz - 5.79 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 6cm CW+Digi / 5GHz WiFi / C-Band | `amateur` | 5.79 GHz - 5.85 GHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 5GHz | `amateur` | 5.65 GHz - 5.67 GHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 5GHz | `amateur` | 5.82 GHz - 5.85 GHz | `band` | `amateur` | `amateur` |

### `band:amateur:600m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `belgium.json` | 600m - Amateur | `amateur` | 501 kHz - 504 kHz | `band` | `amateur` | `amateur` |

### `band:amateur:60m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 60m Ham Band (Unavailable) | `amateur` | 5.3515 MHz - 5.3665 MHz | `band` | `amateur` | `amateur` |
| `austria.json` | 60m | `amateur` | 5.3513 MHz - 5.3665 MHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 60m - Amateur | `amateur` | 5.3515 MHz - 5.3665 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | \|60m Ham Band CW, Digital | `amateur` | 5.3515 MHz - 5.354 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB, DV, Digital | `amateur1` | 5.354 MHz - 5.366 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, Digital 60m Ham Band\| | `amateur` | 5.366 MHz - 5.3665 MHz | `band` | `amateur` | `amateur` |
| `canada.json` | 60m Ham Band | `amateur` | 5.351 MHz - 5.366 MHz | `band` | `amateur` | `amateur` |
| `france.json` | 60m - Radioamateur | `amateur` | 5.3515 MHz - 5.3665 MHz | `band` | `amateur` | `amateur` |
| `general.json` | 60m Ham Band | `amateur` | 5.3515 MHz - 5.3665 MHz | `band` | `amateur` | `amateur` |
| `germany.json` | 60m-Amateur | `amateur` | 5.3515 MHz - 5.3665 MHz | `band` | `amateur` | `amateur` |
| `ireland.json` | 60m ham band | `amateur` | 5.3515 MHz - 5.3665 MHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 60m | `amateur` | 5.3515 MHz - 5.3665 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 60m Ham Band Ch. 1 (Not NL) | `amateur` | 5.3305 MHz - 5.3334 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 60m Ham Band (Not NL) | `amateur1` | 5.3334 MHz - 5.3465 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Ch. 2 (60m) (Not NL) | `amateur` | 5.3465 MHz - 5.3494 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 60m Ham Band (Not NL) | `amateur1` | 5.3494 MHz - 5.3515 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 60m Ham Band CW | `amateur` | 5.3515 MHz - 5.354 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 60m HAM Alle modes (USB) | `amateur1` | 5.354 MHz - 5.366 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 60m HAM Weak NB modes | `amateur` | 5.366 MHz - 5.3665 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 60m Ham Band (Not NL) | `amateur1` | 5.3665 MHz - 5.3715 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Ch. 4 (60m) (Not NL) | `amateur` | 5.3715 MHz - 5.3744 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 60m Ham Band (Not NL) | `amateur1` | 5.3744 MHz - 5.4035 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Ch. 5 60m Ham Band (Not NL) | `amateur` | 5.4035 MHz - 5.4064 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 60m | `amateur` | 5.3515 MHz - 5.3665 MHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 60m Ham Band | `amateur` | 5.2585 MHz - 5.4065 MHz | `band` | `amateur` | `amateur` |
| `usa.json` | 60m Ham Band | `amateur` | 5.3305 MHz - 5.4065 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:630m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 630m Ham Band | `amateur` | 472 kHz - 479 kHz | `band` | `amateur` | `amateur` |
| `austria.json` | 630m | `amateur` | 472 kHz - 479 kHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 630m - Amateur | `amateur` | 472 kHz - 479 kHz | `band` | `amateur` | `amateur` |
| `brazil.json` | 635m Ham Band CW, Digital | `amateur` | 472 kHz - 479 kHz | `band` | `amateur` | `amateur` |
| `canada.json` | 630m Ham Band | `amateur` | 472 kHz - 479 kHz | `band` | `amateur` | `amateur` |
| `france.json` | 472KHz - Radioamateur | `amateur` | 472 kHz - 479 kHz | `band` | `amateur` | `amateur` |
| `germany.json` | 630m-Amateur | `amateur` | 472 kHz - 479 kHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 472kHz | `amateur` | 472 kHz - 479 kHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 635m Ham Band CW | `amateur` | 472 kHz - 475 kHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 635m Ham Band CW, Digimodes | `amateur1` | 475 kHz - 479 kHz | `band` | `amateur` | `amateur` |
| `republic-of-korea.json` | Amateur | `amateur` | 472 kHz - 479 kHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 630m | `amateur` | 472 kHz - 479 kHz | `band` | `amateur` | `amateur` |
| `usa.json` | 630m Band | `amateur` | 472 kHz - 479 kHz | `band` | `amateur` | `amateur` |

### `band:amateur:6m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 6m Ham Band | `amateur` | 50 MHz - 54 MHz | `band` | `amateur` | `amateur` |
| `austria.json` | 6m | `amateur` | 50 MHz - 52 MHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 6m - Amateur | `amateur` | 50 MHz - 52 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | 6m Ham Band | `amateur` | 50 MHz - 54 MHz | `band` | `amateur` | `amateur` |
| `canada.json` | 6m Ham Band | `amateur` | 50 MHz - 54 MHz | `band` | `amateur` | `amateur` |
| `china.json` | 6m Ham Band | `amateur` | 50 MHz - 54 MHz | `band` | `amateur` | `amateur` |
| `france.json` | 6m - Radioamateur | `amateur` | 50 MHz - 54 MHz | `band` | `amateur` | `amateur` |
| `general.json` | 6m Ham Band | `amateur` | 50 MHz - 54 MHz | `band` | `amateur` | `amateur` |
| `germany.json` | 6m-Amateur | `amateur` | 50.03 MHz - 51 MHz | `band` | `amateur` | `amateur` |
| `ireland.json` | 6m ham band | `amateur` | 50 MHz - 52 MHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 6m | `amateur` | 50 MHz - 51 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 6m Ham Band | `amateur` | 50 MHz - 52 MHz | `band` | `amateur` | `amateur` |
| `republic-of-korea.json` | Amateur Station | `amateur` | 50 MHz - 54 MHz | `band` | `amateur` | `amateur` |
| `slovakia.json` | 6m | `amateur` | 50 MHz - 52 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 6m | `amateur` | 50.03 MHz - 51 MHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 6m Ham Band | `amateur` | 50 MHz - 52 MHz | `band` | `amateur` | `amateur` |
| `usa.json` | 6m Ham Band | `amateur` | 50 MHz - 54 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:6mm`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 6mm Ham Band | `amateur` | 47 GHz - 47.2 GHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 6mm - Amateur | `amateur` | 47 GHz - 47.2 GHz | `band` | `amateur` | `amateur` |
| `brazil.json` | 6mm Ham Band | `amateur` | 47 GHz - 47.2 GHz | `band` | `amateur` | `amateur` |
| `china.json` | 6mm Ham Band | `amateur` | 47 GHz - 47.2 GHz | `band` | `amateur` | `amateur` |
| `france.json` | 6mm - Radioamateur | `amateur` | 47 GHz - 47.2 GHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 7mm | `amateur` | 47 GHz - 47.2 GHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Radio Ham 6 mm band | `amateur` | 47 GHz - 47.2 GHz | `band` | `amateur` | `amateur` |
| `russia.json` | 6mm | `amateur` | 47.002 GHz - 47.2 GHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 47GHz | `amateur` | 47 GHz - 47.2 GHz | `band` | `amateur` | `amateur` |

### `band:amateur:70cm`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 70cm Ham Band | `amateur` | 430 MHz - 450 MHz | `band` | `amateur` | `amateur` |
| `austria.json` | 70cm | `amateur` | 430 MHz - 440 MHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 70cm - Amateur | `amateur` | 430 MHz - 440 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | \|70cm Ham Band All Modes | `amateur` | 430 MHz - 432 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW - EME | `amateur1` | 432 MHz - 432.025 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, Digital - EME | `amateur` | 432.025 MHz - 432.1 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB - Calling freq: 432.1 MHz | `amateur1` | 432.1 MHz - 432.3 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW - Pilot Emissions | `amateur` | 432.3 MHz - 432.4 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, Digital - Pilot Emissions | `amateur1` | 432.4 MHz - 432.42 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSB, Digital | `amateur` | 432.42 MHz - 433 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, Digital | `amateur1` | 433 MHz - 433.05 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | All Modes | `amateur` | 433.05 MHz - 434 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | Fm, DV - Repeater input | `amateur1` | 434 MHz - 435 MHz | `segment` | `amateur` | `amateur` |
| `brazil.json` | All Modes - Satellites | `amateur` | 435 MHz - 438 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | All Modes | `amateur1` | 438 MHz - 439 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | FM, DV - Repeater output 70cm Ham Band\| | `amateur` | 439 MHz - 440 MHz | `segment` | `amateur` | `amateur` |
| `canada.json` | 70cm Ham Band | `amateur` | 430 MHz - 450 MHz | `band` | `amateur` | `amateur` |
| `china.json` | 70cm Ham Band | `amateur` | 430 MHz - 440 MHz | `band` | `amateur` | `amateur` |
| `france.json` | 70cm - Radioamateur | `amateur` | 420 MHz - 450 MHz | `band` | `amateur` | `amateur` |
| `general.json` | 70cm Ham Band | `amateur` | 420 MHz - 450 MHz | `band` | `amateur` | `amateur` |
| `germany.json` | 70cm-Amateur | `amateur` | 430 MHz - 440 MHz | `band` | `amateur` | `amateur` |
| `ireland.json` | 70cm ham band | `amateur` | 430 MHz - 440 MHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 70cm | `amateur` | 430 MHz - 434 MHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 70cm | `amateur` | 435 MHz - 438 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | \|70cm Ham Band | `amateur` | 430 MHz - 433.05 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 70cm Ham Band\| | `amateur` | 438 MHz - 440 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 70cm | `amateur` | 430 MHz - 432.025 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 70cm CW+Digi | `amateur` | 432.025 MHz - 432.1 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 70cm CW+SSB+Digi | `amateur` | 432.1 MHz - 432.4 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 70cm Beacons | `amateur` | 432.4 MHz - 432.5 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 70cm | `amateur` | 432.5 MHz - 433 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 70cm FM Repeaters in | `amateur` | 433 MHz - 433.075 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | LPD / 70cm FM Repeaters in | `amateur` | 433.075 MHz - 433.6 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | LPD / 70cm | `amateur` | 433.6 MHz - 434 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | LPD / 70cm CW+Digi | `amateur` | 434 MHz - 434.1 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | LPD / 70cm | `amateur` | 434.1 MHz - 434.6 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | LPD / 70cm FM Repeaters out | `amateur` | 434.6 MHz - 434.775 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 70cm FM Repeaters out | `amateur` | 434.775 MHz - 435 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 70cm SAT | `amateur` | 435 MHz - 440 MHz | `band` | `amateur` | `amateur` |
| `slovakia.json` | 70cm | `amateur` | 430 MHz - 440 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 70cm | `amateur` | 430.2 MHz - 430.7 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 70cm-RepeaterRX | `amateur` | 431.55 MHz - 431.825 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 70cm | `amateur` | 432 MHz - 432.975 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 70cm | `amateur` | 433.4 MHz - 434 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 70cm | `amateur` | 435 MHz - 438 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 70cm-RepeaterTX | `amateur` | 439.15 MHz - 439.425 MHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 70cm Ham Band | `amateur` | 430 MHz - 440 MHz | `band` | `amateur` | `amateur` |
| `usa.json` | 70cm Ham Band | `amateur` | 420 MHz - 450 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:80m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 80m Ham Band | `amateur` | 3.5 MHz - 3.7 MHz | `band` | `amateur` | `amateur` |
| `australia.json` | 80m Ham Band | `amateur` | 3.776 MHz - 3.8 MHz | `band` | `amateur` | `amateur` |
| `austria.json` | 80m | `amateur` | 3.5 MHz - 3.8 MHz | `band` | `amateur` | `amateur` |
| `belgium.json` | 80m - Amateur | `amateur` | 3.5 MHz - 3.8 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | \|80m Ham Band CW | `amateur` | 3.5 MHz - 3.57 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, Digital | `amateur1` | 3.57 MHz - 3.59 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSD, AM, Digital | `amateur` | 3.59 MHz - 3.6 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSD, AM, DV, Digital | `amateur1` | 3.6 MHz - 3.775 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSD, DV, Digital | `amateur` | 3.775 MHz - 3.875 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSD, DV, Digital | `amateur1` | 3.775 MHz - 3.875 MHz | `band` | `amateur` | `amateur` |
| `brazil.json` | CW, SSD, AM, DV, Digital, 80m Ham Band\| | `amateur` | 3.875 MHz - 4 MHz | `band` | `amateur` | `amateur` |
| `canada.json` | 80m Ham Band | `amateur` | 3.5 MHz - 4 MHz | `band` | `amateur` | `amateur` |
| `china.json` | 80m Ham Band | `amateur` | 3.5 MHz - 3.9 MHz | `band` | `amateur` | `amateur` |
| `france.json` | 80m - Radioamateur | `amateur` | 3.6 MHz - 3.8 MHz | `band` | `amateur` | `amateur` |
| `general.json` | 80m Ham Band | `amateur` | 3.5 MHz - 3.95 MHz | `band` | `amateur` | `amateur` |
| `germany.json` | 80m-Amateur | `amateur` | 3.5 MHz - 3.8 MHz | `band` | `amateur` | `amateur` |
| `ireland.json` | 80m ham band | `amateur` | 3.5 MHz - 3.8 MHz | `band` | `amateur` | `amateur` |
| `italy.json` | Radioamatori 80m | `amateur` | 3.5 MHz - 3.8 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | \|80m Ham Band | `amateur` | 3.5 MHz - 3.57 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | RTTY/Data DX | `amateur1` | 3.57 MHz - 3.6 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | 80m Ham Band | `amateur` | 3.6 MHz - 3.79 MHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | DX window  80m Ham Band\| | `amateur1` | 3.79 MHz - 3.8 MHz | `band` | `amateur` | `amateur` |
| `republic-of-korea.json` | Amateur Station | `amateur` | 3.5 MHz - 3.55 MHz | `band` | `amateur` | `amateur` |
| `republic-of-korea.json` | Amateur Station | `amateur` | 3.79 MHz - 3.8 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 80m CW | `amateur` | 3.5 MHz - 3.51 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 80m CW Contest | `amateur` | 3.51 MHz - 3.56 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 80m CW | `amateur` | 3.56 MHz - 3.57 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 80m NB | `amateur` | 3.57 MHz - 3.6 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 80m SSB Contest | `amateur` | 3.6 MHz - 3.65 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 80m | `amateur` | 3.65 MHz - 3.7 MHz | `band` | `amateur` | `amateur` |
| `russia.json` | 80m SSB Contest | `amateur` | 3.7 MHz - 3.8 MHz | `band` | `amateur` | `amateur` |
| `slovakia.json` | 80m | `amateur` | 3.5 MHz - 3.8 MHz | `band` | `amateur` | `amateur` |
| `turkey.json` | 80m | `amateur` | 3.5 MHz - 3.8 MHz | `band` | `amateur` | `amateur` |
| `united-kingdom.json` | 80m Ham Band | `amateur` | 3.5 MHz - 3.8 MHz | `band` | `amateur` | `amateur` |
| `usa.json` | 80m Ham Band | `amateur` | 3.5 MHz - 4 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:8m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `belgium.json` | 8m - Amateur | `amateur` | 40.66 MHz - 40.69 MHz | `band` | `amateur` | `amateur` |

### `band:amateur:9cm`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 9cm Ham Band | `amateur` | 3.3 GHz - 3.4 GHz | `band` | `amateur` | `amateur` |
| `australia.json` | 9cm Ham Band (Restricted) | `amateur` | 3.4 GHz - 3.6 GHz | `band` | `amateur` | `amateur` |
| `brazil.json` | 9cm Ham Band | `amateur` | 3.4 GHz - 3.5 GHz | `band` | `amateur` | `amateur` |
| `china.json` | 9cm Ham Band | `amateur` | 3.3 GHz - 3.5 GHz | `band` | `amateur` | `amateur` |
| `france.json` | 9cm - Radioamateur | `amateur` | 3.4 GHz - 3.475 GHz | `band` | `amateur` | `amateur` |
| `germany.json` | 9cm-Amateur | `amateur` | 3.4 GHz - 3.475 GHz | `band` | `amateur` | `amateur` |
| `netherlands.json` | Radio Ham 9 cm band | `amateur` | 3.4 GHz - 3.475 GHz | `band` | `amateur` | `amateur` |

### `band:aviation:hf:10mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Aviation - HF | `aviation` | 10.005 MHz - 10.1 MHz | `band` | `aviation` | `aviation-communication` |
| `germany.json` | Aeronautical HF | `aviation` | 10.005 MHz - 10.1 MHz | `band` | `aviation` | `aviation-communication` |
| `italy.json` | Mobile aeronautico | `aviation` | 10.005 MHz - 10.1 MHz | `band` | `aviation` | `aviation-communication` |
| `netherlands.json` | Aeronautical | `aviation` | 10.005 MHz - 10.1 MHz | `band` | `aviation` | `aviation-communication` |
| `united-kingdom.json` | Aeronautical Mobile | `aviation` | 10.005 MHz - 10.1 MHz | `band` | `aviation` | `aviation-communication` |

### `band:aviation:hf:11mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Aviation - HF | `aviation` | 11.175 MHz - 11.4 MHz | `band` | `aviation` | `aviation-communication` |
| `germany.json` | Aeronautical HF | `aviation` | 11.175 MHz - 11.4 MHz | `band` | `aviation` | `aviation-communication` |
| `italy.json` | Mobile aeronautico | `aviation` | 11.175 MHz - 11.4 MHz | `band` | `aviation` | `aviation-communication` |
| `netherlands.json` | Aeronautical | `aviation` | 10.15 MHz - 11.407 MHz | `band` | `aviation` | `aviation-communication` |
| `republic-of-korea.json` | Aviation Mobile | `aviation` | 10.15 MHz - 11.6 MHz | `band` | `aviation` | `aviation-communication` |
| `united-kingdom.json` | Aeronautical Mobile | `aviation` | 11.175 MHz - 11.4 MHz | `band` | `aviation` | `aviation-communication` |

### `band:aviation:hf:13mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Aviation - HF | `aviation` | 13.2 MHz - 13.36 MHz | `band` | `aviation` | `aviation-communication` |
| `germany.json` | Aeronautical HF | `aviation` | 13.2 MHz - 13.36 MHz | `band` | `aviation` | `aviation-communication` |
| `italy.json` | Mobile aeronautico | `aviation` | 13.2 MHz - 13.36 MHz | `band` | `aviation` | `aviation-communication` |
| `netherlands.json` | (Transoceanic Flights) Aeronautical and (Ship/Shore) Maritime | `aviation` | 12.7919 MHz - 13.36 MHz | `band` | `aviation` | `aviation-communication` |
| `republic-of-korea.json` | Aviation Mobile | `aviation` | 13.26 MHz - 13.36 MHz | `band` | `aviation` | `aviation-communication` |
| `united-kingdom.json` | Aeronautical Mobile | `aviation` | 13.2 MHz - 13.36 MHz | `band` | `aviation` | `aviation-communication` |

### `band:aviation:hf:15mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Aviation - HF | `aviation` | 15.01 MHz - 15.1 MHz | `band` | `aviation` | `aviation-communication` |
| `germany.json` | Aeronautical HF | `aviation` | 15.01 MHz - 15.1 MHz | `band` | `aviation` | `aviation-communication` |
| `italy.json` | Mobile aeronautico | `aviation` | 15.01 MHz - 15.1 MHz | `band` | `aviation` | `aviation-communication` |
| `netherlands.json` | (Transoceanic Flights) Aeronautical Mobile | `aviation` | 15.005 MHz - 15.1 MHz | `band` | `aviation` | `aviation-communication` |
| `republic-of-korea.json` | Aviation Mobile | `aviation` | 15.01 MHz - 15.1 MHz | `band` | `aviation` | `aviation-communication` |
| `united-kingdom.json` | Aeronautical Mobile | `aviation` | 15.01 MHz - 15.1 MHz | `band` | `aviation` | `aviation-communication` |

### `band:aviation:hf:17mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Aviation - HF | `aviation` | 17.9 MHz - 18.03 MHz | `band` | `aviation` | `aviation-communication` |
| `italy.json` | Mobile aeronautico | `aviation` | 17.9 MHz - 18.03 MHz | `band` | `aviation` | `aviation-communication` |
| `netherlands.json` | (Transoceanic Flights) Aeronautical | `aviation` | 17.9 MHz - 18.068 MHz | `band` | `aviation` | `aviation-communication` |
| `united-kingdom.json` | Aeronautical Mobile | `aviation` | 17.9 MHz - 18.03 MHz | `band` | `aviation` | `aviation-communication` |

### `band:aviation:hf:22mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Aviation - HF | `aviation` | 21.925 MHz - 22 MHz | `band` | `aviation` | `aviation-communication` |
| `germany.json` | Aeronautical HF | `aviation` | 21.87 MHz - 22 MHz | `band` | `aviation` | `aviation-communication` |
| `italy.json` | Mobile aeronautico | `aviation` | 21.925 MHz - 22 MHz | `band` | `aviation` | `aviation-communication` |
| `netherlands.json` | Aeronautical Mobile | `aviation` | 21.85 MHz - 22 MHz | `band` | `aviation` | `aviation-communication` |
| `united-kingdom.json` | Aeronautical Mobile | `aviation` | 21.87 MHz - 22 MHz | `band` | `aviation` | `aviation-communication` |

### `band:aviation:hf:23mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Aviation - HF | `aviation` | 23.2 MHz - 23.35 MHz | `band` | `aviation` | `aviation-communication` |
| `germany.json` | Aeronautical HF | `aviation` | 23.2 MHz - 23.35 MHz | `band` | `aviation` | `aviation-communication` |
| `italy.json` | Mobile aeronautico | `aviation` | 23.2 MHz - 23.35 MHz | `band` | `aviation` | `aviation-communication` |
| `united-kingdom.json` | Aeronautical Mobile | `aviation` | 23.2 MHz - 23.35 MHz | `band` | `aviation` | `aviation-communication` |

### `band:aviation:hf:2mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Aviation - HF | `aviation` | 2.85 MHz - 3.155 MHz | `band` | `aviation` | `aviation-communication` |
| `germany.json` | Aeronautical HF | `aviation` | 2.85 MHz - 3.155 MHz | `band` | `aviation` | `aviation-communication` |
| `italy.json` | Mobile aeronautico | `aviation` | 2.85 MHz - 3.155 MHz | `band` | `aviation` | `aviation-communication` |
| `netherlands.json` | Transoceanic Flights (Aeronautical Mobile) | `aviation` | 2.85 MHz - 3.155 MHz | `band` | `aviation` | `aviation-communication` |
| `republic-of-korea.json` | Aviation Mobile R | `aviation` | 2.85 MHz - 3.025 MHz | `band` | `aviation` | `aviation-communication` |
| `republic-of-korea.json` | Aviation Mobile OR | `aviation` | 3.025 MHz - 3.155 MHz | `band` | `aviation` | `aviation-communication` |
| `united-kingdom.json` | Aeronautical Mobile | `aviation` | 2.85 MHz - 3.155 MHz | `band` | `aviation` | `aviation-communication` |

### `band:aviation:hf:3.4mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Aviation - HF | `aviation` | 3.4 MHz - 3.5 MHz | `band` | `aviation` | `aviation-communication` |
| `germany.json` | Aeronautical HF | `aviation` | 3.4 MHz - 3.5 MHz | `band` | `aviation` | `aviation-communication` |
| `italy.json` | Mobile aviazione | `aviation` | 3.4 MHz - 3.5 MHz | `band` | `aviation` | `aviation-communication` |
| `netherlands.json` | Aeronautical | `aviation` | 3.4 MHz - 3.5 MHz | `band` | `aviation` | `aviation-communication` |
| `republic-of-korea.json` | Aviation Mobile R | `aviation` | 3.4 MHz - 3.5 MHz | `band` | `aviation` | `aviation-communication` |
| `united-kingdom.json` | Aeronautical Mobile | `aviation` | 3.4 MHz - 3.5 MHz | `band` | `aviation` | `aviation-communication` |

### `band:aviation:hf:3.8mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `netherlands.json` | Aeronautical Mobile Service | `aviation` | 3.8 MHz - 3.95 MHz | `band` | `aviation` | `aviation-communication` |

### `band:aviation:hf:4mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Aviation - HF | `aviation` | 4.65 MHz - 4.75 MHz | `band` | `aviation` | `aviation-communication` |
| `germany.json` | Aeronautical HF | `aviation` | 4.65 MHz - 4.75 MHz | `band` | `aviation` | `aviation-communication` |
| `italy.json` | Mobile aeronautico | `aviation` | 4.65 MHz - 4.75 MHz | `band` | `aviation` | `aviation-communication` |
| `republic-of-korea.json` | Aviation Mobile R | `aviation` | 4.65 MHz - 4.85 MHz | `band` | `aviation` | `aviation-communication` |
| `united-kingdom.json` | Aeronautical Mobile | `aviation` | 4.65 MHz - 4.75 MHz | `band` | `aviation` | `aviation-communication` |

### `band:aviation:hf:5mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Aviation - HF | `aviation` | 5.45 MHz - 5.73 MHz | `band` | `aviation` | `aviation-communication` |
| `germany.json` | Aeronautical HF | `aviation` | 5.48 MHz - 5.73 MHz | `band` | `aviation` | `aviation-communication` |
| `italy.json` | Mobile aeronautico | `aviation` | 5.45 MHz - 5.73 MHz | `band` | `aviation` | `aviation-communication` |
| `netherlands.json` | Aeronautical/ Maritime | `aviation` | 5.4064 MHz - 5.9 MHz | `band` | `aviation` | `aviation-communication` |
| `republic-of-korea.json` | Search Rescue | `aviation` | 5.48 MHz - 5.73 MHz | `band` | `aviation` | `aviation-communication` |
| `united-kingdom.json` | Aeronautical Mobile | `aviation` | 5.45 MHz - 5.73 MHz | `band` | `aviation` | `aviation-communication` |

### `band:aviation:hf:6mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Aviation - HF | `aviation` | 6.525 MHz - 6.765 MHz | `band` | `aviation` | `aviation-communication` |
| `germany.json` | Aeronautical HF | `aviation` | 6.525 MHz - 6.765 MHz | `band` | `aviation` | `aviation-communication` |
| `netherlands.json` | (Transoceanic Flights) Aeronautical and (Ship/Shore) Maritime | `aviation` | 6.3425 MHz - 6.765 MHz | `band` | `aviation` | `aviation-communication` |
| `united-kingdom.json` | Aeronautical Mobile | `aviation` | 6.525 MHz - 6.765 MHz | `band` | `aviation` | `aviation-communication` |

### `band:aviation:hf:8mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Aviation - HF | `aviation` | 8.815 MHz - 9.04 MHz | `band` | `aviation` | `aviation-communication` |
| `germany.json` | Aeronautical HF | `aviation` | 8.815 MHz - 9.04 MHz | `band` | `aviation` | `aviation-communication` |
| `italy.json` | Mobile aeronautico | `aviation` | 8.815 MHz - 9.04 MHz | `band` | `aviation` | `aviation-communication` |
| `netherlands.json` | (Transoceanic Flights) Aeronautical and (Ship/Shore) Maritime | `marine1` | 8.68 MHz - 9.108 MHz | `band` | `aviation` | `aviation-communication` |
| `united-kingdom.json` | Aeronautical Mobile | `aviation` | 8.815 MHz - 9.04 MHz | `band` | `aviation` | `aviation-communication` |

### `band:aviation:l-band`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `italy.json` | ADS-B, DME, TACAN | `aviation` | 960 MHz - 1.164 GHz | `band` | `aviation` | `aviation-surveillance` |

### `band:aviation:vhf-voice`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Air Band Voice | `aviation` | 117.975 MHz - 137 MHz | `band` | `aviation` | `aviation-communication` |
| `canada.json` | Air Band Voice | `aviation` | 117 MHz - 137 MHz | `band` | `aviation` | `aviation-communication` |
| `china.json` | Air Band Voice | `aviation` | 117.975 MHz - 137 MHz | `band` | `aviation` | `aviation-communication` |
| `france.json` | Aviation - Voix | `aviation` | 118 MHz - 137 MHz | `band` | `aviation` | `aviation-communication` |
| `general.json` | Air Band Voice | `aviation` | 118 MHz - 137 MHz | `band` | `aviation` | `aviation-communication` |
| `germany.json` | Air Band Voice | `aviation` | 118 MHz - 136.7 MHz | `band` | `aviation` | `aviation-communication` |
| `germany.json` | Air Band CPDLC/Datalink | `aviation` | 136.7 MHz - 137 MHz | `band` | `aviation` | `aviation-communication` |
| `ireland.json` | Airband Voice | `aviation` | 118 MHz - 137 MHz | `band` | `aviation` | `aviation-communication` |
| `italy.json` | Mobile aeronautico | `aviation` | 117.975 MHz - 137 MHz | `band` | `aviation` | `aviation-communication` |
| `netherlands.json` | Air Band Voice | `aviation` | 118 MHz - 121.49 MHz | `band` | `aviation` | `aviation-communication` |
| `netherlands.json` | Civilian Aircraft Distress/ Emergency | `aviation` | 121.49 MHz - 121.51 MHz | `band` | `aviation` | `aviation-communication` |
| `netherlands.json` | Air Band Voice | `aviation` | 121.51 MHz - 131.545 MHz | `band` | `aviation` | `aviation-communication` |
| `netherlands.json` | ACARS | `aviation` | 131.545 MHz - 131.555 MHz | `band` | `aviation` | `aviation-communication` |
| `netherlands.json` | Aviation | `aviation` | 131.555 MHz - 137 MHz | `band` | `aviation` | `aviation-communication` |
| `russia.json` | Air Band Voice | `aviation` | 118 MHz - 137 MHz | `band` | `aviation` | `aviation-communication` |
| `turkey.json` | Airband Voice | `aviation` | 117.975 MHz - 137 MHz | `band` | `aviation` | `aviation-communication` |
| `united-kingdom.json` | Air Band Voice | `aviation` | 117.975 MHz - 137 MHz | `band` | `aviation` | `aviation-communication` |
| `usa.json` | Air Band Voice | `aviation` | 118 MHz - 137 MHz | `band` | `aviation` | `aviation-communication` |

### `band:broadcast:11m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Shortwave Broadcast | `broadcast` | 25.67 MHz - 26.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `belgium.json` | AM Broadcast | `broadcast` | 25.67 MHz - 26.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `canada.json` | Shortwave Broadcast | `broadcast` | 25.67 MHz - 26.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | Shortwave Broadcast | `broadcast` | 25.67 MHz - 26.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | 11m - radiodiffusion | `broadcast` | 25.67 MHz - 26.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Shortwave Broadcast | `broadcast` | 25.67 MHz - 26.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | 11m-Broadcast | `broadcast` | 25.67 MHz - 26.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | 11m SW Broadcast | `broadcast` | 25.67 MHz - 26.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OC 11m | `broadcast` | 25.67 MHz - 26.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Shortwave (11m) Broadcast | `broadcast` | 25.67 MHz - 26.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Shortwave Broadcast | `broadcast` | 25.67 MHz - 26.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `russia.json` | SW 11m | `broadcast` | 25.65 MHz - 26.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | Shortwave Broadcast | `broadcast` | 25.67 MHz - 26.1 MHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:120m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Shortwave Broadcast | `broadcast` | 2.3 MHz - 2.468 MHz | `band` | `broadcast` | `sound-broadcast` |
| `canada.json` | Shortwave Broadcast | `broadcast` | 2.3 MHz - 2.468 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | Shortwave Broadcast | `broadcast` | 2.3 MHz - 2.495 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | 120m - radiodiffusion | `broadcast` | 2.3 MHz - 2.5 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Shortwave Broadcast | `broadcast` | 2.3 MHz - 2.468 MHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | 120m-Broadcast | `broadcast` | 2.3 MHz - 2.495 MHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | 120m SW broadcast | `broadcast` | 2.3 MHz - 2.495 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OC 120m | `broadcast` | 2.3 MHz - 2.5 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Shortwave (Tropical Band 120m) Broadcast | `broadcast` | 2.3 MHz - 2.498 MHz | `band` | `broadcast` | `sound-broadcast` |
| `united-kingdom.json` | 120m Broadcast | `broadcast` | 2.3 MHz - 2.495 MHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | Shortwave Broadcast | `broadcast` | 2.3 MHz - 2.468 MHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:13m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Shortwave Broadcast | `broadcast` | 21.45 MHz - 21.85 MHz | `band` | `broadcast` | `sound-broadcast` |
| `belgium.json` | AM Broadcast | `broadcast` | 21.45 MHz - 21.85 MHz | `band` | `broadcast` | `sound-broadcast` |
| `canada.json` | Shortwave Broadcast | `broadcast` | 21.45 MHz - 21.85 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | Shortwave Broadcast | `broadcast` | 21.45 MHz - 21.85 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | 13 mètres - radiodiffusion | `broadcast` | 21.45 MHz - 21.85 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Shortwave Broadcast | `broadcast` | 21.45 MHz - 21.85 MHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | 13m-Broadcast | `broadcast` | 21.45 MHz - 21.85 MHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | 13m SW Broadcast | `broadcast` | 21.45 MHz - 21.85 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OC 13m | `broadcast` | 21.45 MHz - 21.85 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Shortwave (13m) Broadcast | `broadcast` | 21.45 MHz - 21.85 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Shortwave Broadcast | `broadcast` | 21.45 MHz - 21.85 MHz | `band` | `broadcast` | `sound-broadcast` |
| `russia.json` | SW 13m | `broadcast` | 21.45 MHz - 21.85 MHz | `band` | `broadcast` | `sound-broadcast` |
| `united-kingdom.json` | 13m Broadcast | `broadcast` | 21.45 MHz - 21.85 MHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | Shortwave Broadcast | `broadcast` | 21.45 MHz - 21.85 MHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:15m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Shortwave Broadcast | `broadcast` | 18.9 MHz - 19.02 MHz | `band` | `broadcast` | `sound-broadcast` |
| `canada.json` | Shortwave Broadcast | `broadcast` | 18.9 MHz - 19.02 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | Shortwave Broadcast | `broadcast` | 18.9 MHz - 19.02 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | 15m - radiodiffusion | `broadcast` | 18.9 MHz - 19.02 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Shortwave Broadcast | `broadcast` | 18.9 MHz - 19.02 MHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | 15m-Broadcast | `broadcast` | 18.9 MHz - 19.02 MHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | 15m SW Broadcast | `broadcast` | 18.9 MHz - 19.02 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OC 15m | `broadcast` | 18.9 MHz - 19.02 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Shortwave (15m) Broadcast | `broadcast` | 18.9 MHz - 19.02 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Broadcast | `broadcast` | 18.9 MHz - 19.02 MHz | `band` | `broadcast` | `sound-broadcast` |
| `united-kingdom.json` | 15m Broadcast | `broadcast` | 18.9 MHz - 19.02 MHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | Shortwave Broadcast | `broadcast` | 18.9 MHz - 19.02 MHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:16m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Shortwave Broadcast | `broadcast` | 17.48 MHz - 17.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `belgium.json` | AM Broadcast | `broadcast` | 17.55 MHz - 17.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `canada.json` | Shortwave Broadcast | `broadcast` | 17.48 MHz - 17.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | Shortwave Broadcast | `broadcast` | 17.48 MHz - 17.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | 16m - radiodiffusion | `broadcast` | 17.48 MHz - 17.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Shortwave Broadcast | `broadcast` | 17.48 MHz - 17.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | 16m-Broadcast | `broadcast` | 17.48 MHz - 17.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | 16m SW Broadcast | `broadcast` | 17.48 MHz - 17.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OC 16m | `broadcast` | 17.48 MHz - 17.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Shortwave (16m) Broadcast | `broadcast` | 17.48 MHz - 17.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `russia.json` | SW 16m | `broadcast` | 17.55 MHz - 17.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `united-kingdom.json` | 16m Broadcast | `broadcast` | 17.48 MHz - 17.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | Shortwave Broadcast | `broadcast` | 17.48 MHz - 17.9 MHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:19m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Shortwave Broadcast | `broadcast` | 15.1 MHz - 15.8 MHz | `band` | `broadcast` | `sound-broadcast` |
| `belgium.json` | AM Broadcast | `broadcast` | 15.1 MHz - 15.6 MHz | `band` | `broadcast` | `sound-broadcast` |
| `canada.json` | Shortwave Broadcast | `broadcast` | 15.1 MHz - 15.8 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | Shortwave Broadcast | `broadcast` | 15.1 MHz - 15.8 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | 19m - radiodiffusion | `broadcast` | 15.1 MHz - 15.8 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Shortwave Broadcast | `broadcast` | 15.1 MHz - 15.8 MHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | 19m-Broadcast | `broadcast` | 15.1 MHz - 15.8 MHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | 19m SW Broadcast | `broadcast` | 15.1 MHz - 15.8 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OC 19m | `broadcast` | 15.1 MHz - 15.8 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Shortwave (19m) Broadcast | `broadcast` | 15.1 MHz - 15.8 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Shortwave Broadcast | `broadcast` | 15.1 MHz - 15.6 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Shortwave Broadcast | `broadcast` | 15.6 MHz - 15.8 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Shortwave Broadcast | `broadcast` | 15.8 MHz - 15.995 MHz | `band` | `broadcast` | `sound-broadcast` |
| `russia.json` | SW 19m | `broadcast` | 15.1 MHz - 15.6 MHz | `band` | `broadcast` | `sound-broadcast` |
| `united-kingdom.json` | 19m Broadcast | `broadcast` | 15.1 MHz - 15.8 MHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | Shortwave Broadcast | `broadcast` | 15.1 MHz - 15.8 MHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:22m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Shortwave Broadcast | `broadcast` | 13.57 MHz - 13.87 MHz | `band` | `broadcast` | `sound-broadcast` |
| `belgium.json` | AM Broadcast | `broadcast` | 13.6 MHz - 13.8 MHz | `band` | `broadcast` | `sound-broadcast` |
| `canada.json` | Shortwave Broadcast | `broadcast` | 13.57 MHz - 13.87 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | Shortwave Broadcast | `broadcast` | 13.57 MHz - 13.87 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | 21m - radiodiffusion | `broadcast` | 13.57 MHz - 13.87 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Shortwave Broadcast | `broadcast` | 13.57 MHz - 13.87 MHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | 22m-Broadcast | `broadcast` | 13.57 MHz - 13.87 MHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | 22m SW Broadcast | `broadcast` | 13.57 MHz - 13.87 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OC 21m | `broadcast` | 13.57 MHz - 13.87 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Shortwave (22m) Broadcast | `broadcast` | 13.57 MHz - 13.87 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Broadcast | `broadcast` | 13.57 MHz - 13.6 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Shortwave Broadcast | `broadcast` | 13.6 MHz - 13.8 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Broadcast | `broadcast` | 13.8 MHz - 13.87 MHz | `band` | `broadcast` | `sound-broadcast` |
| `russia.json` | SW 22m | `broadcast` | 13.6 MHz - 13.8 MHz | `band` | `broadcast` | `sound-broadcast` |
| `united-kingdom.json` | 22m Broadcast | `broadcast` | 13.57 MHz - 13.87 MHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | Shortwave Broadcast | `broadcast` | 13.57 MHz - 13.87 MHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:25m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Shortwave Broadcast | `broadcast` | 11.6 MHz - 12.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `belgium.json` | AM Broadcast | `broadcast` | 11.65 MHz - 12.05 MHz | `band` | `broadcast` | `sound-broadcast` |
| `canada.json` | Shortwave Broadcast | `broadcast` | 11.6 MHz - 12.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | Shortwave Broadcast | `broadcast` | 11.6 MHz - 12.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | 25m - radiodiffusion | `broadcast` | 11.6 MHz - 12.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Shortwave Broadcast | `broadcast` | 11.6 MHz - 12.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | 25m-Broadcast | `broadcast` | 11.6 MHz - 12.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | 25m SW Broadcast | `broadcast` | 11.6 MHz - 12.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OC 25m | `broadcast` | 11.6 MHz - 12.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Shortwave (25m) Broadcast | `broadcast` | 11.6 MHz - 12.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Broadcast | `broadcast` | 11.6 MHz - 11.65 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Shortwave Broadcast | `broadcast` | 11.65 MHz - 12.05 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Broadcast | `broadcast` | 12.05 MHz - 12.1 MHz | `band` | `broadcast` | `sound-broadcast` |
| `russia.json` | SW 25m | `broadcast` | 11.65 MHz - 12.05 MHz | `band` | `broadcast` | `sound-broadcast` |
| `united-kingdom.json` | 25m Broadcast | `broadcast` | 11.6 MHz - 12.23 MHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | Shortwave Broadcast | `broadcast` | 11.6 MHz - 12.1 MHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:31m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Shortwave Broadcast | `broadcast` | 9.4 MHz - 9.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `belgium.json` | AM Broadcast | `broadcast` | 9.5 MHz - 9.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `canada.json` | Shortwave Broadcast | `broadcast` | 9.4 MHz - 9.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | Shortwave Broadcast | `broadcast` | 9.4 MHz - 9.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | 31m - radiodiffusion | `broadcast` | 9.4 MHz - 9.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Shortwave Broadcast | `broadcast` | 9.4 MHz - 9.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | 31m-Broadcast | `broadcast` | 9.4 MHz - 9.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | 31m SW Broadcast | `broadcast` | 9.4 MHz - 9.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OC 31m | `broadcast` | 9.4 MHz - 9.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Shortwave (31m) Broadcast | `broadcast` | 9.4 MHz - 9.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Broadcast | `broadcast` | 9.4 MHz - 9.5 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Shortwave Broadcast | `broadcast` | 9.5 MHz - 9.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `russia.json` | SW 31m | `broadcast` | 9.5 MHz - 9.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `united-kingdom.json` | 31m Broadcast | `broadcast` | 9.4 MHz - 9.9 MHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | Shortwave Broadcast | `broadcast` | 9.4 MHz - 9.9 MHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:41m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Shortwave Broadcast | `broadcast` | 7.2 MHz - 7.45 MHz | `band` | `broadcast` | `sound-broadcast` |
| `belgium.json` | AM Broadcast | `broadcast` | 7.2 MHz - 7.3 MHz | `band` | `broadcast` | `sound-broadcast` |
| `canada.json` | Shortwave Broadcast | `broadcast` | 7.3 MHz - 7.45 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | Shortwave Broadcast | `broadcast` | 7.2 MHz - 7.35 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | 41m - radiodiffusion | `broadcast` | 7.2 MHz - 7.45 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Shortwave Broadcast | `broadcast` | 7.2 MHz - 7.45 MHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | 41m-Broadcast | `broadcast` | 7.2 MHz - 7.45 MHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | 40m SW Broadcast | `broadcast` | 7.2 MHz - 7.45 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OC 41m | `broadcast` | 7.2 MHz - 7.45 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Shortwave (41m) Broadcast | `broadcast` | 7.2 MHz - 7.45 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Shortwave Broadcast | `broadcast` | 7.2 MHz - 7.45 MHz | `band` | `broadcast` | `sound-broadcast` |
| `united-kingdom.json` | 41m Broadcast | `broadcast` | 7.2 MHz - 7.45 MHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | Shortwave Broadcast | `broadcast` | 7.3 MHz - 7.45 MHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:49m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Shortwave Broadcast | `broadcast` | 5.9 MHz - 6.2 MHz | `band` | `broadcast` | `sound-broadcast` |
| `belgium.json` | AM Broadcast | `broadcast` | 5.95 MHz - 6.2 MHz | `band` | `broadcast` | `sound-broadcast` |
| `canada.json` | Shortwave Broadcast | `broadcast` | 5.9 MHz - 6.2 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | Shortwave Broadcast | `broadcast` | 5.9 MHz - 6.2 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | 49m - radiodiffusion | `broadcast` | 5.9 MHz - 6.2 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Shortwave Broadcast | `broadcast` | 5.9 MHz - 6.2 MHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | 49m-Broadcast | `broadcast` | 5.9 MHz - 6.2 MHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | 49m SW Broadcast | `broadcast` | 5.9 MHz - 6.2 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OC 49m | `broadcast` | 5.9 MHz - 6.2 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Shortwave (49m) Broadcast | `broadcast` | 5.9 MHz - 6.2 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Broadcast | `broadcast` | 5.9 MHz - 5.95 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Shortwave Broadcast | `broadcast` | 5.95 MHz - 6.2 MHz | `band` | `broadcast` | `sound-broadcast` |
| `russia.json` | SW 49m | `broadcast` | 5.9 MHz - 6.2 MHz | `band` | `broadcast` | `sound-broadcast` |
| `united-kingdom.json` | 49m Broadcast | `broadcast` | 5.9 MHz - 6.2 MHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | Shortwave Broadcast | `broadcast` | 5.9 MHz - 6.2 MHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:60m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Shortwave Broadcast | `broadcast` | 4.75 MHz - 4.995 MHz | `band` | `broadcast` | `sound-broadcast` |
| `australia.json` | Shortwave Broadcast | `broadcast` | 5.005 MHz - 5.06 MHz | `band` | `broadcast` | `sound-broadcast` |
| `canada.json` | Shortwave Broadcast | `broadcast` | 4.75 MHz - 5.06 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | Shortwave Broadcast | `broadcast` | 4.75 MHz - 4.995 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | Shortwave Broadcast | `broadcast` | 5.005 MHz - 5.06 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | 60m - radiodiffusion | `broadcast` | 4.75 MHz - 4.995 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | 60m - radiodiffusion | `broadcast` | 5.005 MHz - 5.06 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Shortwave Broadcast | `broadcast` | 4.75 MHz - 4.995 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Shortwave Broadcast | `broadcast` | 5.005 MHz - 5.06 MHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | 60m-Broadcast | `broadcast` | 4.75 MHz - 5.06 MHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | 60m SW Broadcast | `broadcast` | 4.75 MHz - 5.06 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OC 60m | `broadcast` | 4.75 MHz - 4.995 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OC 60m | `broadcast` | 5.005 MHz - 5.06 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Shortwave (Tropical Band 60m) Broadcast | `broadcast` | 4.75 MHz - 4.995 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Shortwave (Tropical Band 60m) Broadcast | `broadcast` | 5.005 MHz - 5.06 MHz | `band` | `broadcast` | `sound-broadcast` |
| `russia.json` | SW 60m | `broadcast` | 4.75 MHz - 5.06 MHz | `band` | `broadcast` | `sound-broadcast` |
| `united-kingdom.json` | 60m Broadcast | `broadcast` | 4.75 MHz - 4.995 MHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | Shortwave Broadcast | `broadcast` | 4.75 MHz - 4.995 MHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | Shortwave Broadcast | `broadcast` | 5.005 MHz - 5.06 MHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:75m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Shortwave Broadcast | `broadcast` | 3.95 MHz - 4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | Shortwave Broadcast | `broadcast` | 3.9 MHz - 4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | 75m - radiodiffusion | `broadcast` | 3.9 MHz - 4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Shortwave Broadcast | `broadcast` | 3.95 MHz - 4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | 75m-Broadcast | `broadcast` | 3.9 MHz - 4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | 75m SW Broadcast | `broadcast` | 3.9 MHz - 4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OC 75m | `broadcast` | 3.9 MHz - 4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Shortwave (75m) Broadcast | `broadcast` | 3.95 MHz - 4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Shortwave Broadcast | `broadcast` | 3.9 MHz - 3.95 MHz | `band` | `broadcast` | `sound-broadcast` |
| `russia.json` | SW 75m | `broadcast` | 3.9 MHz - 4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `united-kingdom.json` | 75m Broadcast | `broadcast` | 3.9 MHz - 4 MHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:90m`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Shortwave Broadcast | `broadcast` | 3.2 MHz - 3.4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `canada.json` | Shortwave Broadcast | `broadcast` | 3.2 MHz - 3.4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | Shortwave Broadcast | `broadcast` | 3.2 MHz - 3.4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | 90m - radiodiffusion | `broadcast` | 3.2 MHz - 3.4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Shortwave Broadcast | `broadcast` | 3.2 MHz - 3.4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | 90m-Broadcast | `broadcast` | 3.2 MHz - 3.4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | 90m SW Broadcast | `broadcast` | 3.2 MHz - 3.4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OC 90m | `broadcast` | 3.2 MHz - 3.4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Shortwave (Tropical Band 90m) Broadcast | `broadcast` | 3.2 MHz - 3.4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `russia.json` | SW 90m | `broadcast` | 3.2 MHz - 3.4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `united-kingdom.json` | 90m Broadcast | `broadcast` | 3.2 MHz - 3.4 MHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | Shortwave Broadcast | `broadcast` | 3.2 MHz - 3.4 MHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:fm`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | FM Broadcast | `broadcast` | 87.5 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |
| `austria.json` | FM | `broadcast` | 87.5 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |
| `belgium.json` | FM Broadcast | `broadcast` | 87.5 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |
| `canada.json` | FM Broadcast | `broadcast` | 76 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | FM Broadcast | `broadcast` | 76 MHz - 84 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | FM Broadcast | `broadcast` | 87 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | Radiodiffusion - Bande FM | `broadcast` | 80 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | FM Broadcast | `broadcast` | 87.5 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | FM-Broadcast | `broadcast` | 87.5 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | FM Broadcast | `broadcast` | 87.5 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione FM | `broadcast` | 87.5 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | FM Broadcast | `broadcast` | 87.5 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | FM Broadcast | `broadcast` | 88 MHz - 100 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | FM Broadcast | `broadcast` | 100 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |
| `russia.json` | Broadcast FM(OIRT) | `broadcast` | 65.9 MHz - 74 MHz | `band` | `broadcast` | `sound-broadcast` |
| `russia.json` | Broadcast FM(CCIR) | `broadcast` | 87.5 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |
| `slovakia.json` | FM | `broadcast` | 87.5 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |
| `turkey.json` | FM | `broadcast` | 87.5 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |
| `united-kingdom.json` | FM Broadcast | `broadcast` | 87.5 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | FM Broadcast | `broadcast` | 87.5 MHz - 108 MHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:longwave`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Grande ondes | `broadcast` | 148.5 kHz - 519 kHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Long Wave | `broadcast` | 148.5 kHz - 283.5 kHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | LW-Broadcast | `broadcast` | 148.5 kHz - 283.5 kHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | Long wave | `broadcast` | 148.5 kHz - 282.5 kHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OL | `broadcast` | 148.5 kHz - 283.5 kHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Long Wave | `broadcast` | 148.5 kHz - 255 kHz | `band` | `broadcast` | `sound-broadcast` |
| `russia.json` | LW | `broadcast` | 144 kHz - 415 kHz | `band` | `broadcast` | `sound-broadcast` |
| `united-kingdom.json` | Long Wave | `broadcast` | 148.5 kHz - 283.5 kHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | Long Wave | `broadcast` | 148.5 kHz - 519 kHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:mediumwave`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Medium Wave Broadcast (AM Broadcast) | `broadcast` | 520 kHz - 1.71 MHz | `band` | `broadcast` | `sound-broadcast` |
| `belgium.json` | AM Broadcast | `broadcast` | 526.5 kHz - 1.6065 MHz | `band` | `broadcast` | `sound-broadcast` |
| `canada.json` | Mediumwave Broadcast | `broadcast` | 530 kHz - 1.7 MHz | `band` | `broadcast` | `sound-broadcast` |
| `china.json` | Medium Wave Broadcast | `broadcast` | 526.5 kHz - 1.6065 MHz | `band` | `broadcast` | `sound-broadcast` |
| `france.json` | Ondes moyennes (AM Broadcast) | `broadcast` | 520 kHz - 1.705 MHz | `band` | `broadcast` | `sound-broadcast` |
| `general.json` | Medium Wave | `broadcast` | 526.5 kHz - 1.6065 MHz | `band` | `broadcast` | `sound-broadcast` |
| `germany.json` | MW-Broadcast | `broadcast` | 526.5 kHz - 1.6065 MHz | `band` | `broadcast` | `sound-broadcast` |
| `ireland.json` | AM broadcast | `broadcast` | 531 kHz - 1.602 MHz | `band` | `broadcast` | `sound-broadcast` |
| `italy.json` | Radiodiffusione OM | `broadcast` | 520 kHz - 1.6065 MHz | `band` | `broadcast` | `sound-broadcast` |
| `netherlands.json` | Medium Wave | `broadcast` | 526.5 kHz - 1.6065 MHz | `band` | `broadcast` | `sound-broadcast` |
| `republic-of-korea.json` | Standard Broadcast | `broadcast` | 526.5 kHz - 1.6065 MHz | `band` | `broadcast` | `sound-broadcast` |
| `russia.json` | MW | `broadcast` | 520 kHz - 1.602 MHz | `band` | `broadcast` | `sound-broadcast` |
| `united-kingdom.json` | Medium Wave (AM Broadcast) | `broadcast` | 526.5 kHz - 1.6065 MHz | `band` | `broadcast` | `sound-broadcast` |
| `usa.json` | Medium Wave (AM Broadcast) | `broadcast` | 525 kHz - 1.705 MHz | `band` | `broadcast` | `sound-broadcast` |

### `band:broadcast:television:uhf`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Digital TV Broadcast (DVB-T) | `broadcast` | 526 MHz - 694 MHz | `band` | `broadcast` | `television-broadcast` |
| `belgium.json` | DVB-T - Broadcast | `broadcast` | 470 MHz - 790 MHz | `band` | `broadcast` | `television-broadcast` |
| `canada.json` | TV Channels 14-36 | `broadcast` | 470 MHz - 608 MHz | `band` | `broadcast` | `television-broadcast` |
| `canada.json` | TV Channels 38-51 | `broadcast` | 614 MHz - 806 MHz | `band` | `broadcast` | `television-broadcast` |
| `france.json` | TNT (DVB-T) | `broadcast` | 470 MHz - 694 MHz | `band` | `broadcast` | `television-broadcast` |
| `germany-mobile-networks.json` | DVB-T2 | `broadcast` | 470 MHz - 694 MHz | `band` | `broadcast` | `television-broadcast` |
| `germany.json` | DVB-T2 (TV) | `broadcast` | 470 MHz - 690 MHz | `band` | `broadcast` | `television-broadcast` |
| `italy.json` | TV UHF | `broadcast` | 470 MHz - 791 MHz | `band` | `broadcast` | `television-broadcast` |
| `netherlands.json` | DVB-T Television UHF | `broadcast` | 470 MHz - 608 MHz | `band` | `broadcast` | `television-broadcast` |
| `netherlands.json` | DVB-T Television UHF | `broadcast` | 614 MHz - 790 MHz | `band` | `broadcast` | `television-broadcast` |
| `russia.json` | UHF TV | `broadcast` | 470 MHz - 790 MHz | `band` | `broadcast` | `television-broadcast` |
| `united-kingdom.json` | Digital TV Broadcast | `broadcast` | 470 MHz - 700 MHz | `band` | `broadcast` | `television-broadcast` |
| `usa.json` | TV Channels 14-20 | `broadcast` | 470 MHz - 512 MHz | `band` | `broadcast` | `television-broadcast` |
| `usa.json` | TV Channels 21-36 | `broadcast` | 512 MHz - 608 MHz | `band` | `broadcast` | `television-broadcast` |
| `usa.json` | TV Broadcasting | `broadcast` | 614 MHz - 698 MHz | `band` | `broadcast` | `television-broadcast` |

### `band:broadcast:television:vhf-high`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Digital TV Broadcast (DVB-T) | `broadcast` | 174 MHz - 195 MHz | `band` | `broadcast` | `television-broadcast` |
| `australia.json` | Digital TV Broadcast (DVB-T) | `broadcast` | 209 MHz - 230 MHz | `band` | `broadcast` | `television-broadcast` |
| `canada.json` | TV Channels 7-13 | `broadcast` | 174 MHz - 216 MHz | `band` | `broadcast` | `television-broadcast` |
| `italy.json` | TV banda III e DAB | `broadcast` | 174 MHz - 223 MHz | `band` | `broadcast` | `television-broadcast` |
| `republic-of-korea.json` | TV Broadcast | `broadcast` | 174 MHz - 216 MHz | `band` | `broadcast` | `television-broadcast` |
| `turkey.json` | DVB-T | `broadcast` | 174 MHz - 216 MHz | `band` | `broadcast` | `television-broadcast` |
| `usa.json` | TV Channels 7-13 | `broadcast` | 174 MHz - 216 MHz | `band` | `broadcast` | `television-broadcast` |

### `band:broadcast:television:vhf-low`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `canada.json` | TV Channels 2-4 | `broadcast` | 54 MHz - 72 MHz | `band` | `broadcast` | `television-broadcast` |
| `republic-of-korea.json` | TV Broadcast | `broadcast` | 54 MHz - 72 MHz | `band` | `broadcast` | `television-broadcast` |
| `republic-of-korea.json` | TV Broadcast | `broadcast` | 76 MHz - 88 MHz | `band` | `broadcast` | `television-broadcast` |
| `usa.json` | TV Channels 2-4 | `broadcast` | 54 MHz - 72 MHz | `band` | `broadcast` | `television-broadcast` |
| `usa.json` | TV Channels 5-6 | `broadcast` | 76 MHz - 88 MHz | `band` | `broadcast` | `television-broadcast` |

### `band:broadcast:weather-radio`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `canada.json` | Weatheradio | `broadcast` | 162.4 MHz - 162.7 MHz | `band` | `broadcast` | `weather-broadcast` |
| `france.json` | NOAA Weather Radio | `broadcast` | 162.3625 MHz - 162.5875 MHz | `band` | `broadcast` | `weather-broadcast` |
| `usa.json` | NOAA Weather Radio | `broadcast` | 162.3625 MHz - 162.5875 MHz | `band` | `broadcast` | `weather-broadcast` |

### `band:cellular:dcs-1800`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `italy.json` | GSM, IMT, MCA, MCV | `cellular` | 1.715 GHz - 1.785 GHz | `band` | `cellular` | `cellular-gsm` |
| `italy.json` | GSM, IMT, MCA, MCV | `cellular` | 1.805 GHz - 1.88 GHz | `band` | `cellular` | `cellular-gsm` |
| `netherlands.json` | GSM | `utility` | 1.71 GHz - 1.785 GHz | `band` | `cellular` | `cellular-gsm` |
| `netherlands.json` | GSM | `utility` | 1.805 GHz - 1.88 GHz | `band` | `cellular` | `cellular-gsm` |
| `russia.json` | DCS-1800 Uplink / L-Band | `broadcast` | 1.71 GHz - 1.785 GHz | `segment` | `cellular` | `cellular-gsm` |
| `russia.json` | DCS-1800 Downlink / L-Band | `broadcast` | 1.805 GHz - 1.88 GHz | `segment` | `cellular` | `cellular-gsm` |

### `band:cellular:eutran:1`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `germany-mobile-lte-bands.json` | LTE band 1 (IMT) FDD uplink | `LTE.FDD.uplink` | 1.92 GHz - 1.98 GHz | `segment` | `cellular` | `cellular-lte` |
| `germany-mobile-lte-bands.json` | LTE band 1 (IMT) FDD downlink | `LTE.FDD.downlink` | 2.11 GHz - 2.17 GHz | `segment` | `cellular` | `cellular-lte` |

### `band:cellular:eutran:20`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `germany-mobile-lte-bands.json` | LTE band 20 (Digital Dividend) FDD uplink | `LTE.FDD.uplink` | 832 MHz - 862 MHz | `segment` | `cellular` | `cellular-lte` |
| `italy.json` | LTE | `broadcast` | 791 MHz - 862 MHz | `band` | `cellular` | `cellular-lte` |
| `russia.json` | LTE-800-FDD Downlink | `broadcast` | 791 MHz - 821 MHz | `segment` | `cellular` | `cellular-lte` |
| `russia.json` | LTE-800-FDD Uplink | `broadcast` | 832 MHz - 862 MHz | `segment` | `cellular` | `cellular-lte` |

### `band:cellular:eutran:28`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `germany-mobile-lte-bands.json` | LTE band 28 (APT) FDD uplink | `LTE.FDD.uplink` | 703 MHz - 748 MHz | `segment` | `cellular` | `cellular-lte` |

### `band:cellular:eutran:3`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `germany-mobile-lte-bands.json` | LTE band 3 (DCS) FDD uplink | `LTE.FDD.uplink` | 1.71 GHz - 1.785 GHz | `segment` | `cellular` | `cellular-lte` |
| `germany-mobile-lte-bands.json` | LTE band 3 (DCS) FDD downlink | `LTE.FDD.downlink` | 1.805 GHz - 1.88 GHz | `segment` | `cellular` | `cellular-lte` |

### `band:cellular:eutran:32`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `united-kingdom.json` | Band 32 Cell phone | `cellular` | 1.452 GHz - 1.492 GHz | `band` | `cellular` | `cellular-lte` |

### `band:cellular:eutran:38`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `russia.json` | LTE-TDD / S-Band | `broadcast` | 2.57 GHz - 2.62 GHz | `band` | `cellular` | `cellular-lte` |

### `band:cellular:eutran:7`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `germany-mobile-lte-bands.json` | LTE band 7 (IMT-E) FDD uplink | `LTE.FDD.uplink` | 2.5 GHz - 2.57 GHz | `segment` | `cellular` | `cellular-lte` |
| `germany-mobile-lte-bands.json` | LTE band 7 (IMT-E) FDD downlink | `LTE.FDD.downlink` | 2.62 GHz - 2.69 GHz | `segment` | `cellular` | `cellular-lte` |
| `russia.json` | LTE-FDD Uplink / S-Band | `broadcast` | 2.5 GHz - 2.57 GHz | `segment` | `cellular` | `cellular-lte` |
| `russia.json` | LTE-FDD Downlink / S-Band | `broadcast` | 2.62 GHz - 2.69 GHz | `segment` | `cellular` | `cellular-lte` |

### `band:cellular:eutran:8`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `germany-mobile-lte-bands.json` | LTE band 8 (Extended GSM) FDD uplink | `LTE.FDD.uplink` | 880 MHz - 915 MHz | `segment` | `cellular` | `cellular-lte` |

### `band:cellular:gsm-900`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `italy.json` | GSM | `cellular` | 880 MHz - 915 MHz | `band` | `cellular` | `cellular-gsm` |
| `italy.json` | GSM | `cellular` | 925 MHz - 960 MHz | `band` | `cellular` | `cellular-gsm` |
| `netherlands.json` | GSM | `utility` | 880 MHz - 915 MHz | `band` | `cellular` | `cellular-gsm` |
| `netherlands.json` | GSM | `utility` | 925 MHz - 960 MHz | `band` | `cellular` | `cellular-gsm` |
| `russia.json` | GSM-900 Uplink | `broadcast` | 880 MHz - 915 MHz | `segment` | `cellular` | `cellular-gsm` |
| `russia.json` | GSM-900 Downlink | `broadcast` | 925 MHz - 960 MHz | `segment` | `cellular` | `cellular-gsm` |

### `band:cellular:gsm-r-900`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `germany-mobile-networks.json` | GSM-R FDD uplink | `mobile.gsm-r` | 873.1 MHz - 880 MHz | `segment` | `cellular` | `cellular-gsm` |
| `germany-mobile-networks.json` | GSM-R FDD downlink | `mobile.gsm-r` | 918.1 MHz - 925 MHz | `segment` | `cellular` | `cellular-gsm` |
| `italy.json` | GSM-R | `cellular` | 876 MHz - 880 MHz | `band` | `cellular` | `cellular-gsm` |
| `netherlands.json` | GSM-R (Train) | `utility1` | 876 MHz - 880 MHz | `band` | `cellular` | `cellular-gsm` |
| `netherlands.json` | GSM-R (Train) | `utility1` | 921 MHz - 925 MHz | `band` | `cellular` | `cellular-gsm` |

### `band:ism:122ghz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `italy.json` | ISM | `ism` | 120.2 GHz - 122.25 GHz | `band` | `ism` | `ism` |
| `netherlands.json` | ISM | `utility` | 120.2 GHz - 122.25 GHz | `band` | `ism` | `ism` |

### `band:ism:24ghz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `italy.json` | ISM, SRD e LPR | `ism` | 24.05 GHz - 24.45 GHz | `band` | `ism` | `ism` |
| `netherlands.json` | ISM, SRD and LPR | `utility` | 24 GHz - 24.45 GHz | `band` | `ism` | `ism` |

### `band:ism:27mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `netherlands.json` | 10m ISM | `utility` | 26.957 MHz - 27.283 MHz | `band` | `ism` | `ism` |

### `band:ism:2ghz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `netherlands.json` | ISM Band (13cm) | `utility` | 2.4 GHz - 2.5 GHz | `band` | `ism` | `ism` |

### `band:ism:40mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `netherlands.json` | 8m ISM | `utility` | 40.66 MHz - 40.7 MHz | `band` | `ism` | `ism` |

### `band:ism:433mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `netherlands.json` | Shared 70cm Ham and 70cm ISM | `utility` | 433.05 MHz - 434.79 MHz | `band` | `ism` | `ism` |

### `band:ism:61ghz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `italy.json` | ISM | `ism` | 61 GHz - 64 GHz | `band` | `ism` | `ism` |
| `netherlands.json` | ISM | `utility` | 61 GHz - 64 GHz | `band` | `ism` | `ism` |

### `band:ism:6mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `netherlands.json` | ISM Band (50m) | `utility` | 6.765 MHz - 6.795 MHz | `band` | `ism` | `ism` |

### `band:ism:868mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `germany.json` | 868 MHz ISM-Devices | `other` | 866.5 MHz - 871 MHz | `band` | `ism` | `ism` |
| `italy.json` | ISM e RFID | `ism` | 862 MHz - 876 MHz | `band` | `ism` | `ism` |
| `netherlands.json` | ISM Europe | `utility1` | 862 MHz - 870 MHz | `band` | `ism` | `ism` |
| `turkey.json` | RFID | `other` | 865 MHz - 868 MHz | `band` | `ism` | `ism` |

### `band:ism:915mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | ISM Band | `other` | 915 MHz - 928 MHz | `band` | `ism` | `ism` |
| `turkey.json` | RFID | `other` | 916.1 MHz - 918.9 MHz | `band` | `ism` | `ism` |

### `band:maritime:hf:12mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Marine - HF | `marine` | 12.23 MHz - 13.2 MHz | `band` | `maritime` | `maritime` |
| `germany.json` | Maritime | `marine` | 12.23 MHz - 13.2 MHz | `band` | `maritime` | `maritime` |
| `italy.json` | Mobile marittimo | `marine` | 12.23 MHz - 13.2 MHz | `band` | `maritime` | `maritime` |
| `united-kingdom.json` | Maritime | `marine` | 12.23 MHz - 13.2 MHz | `band` | `maritime` | `maritime` |

### `band:maritime:hf:22mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Marine - HF | `marine` | 22 MHz - 22.855 MHz | `band` | `maritime` | `maritime` |
| `italy.json` | Mobile marittimo | `marine` | 22 MHz - 22.855 MHz | `band` | `maritime` | `maritime` |
| `netherlands.json` | (Ship/Shore) Maritime | `marine` | 22 MHz - 24.89 MHz | `band` | `maritime` | `maritime` |

### `band:maritime:hf:25mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Marine - HF | `marine` | 25.07 MHz - 25.21 MHz | `band` | `maritime` | `maritime` |
| `italy.json` | Mobile marittimo | `marine` | 25.07 MHz - 25.21 MHz | `band` | `maritime` | `maritime` |
| `netherlands.json` | Maritime | `marine` | 25.005 MHz - 25.55 MHz | `band` | `maritime` | `maritime` |

### `band:maritime:hf:2mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `germany.json` | Maritime | `marine` | 2.045 MHz - 2.3 MHz | `band` | `maritime` | `maritime` |
| `italy.json` | Mobile marittimo | `marine` | 1.85 MHz - 2.3 MHz | `band` | `maritime` | `maritime` |
| `united-kingdom.json` | Maritime | `marine` | 2.045 MHz - 2.3 MHz | `band` | `maritime` | `maritime` |

### `band:maritime:hf:4mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Marine - HF | `marine` | 4.065 MHz - 4.44 MHz | `band` | `maritime` | `maritime` |
| `germany.json` | Maritime | `marine` | 4.063 MHz - 4.438 MHz | `band` | `maritime` | `maritime` |
| `italy.json` | Mobile marittimo | `marine` | 4.065 MHz - 4.44 MHz | `band` | `maritime` | `maritime` |
| `netherlands.json` | Maritime | `marine1` | 4 MHz - 4.75 MHz | `band` | `maritime` | `maritime` |
| `republic-of-korea.json` | Oceanographic Data | `marine` | 4.063 MHz - 4.065 MHz | `band` | `maritime` | `maritime` |
| `republic-of-korea.json` | Ship Station Duplex Telephone | `marine` | 4.065 MHz - 4.146 MHz | `band` | `maritime` | `maritime` |
| `republic-of-korea.json` | Ship Station Simplex Telephone | `marine` | 4.146 MHz - 4.152 MHz | `band` | `maritime` | `maritime` |
| `republic-of-korea.json` | Ship Station Wideband Telegraph Fax | `marine` | 4.152 MHz - 4.172 MHz | `band` | `maritime` | `maritime` |
| `republic-of-korea.json` | Ship Station Narrowband | `marine` | 4.172 MHz - 4.18175 MHz | `band` | `maritime` | `maritime` |
| `republic-of-korea.json` | Ship Station A1A Morse Code Communication | `marine` | 4.18675 MHz - 4.20225 MHz | `band` | `maritime` | `maritime` |
| `united-kingdom.json` | Maritime | `marine` | 4.063 MHz - 4.438 MHz | `band` | `maritime` | `maritime` |

### `band:maritime:hf:6mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Marine - HF | `marine` | 6.2 MHz - 6.525 MHz | `band` | `maritime` | `maritime` |
| `germany.json` | Maritime | `marine` | 6.2 MHz - 6.525 MHz | `band` | `maritime` | `maritime` |
| `italy.json` | Mobile marittimo | `marine` | 6.2 MHz - 6.525 MHz | `band` | `maritime` | `maritime` |
| `netherlands.json` | (Ship TX) Maritime | `marine` | 6.2 MHz - 6.21475 MHz | `band` | `maritime` | `maritime` |
| `netherlands.json` | (Ship TX) Maritime | `marine` | 6.21625 MHz - 6.3425 MHz | `band` | `maritime` | `maritime` |
| `united-kingdom.json` | Maritime | `marine` | 6.2 MHz - 6.525 MHz | `band` | `maritime` | `maritime` |

### `band:maritime:hf:8mhz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `france.json` | Marine - HF | `marine` | 8.195 MHz - 8.815 MHz | `band` | `maritime` | `maritime` |
| `germany.json` | Maritime | `marine` | 8.195 MHz - 8.815 MHz | `band` | `maritime` | `maritime` |
| `italy.json` | Mobile marittimo | `marine` | 8.195 MHz - 8.815 MHz | `band` | `maritime` | `maritime` |
| `netherlands.json` | Distress | `marine1` | 8.28975 MHz - 8.29225 MHz | `band` | `maritime` | `maritime` |
| `netherlands.json` | (Ship/Shore) Maritime | `marine` | 8.29225 MHz - 8.68 MHz | `band` | `maritime` | `maritime` |
| `united-kingdom.json` | Maritime | `marine` | 8.195 MHz - 8.815 MHz | `band` | `maritime` | `maritime` |

### `band:maritime:mf`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `italy.json` | Mobile marittimo, NAVTEX | `marine` | 435 kHz - 520 kHz | `band` | `maritime` | `maritime` |
| `republic-of-korea.json` | Maritime Telegraph | `marine` | 505 kHz - 526.5 kHz | `band` | `maritime` | `maritime` |

### `band:maritime:vhf`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | VHF Marine Band | `marine` | 156 MHz - 162.05 MHz | `band` | `maritime` | `maritime` |
| `china.json` | Marine | `marine` | 156.4875 MHz - 156.5625 MHz | `band` | `maritime` | `maritime` |
| `china.json` | Marine | `marine` | 156.6725 MHz - 160.975 MHz | `band` | `maritime` | `maritime` |
| `china.json` | Marine | `marine` | 161.475 MHz - 162.05 MHz | `band` | `maritime` | `maritime` |
| `france.json` | Marine | `marine` | 156 MHz - 162.025 MHz | `band` | `maritime` | `maritime` |
| `general.json` | Marine | `marine` | 156 MHz - 162.025 MHz | `band` | `maritime` | `maritime` |
| `germany.json` | Marine | `marine` | 156 MHz - 162.025 MHz | `band` | `maritime` | `maritime` |
| `italy.json` | Marittimo VHF | `marine` | 156 MHz - 162.025 MHz | `band` | `maritime` | `maritime` |
| `netherlands.json` | Fixed Mobile/ Marine | `marine1` | 155.9875 MHz - 174 MHz | `band` | `maritime` | `maritime` |
| `russia.json` | Marine | `marine` | 156 MHz - 162.025 MHz | `band` | `maritime` | `maritime` |
| `united-kingdom.json` | Marine - ship tx | `marine` | 156 MHz - 157.85 MHz | `band` | `maritime` | `maritime` |
| `united-kingdom.json` | Marine - coast tx | `marine` | 160.65 MHz - 162.025 MHz | `band` | `maritime` | `maritime` |
| `usa.json` | Marine | `marine` | 156 MHz - 162.025 MHz | `band` | `maritime` | `maritime` |

### `band:navigation:ils-glide-path`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `netherlands.json` | ILS Glide path | `aviation` | 328.6 MHz - 335.4 MHz | `band` | `navigation` | `navigation` |
| `turkey.json` | ILS-Glide Path | `aviation` | 328.6 MHz - 335.4 MHz | `band` | `navigation` | `navigation` |

### `band:navigation:ndb`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `italy.json` | Radiofari e NDB | `aviation` | 283.5 kHz - 405 kHz | `band` | `navigation` | `navigation` |
| `netherlands.json` | Aeronautical Radionavigation / Maritime | `aviation` | 283.5 kHz - 472 kHz | `band` | `navigation` | `navigation` |

### `band:navigation:vor-ils`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Air Band VOR/ILS | `aviation` | 108 MHz - 117.975 MHz | `band` | `navigation` | `navigation` |
| `canada.json` | Air Band VOR/ILS | `aviation` | 108 MHz - 117 MHz | `band` | `navigation` | `navigation` |
| `china.json` | Air Band Radionavigation | `aviation` | 108 MHz - 117.975 MHz | `band` | `navigation` | `navigation` |
| `france.json` | Aviation - VOR/ILS | `aviation` | 108 MHz - 118 MHz | `band` | `navigation` | `navigation` |
| `general.json` | Air Band VOR/ILS | `aviation` | 108 MHz - 118 MHz | `band` | `navigation` | `navigation` |
| `germany.json` | Air Band VOR/ILS | `aviation` | 108 MHz - 118 MHz | `band` | `navigation` | `navigation` |
| `ireland.json` | Airband VOR/ILS | `aviation` | 108 MHz - 117.9 MHz | `band` | `navigation` | `navigation` |
| `italy.json` | VOR/ILS | `aviation` | 108 MHz - 117.975 MHz | `band` | `navigation` | `navigation` |
| `netherlands.json` | Air Band VOR/ILS | `aviation` | 108 MHz - 118 MHz | `band` | `navigation` | `navigation` |
| `republic-of-korea.json` | ILS Localizer VOR | `fixed` | 108 MHz - 117.975 MHz | `band` | `navigation` | `navigation` |
| `russia.json` | Air Band VOR/ILS | `aviation` | 108 MHz - 118 MHz | `band` | `navigation` | `navigation` |
| `turkey.json` | Airband VOR/ILS | `aviation` | 108 MHz - 117.975 MHz | `band` | `navigation` | `navigation` |
| `united-kingdom.json` | Air Band TACAN/ILS | `aviation` | 108 MHz - 117.975 MHz | `band` | `navigation` | `navigation` |
| `usa.json` | Air Band VOR/ILS | `aviation` | 108 MHz - 118 MHz | `band` | `navigation` | `navigation` |

### `band:personal-radio:cb`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 27MHz CB | `amateur` | 26.965 MHz - 27.405 MHz | `band` | `personal-radio` | `personal-radio` |
| `austria.json` | CB | `other` | 26.965 MHz - 27.405 MHz | `band` | `personal-radio` | `personal-radio` |
| `belgium.json` | 11m - Citizen Band | `amateur` | 26.96 MHz - 27.41 MHz | `band` | `personal-radio` | `personal-radio` |
| `canada.json` | CB | `amateur` | 26.96 MHz - 27.41 MHz | `band` | `personal-radio` | `personal-radio` |
| `france.json` | 11m - CB | `amateur` | 26.96 MHz - 27.41 MHz | `band` | `personal-radio` | `personal-radio` |
| `general.json` | CB | `amateur` | 26.96 MHz - 27.41 MHz | `band` | `personal-radio` | `personal-radio` |
| `germany.json` | CB | `other` | 26.565 MHz - 27.405 MHz | `band` | `personal-radio` | `personal-radio` |
| `ireland.json` | CB | `amateur` | 26.965 MHz - 27.405 MHz | `band` | `personal-radio` | `personal-radio` |
| `italy.json` | CB | `amateur` | 26.175 MHz - 27.23 MHz | `band` | `personal-radio` | `personal-radio` |
| `russia.json` | CB | `amateur` | 26.96 MHz - 27.41 MHz | `band` | `personal-radio` | `personal-radio` |
| `slovakia.json` | CB | `other` | 26.965 MHz - 27.405 MHz | `band` | `personal-radio` | `personal-radio` |
| `turkey.json` | CB | `other` | 26.565 MHz - 27.405 MHz | `band` | `personal-radio` | `personal-radio` |
| `united-kingdom.json` | CB - CEPT | `amateur` | 26.96 MHz - 27.41 MHz | `band` | `personal-radio` | `personal-radio` |
| `united-kingdom.json` | CB | `amateur` | 27.6 MHz - 28 MHz | `band` | `personal-radio` | `personal-radio` |
| `usa.json` | CB | `amateur` | 26.96 MHz - 27.41 MHz | `band` | `personal-radio` | `personal-radio` |

### `band:personal-radio:freenet`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `austria.json` | Freenet | `other` | 149.025 MHz - 149.115625 MHz | `band` | `personal-radio` | `personal-radio` |
| `germany.json` | Freenet | `other` | 149.025 MHz - 149.115625 MHz | `band` | `personal-radio` | `personal-radio` |

### `band:personal-radio:frs-gmrs`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `usa.json` | FRS | `amateur` | 462.55 MHz - 462.725 MHz | `band` | `personal-radio` | `personal-radio` |
| `usa.json` | FRS - GMRS | `amateur` | 467.55 MHz - 467.725 MHz | `band` | `personal-radio` | `personal-radio` |

### `band:personal-radio:murs`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `usa.json` | MURS (lower) | `amateur` | 151.82 MHz - 151.94 MHz | `band` | `personal-radio` | `personal-radio` |
| `usa.json` | MURS (upper) | `amateur` | 154.57 MHz - 154.6 MHz | `band` | `personal-radio` | `personal-radio` |

### `band:personal-radio:pmr446`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `austria.json` | PMR446 | `other` | 446.00625 MHz - 446.196875 MHz | `band` | `personal-radio` | `personal-radio` |
| `belgium.json` | PMR446 | `amateur` | 446 MHz - 446.2 MHz | `band` | `personal-radio` | `personal-radio` |
| `france.json` | PMR446 | `amateur` | 446 MHz - 446.2 MHz | `band` | `personal-radio` | `personal-radio` |
| `general.json` | PMR446 | `amateur` | 446 MHz - 446.2 MHz | `band` | `personal-radio` | `personal-radio` |
| `germany.json` | PMR446 | `other` | 446.00625 MHz - 446.196875 MHz | `band` | `personal-radio` | `personal-radio` |
| `italy.json` | PMR446 | `amateur` | 446 MHz - 446.2 MHz | `band` | `personal-radio` | `personal-radio` |
| `netherlands.json` | PMR446 | `utility1` | 446 MHz - 446.2 MHz | `band` | `personal-radio` | `personal-radio` |
| `russia.json` | PMR | `amateur` | 446 MHz - 446.2 MHz | `band` | `personal-radio` | `personal-radio` |
| `slovakia.json` | PMR446 | `other` | 446.00625 MHz - 446.19375 MHz | `band` | `personal-radio` | `personal-radio` |
| `turkey.json` | PMR446 | `other` | 446.00625 MHz - 446.196875 MHz | `band` | `personal-radio` | `personal-radio` |
| `united-kingdom.json` | PMR446 | `PMR` | 446 MHz - 446.2 MHz | `band` | `personal-radio` | `personal-radio` |
| `usa.json` | PMR446 | `amateur` | 446 MHz - 446.2 MHz | `band` | `personal-radio` | `personal-radio` |

### `band:personal-radio:uhf-cb`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | UHF CB | `amateur` | 476.425 MHz - 477.4125 MHz | `band` | `personal-radio` | `personal-radio` |

### `band:rlan:2ghz`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 2.4GHz WiFi (ISM Band) | `other` | 2.4 GHz - 2.4835 GHz | `band` | `rlan` | `rlan` |
| `italy.json` | ISM, SAP/SAB, 802.11 | `ism` | 2.4 GHz - 2.5 GHz | `band` | `rlan` | `rlan` |
| `russia.json` | 2.4GHz WiFi / S-Band | `broadcast` | 2.45 GHz - 2.4835 GHz | `band` | `rlan` | `rlan` |
| `united-kingdom.json` | ISM - wifi and bluettoth | `ISM` | 2.4 GHz - 2.483 GHz | `band` | `rlan` | `rlan` |

### `band:rlan:5ghz-lower`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | 5GHz WiFi (ISM Band) | `other` | 5.15 GHz - 5.59 GHz | `band` | `rlan` | `rlan` |
| `italy.json` | Reti numeriche e 802.11 | `comms` | 5.25 GHz - 5.65 GHz | `band` | `rlan` | `rlan` |
| `netherlands.json` | Digital networks and 802.11 | `utility` | 5.25 GHz - 5.65 GHz | `band` | `rlan` | `rlan` |
| `russia.json` | 5GHz WiFi / C-Band | `broadcast` | 5.15 GHz - 5.35 GHz | `band` | `rlan` | `rlan` |

### `band:rlan:5ghz-middle`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `russia.json` | 5GHz WiFi / C-Band | `broadcast` | 5.67 GHz - 5.725 GHz | `band` | `rlan` | `rlan` |

### `band:rlan:5ghz-upper`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `russia.json` | C-Band / 5GHz WiFi | `broadcast` | 5.76 GHz - 5.762 GHz | `band` | `rlan` | `rlan` |

### `band:satellite:weather-vhf`

| Plan file | Legacy name | Type | Frequency span | Kind | Service | Family |
|---|---|---|---|---|---|---|
| `australia.json` | Polar Orbiting Satellites | `satellite` | 137 MHz - 138 MHz | `band` | `satellite` | `satellite` |
| `belgium.json` | Space Exploration / Meteorology Sat. / S-PCS | `satellite` | 137 MHz - 138 MHz | `band` | `satellite` | `satellite` |
| `china.json` | LEO Satellite Downlinks | `satellite` | 137 MHz - 138 MHz | `segment` | `satellite` | `satellite` |
| `france.json` | Polar Orbiting Satellites | `satellite` | 137 MHz - 138 MHz | `band` | `satellite` | `satellite` |
| `general.json` | Polar Orbiting Satellites | `satellite` | 137 MHz - 138 MHz | `band` | `satellite` | `satellite` |
| `germany.json` | Earth orbiting Satellites | `satellite` | 137 MHz - 138 MHz | `band` | `satellite` | `satellite` |
| `ireland.json` | Polar orbiting satellites | `satellite` | 137 MHz - 138 MHz | `band` | `satellite` | `satellite` |
| `italy.json` | Satelliti polari | `satellite` | 137 MHz - 138 MHz | `band` | `satellite` | `satellite` |
| `netherlands.json` | Satellite | `satellite1` | 137 MHz - 138 MHz | `band` | `satellite` | `satellite` |
| `russia.json` | Polar Orbiting Satellites | `satellite` | 137 MHz - 138 MHz | `band` | `satellite` | `satellite` |
| `united-kingdom.json` | Satellites | `satellite` | 137 MHz - 138 MHz | `band` | `satellite` | `satellite` |
| `usa.json` | Polar Orbiting Satellites | `satellite` | 137 MHz - 138 MHz | `band` | `satellite` | `satellite` |

## Legacy rows without a stable band ID


### Australia (`australia.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | 27MHz Marine Band | `marine` | 27.68 MHz - 27.98 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 2 | Digital Radio Broadcast (DAB+) | `broadcast` | 195 MHz - 209 MHz | `band` | `broadcast` | `sound-broadcast` | no stable band mapping |
| 3 | 5GHz WiFi (ISM Band) | `other` | 5.65 GHz - 5.835 GHz | `band` | `rlan` | `rlan` | composite span crosses multiple stable bands |

### Belgium (`belgium.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | T-DAB Broadcast | `broadcast` | 174 MHz - 223 MHz | `band` | `broadcast` | `sound-broadcast` | no stable band mapping |

### Canada (`canada.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | Trunked Mobile | `broadcast` | 806 MHz - 890 MHz | `band` | `broadcast` | `sound-broadcast` | no stable band mapping |

### China (Mainland) (`china.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | Frequency and Time Standards | `broadcast` | 24.99 MHz - 25.01 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |

### France (`france.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | Marine - HF | `marine` | 1.607 MHz - 1.81 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 2 | Marine - HF | `marine` | 2.5 MHz - 2.85 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 3 | Marine - HF | `marine` | 3.155 MHz - 3.4 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 4 | Marine - HF | `marine` | 3.5 MHz - 3.6 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 5 | Marine - HF | `marine` | 16.36 MHz - 17.41 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 6 | Marine - HF | `marine` | 18.78 MHz - 18.9 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 7 | Marine - HF | `marine` | 19.68 MHz - 19.8 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 8 | Marine - HF | `marine` | 26.1 MHz - 26.175 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 9 | Radiodiffusion - Bande DAB | `broadcast` | 174 MHz - 223 MHz | `band` | `broadcast` | `sound-broadcast` | no stable band mapping |
| 10 | Military Aviation | `military` | 225 MHz - 380 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 11 | Satellite militaire | `military` | 240 MHz - 270 MHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 12 | Police (TETRAPOL) | `military` | 380 MHz - 400 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 13 | Radiodiffusion - Bande DAB | `broadcast` | 1.452 GHz - 1.492 GHz | `band` | `broadcast` | `sound-broadcast` | no stable band mapping |

### General (`general.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | Military Air | `military` | 225 MHz - 380 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 2 | Military Sat | `military` | 240 MHz - 270 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |

### German LTE bands (`germany-mobile-lte-bands.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | LTE band 28 (APT) FDD downlink | `LTE.FDD.downlink` | 7.58 GHz - 8.03 GHz | `segment` | `cellular` | `cellular-lte` | no stable band mapping |
| 2 | LTE band 20 (Digital Dividend) FDD downlink | `LTE.FDD.downlink` | 7.91 GHz - 8.21 GHz | `segment` | `cellular` | `cellular-lte` | no stable band mapping |
| 3 | LTE band 8 (Extended GSM) FDD downlink | `LTE.FDD.downlink` | 9.25 GHz - 9.6 GHz | `segment` | `cellular` | `cellular-lte` | no stable band mapping |
| 4 | LTE band 32 (L-Band (EU)) SDL downlink | `LTE.SDL` | 14.52 GHz - 14.96 GHz | `segment` | `cellular` | `cellular-lte` | no stable band mapping |

### German Mobile Networks (`germany-mobile-networks.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | 703 Telefonica FDD uplink | `mobile.mno.telefonica` | 703 MHz - 713 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 2 | 713 Telekom FDD uplink | `mobile.mno.telekom` | 713 MHz - 723 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 3 | 723 Vodafone FDD uplink | `mobile.mno.vodafone` | 723 MHz - 733 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 4 | 758 Telefonica FDD downlink | `mobile.mno.telefonica` | 758 MHz - 768 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 5 | 768 Telekom FDD downlink | `mobile.mno.telekom` | 768 MHz - 778 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 6 | 778 Vodafone FDD downlink | `mobile.mno.vodafone` | 778 MHz - 788 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 7 | 791 Telefonica FDD downlink | `mobile.mno.telefonica` | 791 MHz - 801 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 8 | 801 Vodafone FDD downlink | `mobile.mno.vodafone` | 801 MHz - 811 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 9 | 811 Telekom FDD downlink | `mobile.mno.telekom` | 811 MHz - 821 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 10 | 832 Telefonica FDD uplink | `mobile.mno.telefonica` | 832 MHz - 842 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 11 | 842 Vodafone FDD uplink | `mobile.mno.vodafone` | 842 MHz - 852 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 12 | 852 Telekom FDD uplink | `mobile.mno.telekom` | 852 MHz - 862 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 13 | 880 Telefonica FDD uplink | `mobile.mno.telefonica` | 880 MHz - 890 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 14 | 890 Vodafone FDD uplink | `mobile.mno.vodafone` | 890 MHz - 900 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 15 | 900 Telekom FDD uplink | `mobile.mno.telekom` | 900 MHz - 915 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 16 | 925 Telefonica FDD downlink | `mobile.mno.telefonica` | 925 MHz - 935 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 17 | 935 Vodafone FDD downlink | `mobile.mno.vodafone` | 935 MHz - 945 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 18 | 945 Telekom FDD downlink | `mobile.mno.telekom` | 945 MHz - 960 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 19 | 1452 Telekom SDL downlink | `mobile.mno.telekom` | 1.452 GHz - 1.472 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 20 | 1472 Vodafone SDL downlink | `mobile.mno.vodafone` | 1.472 GHz - 1.492 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 21 | 1710 Telekom FDD uplink | `mobile.mno.telekom` | 1.71 GHz - 1.74 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 22 | 1740 Telefonica FDD uplink | `mobile.mno.telefonica` | 1.74 GHz - 1.76 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 23 | 1760 Vodafone FDD uplink | `mobile.mno.vodafone` | 1.76 GHz - 1.785 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 24 | 1805 Telekom FDD downlink | `mobile.mno.telekom` | 1.805 GHz - 1.835 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 25 | 1835 Telefonica FDD downlink | `mobile.mno.telefonica` | 1.835 GHz - 1.855 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 26 | 1855 Vodafone FDD downlink | `mobile.mno.vodafone` | 1.855 GHz - 1.88 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 27 | DECT | `broadcast` | 1.88 GHz - 1.9 GHz | `band` | `broadcast` | `sound-broadcast` | no stable band mapping |
| 28 | 1900.1 Telefonica | `mobile.mno.telefonica` | 1.9001 GHz - 1.9051 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 29 | 1920 Vodafone FDD uplink | `mobile.mno.vodafone` | 1.92 GHz - 1.94 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 30 | 1940 Telefonica FDD uplink | `mobile.mno.telefonica` | 1.94 GHz - 1.96 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 31 | 1960 Telekom FDD uplink | `mobile.mno.telekom` | 1.96 GHz - 1.98 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 32 | 2010.5 Telefonica | `mobile.mno.telefonica` | 2.0105 GHz - 2.0247 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 33 | 2110 Vodafone FDD downlink | `mobile.mno.vodafone` | 2.11 GHz - 2.13 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 34 | 2130 Telefonica FDD downlink | `mobile.mno.telefonica` | 2.13 GHz - 2.15 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 35 | 2150 Telekom FDD downlink | `mobile.mno.telekom` | 2.15 GHz - 2.17 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 36 | 2500 Vodafone FDD uplink | `mobile.mno.vodafone` | 2.5 GHz - 2.52 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 37 | 2520 Telekom FDD uplink | `mobile.mno.telekom` | 2.52 GHz - 2.54 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 38 | 2540 Telefonica FDD uplink | `mobile.mno.telefonica` | 2.54 GHz - 2.57 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 39 | 2570 Telefonica TDD | `mobile.mno.telefonica` | 2.57 GHz - 2.58 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 40 | 2580 Vodafone TDD | `mobile.mno.vodafone` | 2.58 GHz - 2.605 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 41 | 2605 Telekom TDD | `mobile.mno.telekom` | 2.605 GHz - 2.61 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 42 | 2610 Telefonica TDD | `mobile.mno.telefonica` | 2.61 GHz - 2.62 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 43 | 2620 Vodafone FDD downlink | `mobile.mno.vodafone` | 2.62 GHz - 2.64 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 44 | 2640 Telekom FDD downlink | `mobile.mno.telekom` | 2.64 GHz - 2.66 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 45 | 2660 Telefonica FDD downlink | `mobile.mno.telefonica` | 2.66 GHz - 2.69 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 46 | 3400 Vodafone | `mobile.mno.vodafone` | 3.4 GHz - 3.49 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 47 | 3490 Drillisch | `mobile.mno.drillisch` | 3.49 GHz - 3.54 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 48 | 3540 Telefonica | `mobile.mno.telefonica` | 3.54 GHz - 3.61 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 49 | 3610 Telekom | `mobile.mno.telekom` | 3.61 GHz - 3.7 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |

### Germany (`germany.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | Maritime | `marine` | 16.36 MHz - 17.41 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 2 | Maritime - ship tx | `marine` | 18.78 MHz - 18.9 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 3 | Maritime - coast tx | `marine` | 19.68 MHz - 19.99 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 4 | Pager BOS | `other` | 163 MHz - 174 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 5 | DAB+ (digital broadcast) | `broadcast` | 174 MHz - 225 MHz | `band` | `broadcast` | `sound-broadcast` | no stable band mapping |
| 6 | Air Band Military | `military` | 225 MHz - 380 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 7 | TETRA BOS | `other` | 388 MHz - 397 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 8 | Weathersondes | `other` | 401 MHz - 410 MHz | `band` | `meteorological` | `meteorological` | service has no stable frequency-band catalog |
| 9 | TETRA Civil | `other` | 423 MHz - 430 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 10 | Pager Civil | `other` | 446.5 MHz - 470 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 11 | L-Band | `other` | 1.3 GHz - 2 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |

### Ireland (`ireland.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | ADS-B | `aviation` | 1.089 GHz - 1.091 GHz | `channel` | `aviation` | `aviation-surveillance` | individual channel/bookmark; not a band |

### Italy (`italy.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | Ausili metereologici | `utility` | 8.3 kHz - 11.3 kHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 2 | Radionavigazione | `marine` | 11.3 kHz - 148.5 kHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 3 | Radiogoniometria | `marine` | 405 kHz - 415 kHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 4 | Mobile marittimo | `marine` | 1.6065 MHz - 1.83 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 5 | Segnali orari | `utility` | 2.501 MHz - 2.502 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 6 | Mobile marittimo | `marine` | 2.502 MHz - 2.85 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 7 | Mobile marittimo | `marine` | 3.155 MHz - 3.2 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 8 | Mobile marittimo | `marine` | 3.5 MHz - 3.6 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 9 | ISM, reti fisse pubbliche | `utility` | 6.525 MHz - 6.765 MHz | `band` | `ism` | `ism` | no stable band mapping |
| 10 | Segnali orari | `utility` | 14.99 MHz - 15.01 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 11 | Mobile marittimo | `marine` | 16.36 MHz - 17.41 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 12 | Mobile marittimo | `marine` | 18.78 MHz - 18.9 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 13 | Mobile marittimo | `marine` | 19.68 MHz - 19.8 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 14 | Segnali orari | `utility` | 19.99 MHz - 20.01 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 15 | Segnali orari | `utility` | 24.99 MHz - 25.01 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 16 | Radioastronomia | `utility` | 25.55 MHz - 25.67 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 17 | Mobile marittimo | `military` | 26.1 MHz - 26.175 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 18 | Wind profiler | `military` | 45 MHz - 47 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 19 | Wind profiler | `military` | 52.5 MHz - 68 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 20 | Soccorso alpino | `military` | 68 MHz - 74.8 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 21 | Radiofari 75MHz | `aviation` | 74.8 MHz - 75.2 MHz | `channel` | `navigation` | `navigation` | individual channel/bookmark; not a band |
| 22 | Telefonia satellitare | `satellite` | 148 MHz - 150.05 MHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 23 | Mobile aeronautico militare | `military` | 225 MHz - 240 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 24 | Satelliti militari | `military` | 240 MHz - 270 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 25 | Mobile aeronautico militare | `military` | 270 MHz - 399.9 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 26 | Telefonia satellitare | `satellite` | 399.9 MHz - 400.05 MHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 27 | Segnali orari via satellite | `satellite` | 400.05 MHz - 400.15 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 28 | Radiosonde | `satellite` | 400.15 MHz - 406 MHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 29 | EPIRB e PLB | `satellite` | 406 MHz - 406.1 MHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 30 | Satelliti militari | `military` | 406.1 MHz - 410 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 31 | Fisso militare | `military` | 410 MHz - 420 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 32 | Mobile militare | `military` | 420 MHz - 430 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 33 | Fisso militare | `military` | 434 MHz - 435 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 34 | Mobile o fisso privato | `comms` | 438 MHz - 446 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 35 | Mobile o fisso privato | `utility` | 446.2 MHz - 470 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 36 | GSM-R | `cellular` | 9.21 GHz - 925 MHz | `band` | `cellular` | `cellular-gsm` | invalid reversed frequency span |
| 37 | Radiolocalizzazione GNSS | `utility` | 1.164 GHz - 124 MHz | `band` | `land-mobile` | `land-mobile` | invalid reversed frequency span |
| 38 | Wind profiler | `military` | 1.27 GHz - 129.8 MHz | `band` | `other` | `unknown` | invalid reversed frequency span |
| 39 | Radiolocalizzazione | `military` | 1.298 GHz - 130 MHz | `band` | `other` | `unknown` | invalid reversed frequency span |
| 40 | Wind profiler | `military` | 1.3 GHz - 1.4 GHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 41 | Radioastronomia | `utility` | 1.4 GHz - 1.427 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 42 | Reti per segnali audio | `utility` | 1.427 GHz - 1.525 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 43 | Telefonia satellitare | `satellite` | 1.525 GHz - 1.559 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 44 | Radionavigazione GNSS | `satellite` | 1.559 GHz - 1.626 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 45 | Telefonia satellitare | `satellite` | 1.626 GHz - 1.66 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 46 | Satelliti militari | `military` | 1.66 GHz - 1.71 GHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 47 | MCA, MCV | `utility` | 1.71 GHz - 1.715 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 48 | Radiomicrofoni | `utility` | 1.785 GHz - 1.805 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 49 | DECT | `utility` | 1.88 GHz - 1.9 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 50 | GSM, IMT, MCA, MCV | `cellular` | 1.88 GHz - 1.9 GHz | `band` | `cellular` | `cellular-gsm` | no stable band mapping |
| 51 | IMT, MCA, MCV | `utility` | 1.9 GHz - 1.98 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 52 | MSS 2 GHz | `satellite` | 1.98 GHz - 201 MHz | `band` | `satellite` | `satellite` | invalid reversed frequency span |
| 53 | SAP/SAB, IMT, PMSE | `utility` | 2.01 GHz - 2.025 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 54 | Satelliti | `satellite` | 2.025 GHz - 2.11 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 55 | Radioastronomia | `utility` | 2.11 GHz - 2.12 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 56 | IMT, MCA, MCV | `utility` | 2.12 GHz - 2.17 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 57 | MSS 2 GHz | `satellite` | 2.17 GHz - 2.2 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 58 | Telemetria | `utility` | 2.2 GHz - 2.29 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 59 | SAP/SAB | `utility` | 2.29 GHz - 2.3 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 60 | IMT | `cellular` | 2.5 GHz - 2.5445 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 61 | Radioastronomia | `utility` | 2.69 GHz - 2.7 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 62 | Radar meteo | `military` | 2.7 GHz - 2.9 GHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 63 | Radar marittimi | `marine` | 2.9 GHz - 3.4 GHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 64 | Reti numeriche | `comms` | 3.475 GHz - 4.2 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 65 | Radioaltimetri | `aviation` | 4.2 GHz - 4.4 GHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 66 | Feeder link | `satellite` | 5.15 GHz - 5.25 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 67 | Reti numeriche e LPR | `comms` | 5.925 GHz - 7.75 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 68 | LPR | `comms` | 7.75 GHz - 7.975 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 69 | Telerilevamento | `utility` | 7.975 GHz - 8.215 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 70 | TLPR e SRD | `comms` | 8.215 GHz - 8.65 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 71 | Radar Doppler | `aircraft` | 8.65 GHz - 8.85 GHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 72 | Radar marittimi | `marine` | 8.85 GHz - 9 GHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 73 | Radar e transponder SART | `marine` | 9 GHz - 9.5 GHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 74 | TLPR e SRD | `utility` | 9.5 GHz - 10 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 75 | Reti punto-punto televisive | `comms` | 10.5 GHz - 10.68 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 76 | Reti fisse numeriche | `comms` | 10.68 GHz - 11.7 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 77 | Satelliti televisivi | `satellite` | 11.7 GHz - 12.5 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 78 | Reti fisse numeriche | `comms` | 12.5 GHz - 13.25 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 79 | Uplink satellitari | `satellite` | 14 GHz - 14.5 GHz | `segment` | `satellite` | `satellite` | no stable band mapping |
| 80 | Rete fisse numeriche | `comms` | 14.5 GHz - 14.62 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 81 | Rete fisse numeriche | `comms` | 15.23 GHz - 15.35 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 82 | Reti numeriche punto-punto | `comms` | 17.1 GHz - 19.3 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 83 | Feeder link | `satellite` | 19.3 GHz - 19.7 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 84 | HEST, LEST, ESIM, ESOMP | `satellite` | 19.7 GHz - 20.2 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 85 | Reti fisse numeriche | `comms` | 22 GHz - 22.33 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 86 | Reti fisse numeriche | `comms` | 22.67475 GHz - 2.28335 GHz | `band` | `land-mobile` | `land-mobile` | invalid reversed frequency span |
| 87 | Reti fisse numeriche, SAP/SAB | `comms` | 22.92675 GHz - 23.15 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 88 | Reti fisse numeriche | `comms` | 23.15 GHz - 23.338 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 89 | Reti punto-punto e punto-multipunto | `comms` | 24.45 GHz - 25.109 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 90 | LPR, SRD e SRR | `utility` | 25.109 GHz - 2.5445 GHz | `band` | `land-mobile` | `land-mobile` | invalid reversed frequency span |
| 91 | Reti punto-punto e punto-multipunto | `comms` | 2.5445 GHz - 2.6117 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 92 | LPR, SRD e SRR | `utility` | 2.6117 GHz - 2.65 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 93 | Comunicazioni elettroniche terrestri | `comms` | 26.5 GHz - 27.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 94 | Reti punto-punto e punto-multipunto | `comms` | 27.5 GHz - 29.1 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 95 | Reti punto-punto e punto-multipunto | `comms` | 29.1 GHz - 29.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 96 | Reti punto-punto e punto-multipunto | `comms` | 31 GHz - 31.3 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 97 | Reti fisse numeriche ad alta densità | `comms` | 31.983 GHz - 32.599 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 98 | Reti fisse numeriche ad alta densità | `comms` | 32.795 GHz - 33.4 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 99 | Reti fisse numeriche ad alta densità | `comms` | 37.338 GHz - 38.3 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 100 | Reti fisse numeriche ad alta densità | `comms` | 38.59 GHz - 39.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 101 | Sistemi fissi via radio FWS | `utility` | 40.5 GHz - 43.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 102 | Reti fisse numeriche ad alta densità | `comms` | 51.4 GHz - 52.6 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 103 | Reti fisse numeriche ad alta densità | `comms` | 55.78 GHz - 61 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 104 | Reti fisse numeriche ad alta densità | `comms` | 64 GHz - 66 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 105 | Collegamenti fissi ad alta capacità | `comms` | 71 GHz - 74 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 106 | LPR, SRD, SRR, TLPR, radar veicoli | `utility` | 74 GHz - 76.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 107 | Collegamenti fissi ad alta capacità | `comms` | 84 GHz - 86 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |

### Netherlands (`netherlands.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | Unallocated | `utility` | 0 Hz - 8.3 kHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 2 | Weather (LDS) | `utility` | 8.3 kHz - 9 kHz | `band` | `meteorological` | `meteorological` | service has no stable frequency-band catalog |
| 3 | Radionavigation / Weather (LDS) | `utility` | 9 kHz - 11.3 kHz | `band` | `navigation` | `navigation` | no stable band mapping |
| 4 | Radionavigation | `marine` | 11.3 kHz - 14 kHz | `band` | `navigation` | `navigation` | no stable band mapping |
| 5 | Maritime Mobile Service | `marine` | 14 kHz - 19.95 kHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 6 | 20KHz Time Signal | `utility` | 19.95 kHz - 20.05 kHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 7 | Maritime Mobile Service | `marine` | 20.05 kHz - 76.85 kHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 8 | DCF77 (DE) | `utility` | 76.85 kHz - 78.15 kHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 9 | Maritime Mobile Service | `marine` | 78.15 kHz - 123.6 kHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 10 | RFID (LF) | `utility` | 123.6 kHz - 135.7 kHz | `band` | `ism` | `ism` | no stable band mapping |
| 11 | Maritime Mobile Service | `marine` | 137.8 kHz - 148.5 kHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 12 | Long Wave / Aeronautical Radionavigation | `broadcast` | 255 kHz - 283.5 kHz | `band` | `navigation` | `navigation` | no stable band mapping |
| 13 | Aeronautical Radionavigation / Maritime | `aviation` | 479 kHz - 526.5 kHz | `band` | `navigation` | `navigation` | no stable band mapping |
| 14 | Maritime Mobile Service | `marine` | 1.6065 MHz - 1.625 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 15 | Radiolocation Beacons | `military` | 1.625 MHz - 1.635 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 16 | Maritime Mobile Service | `marine` | 1.635 MHz - 1.8 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 17 | Radiolocation Beacons | `military` | 1.8 MHz - 1.81 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 18 | Aeronautical Mobile Service | `aviation` | 1.88 MHz - 2.025 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 19 | Aeronautical Mobile Service / Weather aux. | `aviation` | 2.025 MHz - 2.045 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 20 | Maritime Mobile Service | `marine` | 2.045 MHz - 2.16 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 21 | Radiolocation Beacons | `aviation` | 2.16 MHz - 2.17 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 22 | Maritime Mobile Service | `marine` | 2.17 MHz - 2.1735 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 23 | Distress and Calling for Maritime and Aeronautical | `aviation` | 2.1735 MHz - 2.1905 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 24 | Maritime Mobile | `marine` | 2.1905 MHz - 2.194 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 25 | Aeronautical Mobile | `aviation` | 2.194 MHz - 2.3 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 26 | Standard Frequency and Time Signal | `utility` | 2.498 MHz - 2.501 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 27 | Standard Frequency and Time Signal / Spatial research | `utility` | 2.501 MHz - 2.502 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 28 | Aeronautical Mobile Service | `aviation` | 2.502 MHz - 2.625 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 29 | Maritime Mobile Service | `marine` | 2.625 MHz - 2.65 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 30 | Aeronautical Mobile Service | `aviation` | 2.65 MHz - 2.85 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 31 | Maritime Mobile | `marine` | 3.155 MHz - 3.2 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 32 | Standard Frequency and Time Signal | `utility` | 4.995 MHz - 5.003 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 33 | Standard Frequency and Time Signal / Spatial research | `utility` | 5.003 MHz - 5.005 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 34 | Maritime Mobile/ Private Land Mobile | `aviation` | 5.06 MHz - 5.3305 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 35 | Distress | `aviation` | 6.21375 MHz - 6.21625 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 36 | Long Distance Communications | `aviation` | 6.795 MHz - 7 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 37 | Maritime | `marine` | 7.45 MHz - 7.6335 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 38 | MARS | `marine` | 7.6335 MHz - 7.6365 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 39 | Maritime | `marine` | 7.6365 MHz - 7.85 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 40 | (Ship/Shore) Maritime | `marine` | 7.85 MHz - 8.28975 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 41 | Maritime Mobile/ Private Land Mobile | `marine` | 9.108 MHz - 9.4 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 42 | Private Land Mobile | `utility` | 9.9 MHz - 9.995 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 43 | Standard Frequency and Time Signal | `utility` | 9.995 MHz - 10.003 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 44 | Standard Frequency and Time Signal / Spatial research | `utility` | 10.003 MHz - 10.005 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 45 | MARS | `marine` | 11.407 MHz - 11.41 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 46 | Aeronautical | `aviation` | 11.41 MHz - 11.6 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 47 | (Ship/Shore) Maritime | `aviation` | 12.1 MHz - 12.28875 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 48 | Distress | `aviation` | 12.28875 MHz - 12.29125 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 49 | (Ship/Shore) Maritime | `aviation` | 12.29125 MHz - 12.7879 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 50 | Radio Astronomy | `utility` | 13.36 MHz - 13.41 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 51 | (Ship/Shore) Maritime | `aviation` | 13.41 MHz - 13.553 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 52 | 22m (HiFER) Band and 22m ISM | `amateur` | 13.553 MHz - 13.567 MHz | `band` | `amateur` | `amateur` | no stable band mapping |
| 53 | 22m ISM Band | `utility` | 13.567 MHz - 13.57 MHz | `band` | `ism` | `ism` | no stable band mapping |
| 54 | Fixed and Aero mobile | `utility` | 13.87 MHz - 14 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 55 | Private Land Mobile | `utility` | 14.35 MHz - 14.995 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 56 | Standard Frequency and Time Signal | `utility` | 14.995 MHz - 15.005 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 57 | (Ship/Shore) Maritime | `marine` | 15.8 MHz - 17.48 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 58 | (Ship/Shore) Maritime/ Fixed Service | `marine` | 18.168 MHz - 18.9 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 59 | (Ship/Shore) Maritime | `marine` | 19.02 MHz - 19.995 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 60 | Standard Frequency and Time Signal | `utility` | 19.995 MHz - 20.005 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 61 | Private Land Mobile | `utility` | 20.005 MHz - 21 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 62 | Maritime | `marine` | 24.99 MHz - 24.995 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 63 | Standard Frequency and Time Signal | `utility` | 24.995 MHz - 25.005 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 64 | Radio Astronomy | `utility` | 25.55 MHz - 25.67 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 65 | Maritime | `marine` | 26.1 MHz - 26.957 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 66 | Maritime | `marine` | 27.283 MHz - 27.41 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 67 | Meteorological Aids | `utility` | 27.41 MHz - 28 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 68 | Military, microphones, radiocommands, PMR, medical implants | `utility` | 29.7 MHz - 40.66 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 69 | Fixed Mobile/ Maritime | `marine` | 40.7 MHz - 50 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 70 | Land Military / PMR | `utility` | 52 MHz - 70 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 71 | Miltary, PMR/PAMR | `utility` | 70.5 MHz - 73 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 72 | Radio Astronomy | `utility` | 73 MHz - 74.6 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 73 | Radionavigation | `aviation` | 74.6 MHz - 75.2 MHz | `band` | `navigation` | `navigation` | individual channel or narrow channel window; not a band |
| 74 | Miltary, PMR/PAMR | `utility` | 75.2 MHz - 87.5 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 75 | Aeronautical Military Systems | `military` | 138 MHz - 144 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 76 | ISS Voice Uplink + Beacons | `satellite` | 144.48 MHz - 144.5 MHz | `segment` | `satellite` | `satellite` | no stable band mapping |
| 77 | ISS Voice/ SSTV Downlink | `satellite` | 145.79 MHz - 145.81 MHz | `segment` | `satellite` | `satellite` | no stable band mapping |
| 78 | Satellite in 2m Ham Band\| | `satellite1` | 145.81 MHz - 146 MHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 79 | Fixed Mobile | `marine` | 148 MHz - 155.9875 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 80 | DAB+ radio | `broadcast` | 174 MHz - 230 MHz | `band` | `broadcast` | `sound-broadcast` | no stable band mapping |
| 81 | Defence Systems | `military` | 230 MHz - 242.95 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 82 | EPIRBs | `utility` | 242.95 MHz - 243.05 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 83 | Defence Systems | `military` | 243.05 MHz - 322 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 84 | Radio Astronomy / Defence | `utility` | 322 MHz - 328.6 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 85 | Defence Systems, PMR/PMAR/PPDR | `military` | 335.4 MHz - 399.9 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 86 | MSS Earth stations | `satellite` | 399.9 MHz - 400.05 MHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 87 | Standard Frequency and Time Signal-Satellite Service | `satellite` | 400.05 MHz - 400.15 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 88 | (NVNG)/ Mobile Satellite Service (MSS) | `satellite` | 400.15 MHz - 401 MHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 89 | Earth Exploration Satellite | `satellite1` | 401 MHz - 403 MHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 90 | Meteorological and Medical Aids | `utility` | 403 MHz - 406 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 91 | Distress Beacons (S and RSAT\| NOAAs 15, 18, 19) | `aviation` | 406 MHz - 406.1 MHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 92 | Radio Astronomy | `utility` | 406.1 MHz - 410 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 93 | Land Maritime Military | `military` | 410 MHz - 430 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 94 | Satellite in 70cm Ham Band | `satellite` | 434.79 MHz - 438 MHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 95 | Land Maritime Military | `military` | 440 MHz - 446.0125 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 96 | Land Maritime Military | `military` | 446.1875 MHz - 450 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 97 | Land Mobile, Paging, PMR/PAMR | `utility` | 450 MHz - 465.9865 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 98 | Metops' DCP configuration control | `satellite` | 465.9865 MHz - 465.9885 MHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 99 | Land Mobile, Paging, PMR/PAMR | `utility` | 465.9885 MHz - 470 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 100 | Radio Astronomy (Ch.37) | `utility` | 608 MHz - 614 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 101 | Mobile Communications | `utility` | 790 MHz - 862 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 102 | Mobile Communications | `utility` | 870 MHz - 876 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 103 | Mobile radiolocation | `utility1` | 915 MHz - 921 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 104 | Aviation | `aviation` | 960 MHz - 1.164 GHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 105 | Satellite | `satellite1` | 1.164 GHz - 1.24 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 106 | Radio Astronomy | `utility` | 1.3 GHz - 1.4 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 107 | Earth Exploration-Satellite/ Radio Astronomy | `satellite` | 1.4 GHz - 1.420405 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 108 | Hygrogen Line | `utility` | 1.420405 GHz - 1.420407 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 109 | Earth Exploration-Satellite/ Radio Astronomy | `satellite` | 1.420407 GHz - 1.427 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 110 | Fixed Mobile | `utility` | 1.427 GHz - 1.452 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 111 | T-DAB | `broadcast` | 1.452 GHz - 1.492 GHz | `band` | `broadcast` | `sound-broadcast` | no stable band mapping |
| 112 | Fixed and Military | `utility` | 1.492 GHz - 1.525 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 113 | Satellite | `satellite` | 1.525 GHz - 1.6605 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 114 | Radio Astronomy | `utility` | 1.6605 GHz - 1.6684 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 115 | Meteorological Aids and Radio Astronomy | `utility` | 1.6684 GHz - 1.67 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 116 | Government Use Meteorological-Satellite | `satellite` | 1.67 GHz - 1.675 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 117 | Meteorological Satellite | `satellite1` | 1.675 GHz - 1.6965 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 118 | Meteorological Satellite | `satellite` | 1.6965 GHz - 1.71 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 119 | Fixed Mobile | `utility1` | 1.785 GHz - 1.805 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 120 | DECT | `utility` | 1.88 GHz - 1.9 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 121 | Mobile | `utility1` | 1.9 GHz - 1.98 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 122 | Earth-to-Space and Space-to-Space Communications | `satellite1` | 1.98 GHz - 2.01 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 123 | Mobile | `utility` | 2.01 GHz - 2.025 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 124 | Earth-to-Space and Space-to-Space Communications | `satellite1` | 2.025 GHz - 2.11 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 125 | Earth-to-Space (Deep space) | `satellite1` | 2.11 GHz - 2.12 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 126 | Radio Astronomy | `utility` | 2.29 GHz - 2.3 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 127 | IMT | `cellular` | 2.5 GHz - 2.5445 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 128 | Radioastronomy | `utility` | 2.69 GHz - 2.7 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 129 | Radar meteo | `military` | 2.7 GHz - 2.9 GHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 130 | Maritime Radar | `marine` | 2.9 GHz - 3.4 GHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 131 | Digital Networks | `utility` | 3.475 GHz - 4.2 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 132 | Altimeters | `aviation` | 4.2 GHz - 4.4 GHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 133 | Feeder link | `satellite` | 5.15 GHz - 5.25 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 134 | Digital Networks and LPR | `utility` | 5.925 GHz - 7.75 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 135 | LPR | `utility` | 7.75 GHz - 7.975 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 136 | Remote sensing | `utility` | 7.975 GHz - 8.215 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 137 | TLPR and SRD | `utility` | 8.215 GHz - 8.65 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 138 | Radar Doppler | `aviation` | 8.65 GHz - 8.85 GHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 139 | Maritime Radar | `marine` | 8.85 GHz - 9 GHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 140 | Radar and transponder SART | `marine` | 9 GHz - 9.5 GHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 141 | TLPR and SRD | `utility` | 9.5 GHz - 10 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 142 | Point to point TV networks | `utility` | 10.5 GHz - 10.68 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 143 | Digital network (fixed) | `utility` | 10.68 GHz - 11.7 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 144 | TV satellite | `satellite` | 11.7 GHz - 12.5 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 145 | Digital network (fixed) | `utility` | 12.5 GHz - 13.25 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 146 | Satellite Uplink | `satellite` | 14 GHz - 14.5 GHz | `segment` | `satellite` | `satellite` | no stable band mapping |
| 147 | Digital network (fixed) | `utility` | 14.5 GHz - 14.62 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 148 | Digital network (fixed) | `utility` | 15.23 GHz - 15.35 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 149 | Digital network (poit to point) | `utility` | 17.1 GHz - 19.3 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 150 | Feeder link | `satellite` | 19.3 GHz - 19.7 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 151 | HEST, LEST, ESIM, ESOMP | `satellite` | 19.7 GHz - 20.2 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 152 | Digital network (fixed) | `utility` | 22 GHz - 22.33 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 153 | Digital network (fixed) | `utility` | 22.67475 GHz - 2.28335 GHz | `band` | `land-mobile` | `land-mobile` | invalid reversed frequency span |
| 154 | Digital network (fixed), SAP/SAB | `utility` | 22.92675 GHz - 23.15 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 155 | Digital network (fixed) | `utility` | 23.15 GHz - 23.338 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 156 | Digital network (point to point, multipoint) | `utility` | 24.45 GHz - 25.109 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 157 | LPR, SRD and SRR | `utility` | 25.109 GHz - 2.5445 GHz | `band` | `land-mobile` | `land-mobile` | invalid reversed frequency span |
| 158 | Network (point to point, multipoint) | `utility` | 2.5445 GHz - 2.6117 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 159 | LPR, SRD and SRR | `utility` | 2.6117 GHz - 2.65 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 160 | Terrestrial electric utility | `utility` | 26.5 GHz - 27.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 161 | Network (point to point, multipoint) | `utility` | 27.5 GHz - 29.1 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 162 | Network (point to point, multipoint) | `utility` | 29.1 GHz - 29.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 163 | Network (point to point, multipoint) | `utility` | 31 GHz - 31.3 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 164 | Network (high density, fixed) | `utility` | 31.983 GHz - 32.599 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 165 | Network (high density, fixed) | `utility` | 32.795 GHz - 33.4 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 166 | Network (high density, fixed) | `utility` | 37.338 GHz - 38.3 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 167 | Network (high density, fixed) | `utility` | 38.59 GHz - 39.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 168 | FWS systems (fixed) | `utility` | 40.5 GHz - 43.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 169 | Network (high density, fixed) | `utility` | 51.4 GHz - 52.6 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 170 | Network (high density, fixed) | `utility` | 55.78 GHz - 61 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 171 | Network (high density, fixed) | `utility` | 64 GHz - 66 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 172 | Links (high density, fixed) | `utility` | 71 GHz - 74 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 173 | LPR, SRD, SRR, TLPR, vehichle radar | `utility` | 74 GHz - 76.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 174 | Links (high density, fixed) | `utility` | 84 GHz - 86 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |

### QO-100 (`qo-100.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | Beacon | `broadcast` | 10.4895 GHz - 10.489505 GHz | `band` | `broadcast` | `sound-broadcast` | no stable band mapping |
| 2 | Beacon | `broadcast` | 10.489745 GHz - 10.489755 GHz | `band` | `broadcast` | `sound-broadcast` | no stable band mapping |
| 3 | Beacon | `broadcast` | 10.48999 GHz - 10.49 GHz | `band` | `broadcast` | `sound-broadcast` | no stable band mapping |

### Republic of Korea (`republic-of-korea.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | Radio Navigation | `aviation` | 8.3 kHz - 14 kHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 2 | Coastal Telegraph | `marine` | 14 kHz - 19.95 kHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 3 | Standard Frequency Time Signal | `utility` | 19.95 kHz - 20.25 kHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 4 | Coastal Telegraph | `marine` | 20.25 kHz - 70 kHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 5 | Radio Navigation | `navigation` | 70 kHz - 160 kHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 6 | Aviation Radio Navigation | `aviation` | 160 kHz - 285 kHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 7 | Aviation Maritime Radiobeacon | `aviation` | 285 kHz - 325 kHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 8 | Aviation Radio Navigation | `aviation` | 325 kHz - 472 kHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 9 | International Distress Safety Call | `marine` | 479 kHz - 505 kHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 10 | Radiobuoy | `navigation` | 1.6065 MHz - 1.8 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 11 | Radiobuoy Control LORAN | `radiolocation` | 1.825 MHz - 2 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 12 | Radiobuoy | `fixed` | 2 MHz - 2.065 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 13 | Distress Call | `marine` | 2.065 MHz - 2.107 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 14 | International Distress Search and Rescue | `mobile` | 2.1735 MHz - 2.1905 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 15 | Road Management | `fixed` | 2.194 MHz - 2.495 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 16 | Standard Frequency Time Signal | `utility` | 2.495 MHz - 2.505 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 17 | Ship Station Telephone | `fixed` | 2.505 MHz - 2.85 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 18 | Experimental Station | `fixed` | 3.55 MHz - 3.79 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 19 | Standard Frequency Time Signal | `utility` | 3.995 MHz - 4.005 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 20 | Ship Station Telephone | `marine` | 4.005 MHz - 4.063 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 21 | Radiolocation | `radiolocation` | 4.438 MHz - 4.488 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 22 | Calling Response | `fixed` | 4.488 MHz - 4.65 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 23 | Standard Frequency Time Signal | `utility` | 4.995 MHz - 5.005 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 24 | Standard Frequency Time Signal | `utility` | 7.995 MHz - 8.005 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 25 | Standard Frequency Time Signal | `utility` | 9.995 MHz - 10.005 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 26 | Radio Astronomy | `astronomy` | 13.36 MHz - 13.41 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 27 | Standard Frequency Time Signal | `utility` | 15.995 MHz - 16.005 MHz | `channel` | `time-standard` | `time-standard` | individual channel/bookmark; not a band |
| 28 | Flood Warning | `broadcast` | 72 MHz - 74.8 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 29 | General Communication | `fixed` | 146 MHz - 148 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 30 | Low Power Device | `fixed` | 162.0375 MHz - 174 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 31 | Low Power Device | `fixed` | 216 MHz - 230 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 32 | Low Power Device | `fixed` | 273 MHz - 322 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 33 | Personal Radio | `fixed` | 420 MHz - 470 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 34 | Public Network | `broadcast` | 698 MHz - 806 MHz | `band` | `broadcast` | `sound-broadcast` | no stable band mapping |
| 35 | Low Power Device | `fixed` | 942 MHz - 960 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 36 | Satellite Mobile Communication | `fixed` | 15.25 GHz - 16.605 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 37 | Mobile Communication | `mobile` | 25 GHz - 37 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |

### Russia (`russia.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | Train communications | `railway` | 151.7125 MHz - 156.0125 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 2 | Military Air | `military` | 225 MHz - 240 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 3 | Military Sat / Military Air | `military` | 240 MHz - 270 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 4 | Military Air | `military` | 270 MHz - 380 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 5 | LTE-FDD-450 Uplink | `broadcast` | 451 MHz - 456 MHz | `segment` | `cellular` | `cellular-lte` | no stable band mapping |
| 6 | LTE-FDD-450 Downlink | `broadcast` | 461 MHz - 466 MHz | `segment` | `cellular` | `cellular-lte` | no stable band mapping |
| 7 | L-Band | `broadcast` | 1 GHz - 1.089998 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |
| 8 | ADS-B / L-Band | `broadcast` | 1.089998 GHz - 1.090002 GHz | `channel` | `aviation` | `aviation-surveillance` | individual channel/bookmark; not a band |
| 9 | L-Band | `broadcast` | 1.090002 GHz - 1.26 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |
| 10 | L-Band | `broadcast` | 1.3 GHz - 1.71 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |
| 11 | L-Band | `broadcast` | 1.785 GHz - 1.805 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |
| 12 | DECT Phones / L-Band | `broadcast` | 1.88 GHz - 1.9 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 13 | L-Band | `broadcast` | 1.9 GHz - 1.92 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |
| 14 | UMTS-FDD Uplink / L-Band | `broadcast` | 1.92 GHz - 1.98 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 15 | L-Band | `broadcast` | 1.98 GHz - 2 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |
| 16 | S-Band | `broadcast` | 2 GHz - 2.01 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |
| 17 | UMTS-TDD / S-Band | `broadcast` | 2.01 GHz - 2.025 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 18 | S-Band | `broadcast` | 2.025 GHz - 2.11 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |
| 19 | UMTS-FDD Downlink / S-Band | `broadcast` | 2.11 GHz - 2.17 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 20 | S-Band | `broadcast` | 2.17 GHz - 2.32 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |
| 21 | S-Band | `broadcast` | 2.32015 GHz - 2.4 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |
| 22 | S-Band | `broadcast` | 2.4835 GHz - 2.5 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |
| 23 | S-Band | `broadcast` | 2.69 GHz - 4 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |
| 24 | C-Band | `broadcast` | 4 GHz - 5.15 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |
| 25 | C-Band | `broadcast` | 5.35 GHz - 5.65 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |
| 26 | C-Band | `broadcast` | 5.85 GHz - 5.65 GHz | `spectrum-range` | `other` | `spectrum` | invalid reversed frequency span |
| 27 | C-Band | `broadcast` | 5.85 GHz - 8 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |
| 28 | X-Band | `broadcast` | 8 GHz - 10 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |
| 29 | X-Band | `broadcast` | 10.5 GHz - 12 GHz | `spectrum-range` | `other` | `spectrum` | service-independent spectrum range; not a service band |

### Turkey (`turkey.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | Pagers | `amateur` | 27.75 MHz - 28 MHz | `band` | `amateur` | `amateur` | no stable band mapping |
| 2 | Sayac Okuma | `other` | 169.4 MHz - 169.475 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 3 | Pagers | `other` | 167 MHz - 167.1 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 4 | Public announcement systems | `other` | 173.8825 MHz - 174 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 5 | T-DAB | `broadcast` | 216 MHz - 233 MHz | `band` | `broadcast` | `sound-broadcast` | no stable band mapping |
| 6 | Public Safety/Emergency | `other` | 380 MHz - 385 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 7 | Public Safety/Emergency | `other` | 390 MHz - 395 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 8 | Public announcement systems | `other` | 445.25 MHz - 445.4625 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 9 | DECT | `other` | 1.88 GHz - 1.9 GHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 10 | 3cm | `amateur` | 104.5 GHz - 104.52 GHz | `band` | `amateur` | `amateur` | no stable band mapping |
| 11 | 75GHz | `amateur` | 75.5 GHz - 7.6 GHz | `band` | `amateur` | `amateur` | invalid reversed frequency span |

### UK (`united-kingdom.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | Air/Marine Nav Beacons | `aviation` | 283.5 kHz - 526.5 kHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 2 | Maritime | `marine` | 2.5 MHz - 2.85 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 3 | Maritime | `marine` | 16.36 MHz - 17.41 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 4 | Maritime - ship tx | `marine` | 18.78 MHz - 18.9 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 5 | Maritime - coast tx | `marine` | 19.68 MHz - 19.99 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 6 | 11m Broadcast | `broadcast` | 256.7 MHz - 26.1 MHz | `band` | `broadcast` | `sound-broadcast` | invalid reversed frequency span |
| 7 | Analogue Cordless Phones | `amateur` | 31.0375 MHz - 40.1125 MHz | `band` | `amateur` | `amateur` | no stable band mapping |
| 8 | Low Power Devices | `amateur` | 49.82 MHz - 49.9875 MHz | `band` | `amateur` | `amateur` | no stable band mapping |
| 9 | Land/Mountain Rescue | `PMR` | 147.34375 MHz - 147.5 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 10 | Satellites | `satellite` | 148 MHz - 150.05 MHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 11 | Pagers - Flex/POCSAG | `PMR` | 153.025 MHz - 153.5 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 12 | Land/Mountain Rescue DMR | `PMR` | 155 MHz - 156 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 13 | Short Term Hire | `PMR` | 158.7875 MHz - 159.6875 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 14 | Short Term Hire | `PMR` | 163.2875 MHz - 164.2 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 15 | Business Radio | `PMR` | 165 MHz - 174 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 16 | DAB Radio | `broadcast` | 174 MHz - 230 MHz | `band` | `broadcast` | `sound-broadcast` | no stable band mapping |
| 17 | Military Air | `military` | 230 MHz - 400 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 18 | Satellites | `satellite` | 399.9 MHz - 401 MHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 19 | Weather Balloons | `aviation` | 401 MHz - 406 MHz | `band` | `meteorological` | `meteorological` | service has no stable frequency-band catalog |
| 20 | Private Mobile Radio inc trams | `PMR` | 422 MHz - 424 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 21 | Outside Broadcast Talkback | `PMR` | 446.2 MHz - 447.5125 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 22 | Private Mobile Radio | `PMR` | 447.6 MHz - 454 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 23 | Mosques | `amateur` | 454 MHz - 455 MHz | `band` | `amateur` | `amateur` | no stable band mapping |
| 24 | Private Mobile Radio inc OB | `PMR` | 455 MHz - 470 MHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 25 | Cell phones | `cellular` | 703 MHz - 788 MHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 26 | Band 20 Cell phone downlink | `cellular` | 791 MHz - 821 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 27 | Band 20 Cell phone uplink | `cellular` | 832 MHz - 862 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 28 | Licence Exempt Short Range | `amateur` | 862 MHz - 875.8 MHz | `band` | `amateur` | `amateur` | no stable band mapping |
| 29 | Band 8 Cell phone uplink | `cellular` | 880.1 MHz - 914.9 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 30 | Band 8 Cell phone downlink | `cellular` | 925.1 MHz - 929.5 MHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 31 | Satellite L-band | `satellite` | 1.518 GHz - 1.559 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 32 | Satellite L-band | `satellite` | 1.6265 GHz - 1.6605 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 33 | Satellite L-band | `satellite` | 1.668 GHz - 1.675 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 34 | Band 3 Cell phone uplink | `cellular` | 1.71 GHz - 1.785 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 35 | Band 3 Cell phone downlink | `cellular` | 1.8051 GHz - 1.88 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 36 | DECT cordless phones | `cellular` | 1.88 GHz - 1.9 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 37 | Band 3 Cell phones | `cellular` | 1.9 GHz - 1.92 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 38 | Band 1 Cell phone uplink | `cellular` | 1.92 GHz - 1.9797 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 39 | Band 1 Cell phone downlink | `cellular` | 2.1103 GHz - 2.1697 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 40 | Band 38 Cell phones | `cellular` | 2.5 GHz - 269 MHz | `band` | `cellular` | `cellular-other` | invalid reversed frequency span |
| 41 | Band 42 5G Cell phones | `cellular` | 3.41 GHz - 3.72 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 42 | ISM - wifi | `ISM` | 5.15 GHz - 5.85 GHz | `band` | `rlan` | `rlan` | composite span crosses multiple stable bands |

### USA (`usa.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | Military Air | `military` | 225 MHz - 380 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 2 | Military Sat | `military` | 240 MHz - 270 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
