Radiosonde decoder plugin for SDRIAK
===================================

![radiosondeGPX](https://user-images.githubusercontent.com/17110004/144872708-2a578c62-5493-4845-9098-9328c4e914bf.png)

Compatibility:
--------------

| Manufacturer | Model       | GPS                | Temperature        | Humidity           | XDATA              |
|--------------|-------------|--------------------|--------------------|--------------------|--------------------|
| Vaisala      | RS41-SG     | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| Meteomodem   | M10         | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |                    |
| Meteomodem   | M20         | :heavy_check_mark: | :heavy_check_mark: |                    |                    |
| GRAW         | DFM06/09/17 | :heavy_check_mark: | :heavy_check_mark: |                    |                    |
| Meisei       | iMS-100     | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |                    |
| Meisei       | RS-11G      | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |                    |
| InterMet     | iMet-1/4    | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| Meteolabor   | SRS-C50     | :heavy_check_mark: | :heavy_check_mark: |                    |                    |
| Meteo-Radiy  | MRZ-N1      | :heavy_check_mark: | :heavy_check_mark: |                    |                    |

Installing
----------

The decoder is bundled with SDRIAK when `OPT_BUILD_RADIOSONDE_DECODER` is
enabled (the default).

- **Windows**: the module is installed in the `modules` directory within the
  SDRIAK installation.
- **Linux**: the module is installed in the SDRIAK plugin directory (normally
  `/usr/local/lib/sdriak/plugins`).

The plugin can then be enabled from the module manager in SDRIAK, under the name
*radiosonde\_decoder*.


Building from source
--------------------

Build SDRIAK normally with `OPT_BUILD_RADIOSONDE_DECODER=ON`, then enable the
module from the module manager.
