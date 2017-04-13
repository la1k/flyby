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
A basic usage guide can be found on https://www.la1k.no/2017/03/03/satellite-tracking-using-flyby/ until this is moved to this source directory.

TLE files
---------

Flyby searches for TLE files in `$BASEDIR/flyby/tles/`. `$BASEDIR` is assumed to be:

* Directory defined in `$XDG_DATA_HOME` (or `$HOME/.local/share/`)
* Directories defined in `$XDG_DATA_DIRS` (or `/usr/local/share/:/usr/share/`)

In other words: As a user, you can put your TLE files in `$HOME/.local/share/flyby/tles/`.
Alternatively, `flyby --add-tle-file [FILENAME]` can be used to place the TLE file in the correct location.

All files are
merged in a single database. User-defined TLEs take precedence over system-wide TLEs. For details,
see documentation for `tle_db_from_search_path()` in `$SOURCE_DIR/src/transponder_db.h`.

The TLE database can automatically be updated using the TLEs available from celestrak.com using `flyby-update-tles`, but note that this only will update existing TLEs, not add new.

Transponder database
--------------------

Transponder database is read from `$BASEDIR/flyby/flyby.db`. `$BASEDIR` is assumed to be the same as above.

As a user, you can put your transponder database in `$HOME/.local/share/flyby/flyby.db`. Flyby provides a transponder editor as a part of its NCurses interface, bypassing the need for editing this file directly.

Alternatively, `flyby-satnogs-fetcher` can be used to fetch the SatNOGS transponder database and merge it with flyby's transponder database.

The database can also be fetched to a named file using `flyby-satnogs-fetcher [FILENAME]`, and can then be added to flyby using `flyby-transponder-dbutil -a [FILENAME]`. 

The transponder database utility `flyby-transponder-dbutil` also has support for various options like silent mode and overriding user prompting for whether differing entries should be overwritten. These options can be reviewed using `flyby-transponder-dbutil --help`. This can be used to write cronjobs for updating the transponder database, like the following:

```
tempfile=$(mktemp)
flyby-satnogs-fetcher $tempfile
flyby-transponder-dbutil --silent --force --add-transponder-file $tempfile
rm $tempfile
```
