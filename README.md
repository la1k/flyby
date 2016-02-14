Flyby
=====

Flyby is a console based satellite tracking program that can track a
satellite across the sky with an antenna and adjust your radio with
uplink and downlink frequency doppler shift.

Satellite orbit parameters are given as NORAD two-line element sets
(TLEs). You can use any rotator controller and radio supported by hamlib
(http://hamlib.org).

Build instructions
------------------

Flyby depends on `libpredict` (currently the latest development version). Follow the build and install instructions on https://github.com/la1k/libpredict. 

Flyby is then built using

```
mkdir build
cd build
cmake ..
make
```

Usage instructions
------------------

Run using `flyby`. See `flyby --help` for command line options. 

TLE files
---------

Flyby searches for TLE files in `$BASEDIR/flyby/tles/`. `$BASEDIR` is assumed to be: 

* Directory defined in `$XDG_DATA_HOME` (or `$HOME/.local/share/`)
* Directories defined in `$XDG_DATA_DIRS` (or `/usr/local/share/:/usr/share/`)

In other words: As a user, you can put your TLE files in `$HOME/.local/share/flyby/tles/`.

All files are
merged in a single database. User-defined TLEs take precedence over system-wide TLEs. For details, 
see documentation for `tle_db_from_search_path()` in `$SOURCE_DIR/src/transponder_db.h`. 

Transponder database
--------------------

Transponder database is read from `$BASEDIR/flyby/flyby.db`. `$BASEDIR` is assumed to be the same as above. 

As a user, you can put your transponder database in `$HOME/.local/share/flyby/flyby.db`. An example can be found in `$SOURCE_DIR/default/flyby.db`.
