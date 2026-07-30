# Legacy band ID conversion audit

Generated from `root/res/bandplans/*.json` using the current `classifyLegacyBand()` and `findLegacyBandMapping()` implementation.

- Legacy files: 21
- Legacy rows: 1654
- Rows assigned a stable band ID: 1159
- Rows without a stable band ID: 495

## Deliberate mapping revisions

- Removed `band:time-standard:lf`: it grouped isolated 20 and 77.5 kHz channels rather than an enclosing band.
- Removed `band:time-standard:hf`: it grouped isolated channels from 2.5 through 25 MHz with dissimilar propagation.
- Removed `band:navigation:marker-75mhz`: the legacy rows describe the 75 MHz marker channel/window, not a channelized navigation band.
- Replaced `band:aviation:adsb-dme-tacan` with `band:aviation:l-band`; its probes identify the enclosing L-band and deliberately do not turn narrow ADS-B channel rows into bands.
- Split `band:aviation:hf:3mhz` into the distinct `band:aviation:hf:3.4mhz` and `band:aviation:hf:3.8mhz` bands.
- Split `band:ism:5ghz` into lower, middle, and upper 5 GHz bands. Composite legacy rows crossing more than one are intentionally left without an ID.
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
| `cellular` | 32 | 79 |
| `ism` | 17 | 3 |
| `land-mobile` | 0 | 133 |
| `maritime` | 51 | 61 |
| `meteorological` | 0 | 3 |
| `navigation` | 18 | 6 |
| `other` | 0 | 81 |
| `personal-radio` | 34 | 0 |
| `rlan` | 7 | 5 |
| `satellite` | 12 | 45 |
| `time-standard` | 0 | 24 |

## Summary by classified family

| Family | Assigned | Without ID |
|---|---:|---:|
| `amateur` | 630 | 8 |
| `aviation-communication` | 88 | 29 |
| `aviation-surveillance` | 1 | 2 |
| `cellular-gsm` | 17 | 2 |
| `cellular-lte` | 15 | 6 |
| `cellular-other` | 0 | 71 |
| `ism` | 17 | 3 |
| `land-mobile` | 0 | 133 |
| `maritime` | 51 | 61 |
| `meteorological` | 0 | 3 |
| `navigation` | 18 | 6 |
| `personal-radio` | 34 | 0 |
| `rlan` | 7 | 5 |
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
| composite span crosses multiple stable bands | 5 |
| individual channel or narrow channel window; not a band | 1 |
| individual channel/bookmark; not a band | 27 |
| invalid reversed frequency span | 13 |
| no stable band mapping | 240 |
| service has no stable frequency-band catalog | 191 |
| service-independent spectrum range; not a service band | 18 |

## Legacy rows without a stable band ID


### Australia (`australia.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | 27MHz Marine Band | `marine` | 27.68 MHz - 27.98 MHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 2 | Digital Radio Broadcast (DAB+) | `broadcast` | 195 MHz - 209 MHz | `band` | `broadcast` | `sound-broadcast` | no stable band mapping |
| 3 | 5GHz WiFi (ISM Band) | `other` | 5.15 GHz - 5.59 GHz | `band` | `rlan` | `rlan` | composite span crosses multiple stable bands |
| 4 | 5GHz WiFi (ISM Band) | `other` | 5.65 GHz - 5.835 GHz | `band` | `rlan` | `rlan` | composite span crosses multiple stable bands |

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
| 67 | Reti numeriche e 802.11 | `comms` | 5.25 GHz - 5.65 GHz | `band` | `rlan` | `rlan` | composite span crosses multiple stable bands |
| 68 | Reti numeriche e LPR | `comms` | 5.925 GHz - 7.75 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 69 | LPR | `comms` | 7.75 GHz - 7.975 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 70 | Telerilevamento | `utility` | 7.975 GHz - 8.215 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 71 | TLPR e SRD | `comms` | 8.215 GHz - 8.65 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 72 | Radar Doppler | `aircraft` | 8.65 GHz - 8.85 GHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 73 | Radar marittimi | `marine` | 8.85 GHz - 9 GHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 74 | Radar e transponder SART | `marine` | 9 GHz - 9.5 GHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 75 | TLPR e SRD | `utility` | 9.5 GHz - 10 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 76 | Reti punto-punto televisive | `comms` | 10.5 GHz - 10.68 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 77 | Reti fisse numeriche | `comms` | 10.68 GHz - 11.7 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 78 | Satelliti televisivi | `satellite` | 11.7 GHz - 12.5 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 79 | Reti fisse numeriche | `comms` | 12.5 GHz - 13.25 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 80 | Uplink satellitari | `satellite` | 14 GHz - 14.5 GHz | `segment` | `satellite` | `satellite` | no stable band mapping |
| 81 | Rete fisse numeriche | `comms` | 14.5 GHz - 14.62 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 82 | Rete fisse numeriche | `comms` | 15.23 GHz - 15.35 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 83 | Reti numeriche punto-punto | `comms` | 17.1 GHz - 19.3 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 84 | Feeder link | `satellite` | 19.3 GHz - 19.7 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 85 | HEST, LEST, ESIM, ESOMP | `satellite` | 19.7 GHz - 20.2 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 86 | Reti fisse numeriche | `comms` | 22 GHz - 22.33 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 87 | Reti fisse numeriche | `comms` | 22.67475 GHz - 2.28335 GHz | `band` | `land-mobile` | `land-mobile` | invalid reversed frequency span |
| 88 | Reti fisse numeriche, SAP/SAB | `comms` | 22.92675 GHz - 23.15 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 89 | Reti fisse numeriche | `comms` | 23.15 GHz - 23.338 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 90 | Reti punto-punto e punto-multipunto | `comms` | 24.45 GHz - 25.109 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 91 | LPR, SRD e SRR | `utility` | 25.109 GHz - 2.5445 GHz | `band` | `land-mobile` | `land-mobile` | invalid reversed frequency span |
| 92 | Reti punto-punto e punto-multipunto | `comms` | 2.5445 GHz - 2.6117 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 93 | LPR, SRD e SRR | `utility` | 2.6117 GHz - 2.65 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 94 | Comunicazioni elettroniche terrestri | `comms` | 26.5 GHz - 27.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 95 | Reti punto-punto e punto-multipunto | `comms` | 27.5 GHz - 29.1 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 96 | Reti punto-punto e punto-multipunto | `comms` | 29.1 GHz - 29.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 97 | Reti punto-punto e punto-multipunto | `comms` | 31 GHz - 31.3 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 98 | Reti fisse numeriche ad alta densità | `comms` | 31.983 GHz - 32.599 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 99 | Reti fisse numeriche ad alta densità | `comms` | 32.795 GHz - 33.4 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 100 | Reti fisse numeriche ad alta densità | `comms` | 37.338 GHz - 38.3 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 101 | Reti fisse numeriche ad alta densità | `comms` | 38.59 GHz - 39.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 102 | Sistemi fissi via radio FWS | `utility` | 40.5 GHz - 43.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 103 | Reti fisse numeriche ad alta densità | `comms` | 51.4 GHz - 52.6 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 104 | Reti fisse numeriche ad alta densità | `comms` | 55.78 GHz - 61 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 105 | Reti fisse numeriche ad alta densità | `comms` | 64 GHz - 66 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 106 | Collegamenti fissi ad alta capacità | `comms` | 71 GHz - 74 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 107 | LPR, SRD, SRR, TLPR, radar veicoli | `utility` | 74 GHz - 76.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 108 | Collegamenti fissi ad alta capacità | `comms` | 84 GHz - 86 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |

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
| 134 | Digital networks and 802.11 | `utility` | 5.25 GHz - 5.65 GHz | `band` | `rlan` | `rlan` | composite span crosses multiple stable bands |
| 135 | Digital Networks and LPR | `utility` | 5.925 GHz - 7.75 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 136 | LPR | `utility` | 7.75 GHz - 7.975 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 137 | Remote sensing | `utility` | 7.975 GHz - 8.215 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 138 | TLPR and SRD | `utility` | 8.215 GHz - 8.65 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 139 | Radar Doppler | `aviation` | 8.65 GHz - 8.85 GHz | `band` | `aviation` | `aviation-communication` | no stable band mapping |
| 140 | Maritime Radar | `marine` | 8.85 GHz - 9 GHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 141 | Radar and transponder SART | `marine` | 9 GHz - 9.5 GHz | `band` | `maritime` | `maritime` | no stable band mapping |
| 142 | TLPR and SRD | `utility` | 9.5 GHz - 10 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 143 | Point to point TV networks | `utility` | 10.5 GHz - 10.68 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 144 | Digital network (fixed) | `utility` | 10.68 GHz - 11.7 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 145 | TV satellite | `satellite` | 11.7 GHz - 12.5 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 146 | Digital network (fixed) | `utility` | 12.5 GHz - 13.25 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 147 | Satellite Uplink | `satellite` | 14 GHz - 14.5 GHz | `segment` | `satellite` | `satellite` | no stable band mapping |
| 148 | Digital network (fixed) | `utility` | 14.5 GHz - 14.62 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 149 | Digital network (fixed) | `utility` | 15.23 GHz - 15.35 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 150 | Digital network (poit to point) | `utility` | 17.1 GHz - 19.3 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 151 | Feeder link | `satellite` | 19.3 GHz - 19.7 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 152 | HEST, LEST, ESIM, ESOMP | `satellite` | 19.7 GHz - 20.2 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 153 | Digital network (fixed) | `utility` | 22 GHz - 22.33 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 154 | Digital network (fixed) | `utility` | 22.67475 GHz - 2.28335 GHz | `band` | `land-mobile` | `land-mobile` | invalid reversed frequency span |
| 155 | Digital network (fixed), SAP/SAB | `utility` | 22.92675 GHz - 23.15 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 156 | Digital network (fixed) | `utility` | 23.15 GHz - 23.338 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 157 | Digital network (point to point, multipoint) | `utility` | 24.45 GHz - 25.109 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 158 | LPR, SRD and SRR | `utility` | 25.109 GHz - 2.5445 GHz | `band` | `land-mobile` | `land-mobile` | invalid reversed frequency span |
| 159 | Network (point to point, multipoint) | `utility` | 2.5445 GHz - 2.6117 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 160 | LPR, SRD and SRR | `utility` | 2.6117 GHz - 2.65 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 161 | Terrestrial electric utility | `utility` | 26.5 GHz - 27.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 162 | Network (point to point, multipoint) | `utility` | 27.5 GHz - 29.1 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 163 | Network (point to point, multipoint) | `utility` | 29.1 GHz - 29.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 164 | Network (point to point, multipoint) | `utility` | 31 GHz - 31.3 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 165 | Network (high density, fixed) | `utility` | 31.983 GHz - 32.599 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 166 | Network (high density, fixed) | `utility` | 32.795 GHz - 33.4 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 167 | Network (high density, fixed) | `utility` | 37.338 GHz - 38.3 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 168 | Network (high density, fixed) | `utility` | 38.59 GHz - 39.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 169 | FWS systems (fixed) | `utility` | 40.5 GHz - 43.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 170 | Network (high density, fixed) | `utility` | 51.4 GHz - 52.6 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 171 | Network (high density, fixed) | `utility` | 55.78 GHz - 61 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 172 | Network (high density, fixed) | `utility` | 64 GHz - 66 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 173 | Links (high density, fixed) | `utility` | 71 GHz - 74 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 174 | LPR, SRD, SRR, TLPR, vehichle radar | `utility` | 74 GHz - 76.5 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |
| 175 | Links (high density, fixed) | `utility` | 84 GHz - 86 GHz | `band` | `land-mobile` | `land-mobile` | service has no stable frequency-band catalog |

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
| 31 | Band 32 Cell phone | `cellular` | 1.452 GHz - 1.492 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 32 | Satellite L-band | `satellite` | 1.518 GHz - 1.559 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 33 | Satellite L-band | `satellite` | 1.6265 GHz - 1.6605 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 34 | Satellite L-band | `satellite` | 1.668 GHz - 1.675 GHz | `band` | `satellite` | `satellite` | no stable band mapping |
| 35 | Band 3 Cell phone uplink | `cellular` | 1.71 GHz - 1.785 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 36 | Band 3 Cell phone downlink | `cellular` | 1.8051 GHz - 1.88 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 37 | DECT cordless phones | `cellular` | 1.88 GHz - 1.9 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 38 | Band 3 Cell phones | `cellular` | 1.9 GHz - 1.92 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 39 | Band 1 Cell phone uplink | `cellular` | 1.92 GHz - 1.9797 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 40 | Band 1 Cell phone downlink | `cellular` | 2.1103 GHz - 2.1697 GHz | `segment` | `cellular` | `cellular-other` | no stable band mapping |
| 41 | Band 38 Cell phones | `cellular` | 2.5 GHz - 269 MHz | `band` | `cellular` | `cellular-other` | invalid reversed frequency span |
| 42 | Band 42 5G Cell phones | `cellular` | 3.41 GHz - 3.72 GHz | `band` | `cellular` | `cellular-other` | no stable band mapping |
| 43 | ISM - wifi | `ISM` | 5.15 GHz - 5.85 GHz | `band` | `rlan` | `rlan` | composite span crosses multiple stable bands |

### USA (`usa.json`)

| # | Legacy name | Type | Frequency span | Kind | Service | Family | Reason |
|---:|---|---|---|---|---|---|---|
| 1 | Military Air | `military` | 225 MHz - 380 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
| 2 | Military Sat | `military` | 240 MHz - 270 MHz | `band` | `other` | `unknown` | service has no stable frequency-band catalog |
