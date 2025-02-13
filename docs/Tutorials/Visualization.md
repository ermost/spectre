\cond NEVER
Distributed under the MIT License.
See LICENSE.txt for details.
\endcond
# Running and Visualizing {#tutorial_visualization}

\tableofcontents

### Building an Executable from an Existing Source File

SpECTRE source files for evolution executables are located in
`src/Evolution/Executables`. Executables can be compiled by running the command
`make EXECUTABLE_NAME` where `EXECUTABLE_NAME` is the name of the executable
as defined in the `CMakeLists.txt` file located in the same directory as the
source file. For example, to compile the executable that evolves a scalar wave
using a three-dimensional domain, navigate to `$SPECTRE_HOME/build/`, then
one runs the command: `make EvolveScalarWave3D`, which then results
in an executable of the same name in the `bin` directory of the user's
build directory.

### Running an Evolution Executable

Each SpECTRE executable reads a user-provided YAML file that specifies the
runtime settings of the evolution. This is where options such as the Domain,
AnalyticSolution, TimeStepper, etc are supplied. Example input files are kept
in `tests/InputFiles`, and are written to provide a functional base input file
which the user can then modify as desired. Copy the executable and YAML file
to a directory of your choice. The YAML file is then passed as an argument to
the executable using the flag `--input-file`. For example, for a scalar wave
evolution, run the command:

```
./EvolveScalarWave3D --input-file PlaneWave3D.yaml
```

You can also use the \ref tutorial_cli "command-line interface (CLI)" to run
executables:

```
./spectre run PlaneWave3D.yaml
```

By default, the example input files do not produce any output. This can be
changed by modifying the options passed to `EventsAndTriggers` or
`EventsAndDenseTriggers`:

\snippet PlaneWave1DObserveExample.yaml observe_event_trigger

This will observe the norms of the errors in the system at times
\f$0.0\f$ and \f$1.0\f$ and the field data at the start of every 50th
slab.  A successful observation will result in the creation of H5
files whose names can be specified in the YAML file under the options
`VolumeFileName` and `ReductionFileName`. One volume data file will be
produced from each Charm++ node that is used to run the
executable. Each volume data file will have its corresponding node
number appended to its file name.  Visualization of the volume data
will be described in the next section.

### 3D Data Volume Data In ParaView

A SpECTRE executable with observers produces volume and/or reduced data h5
files. An XDMF file must be created from the volume data in order to do
visualization using ParaView. To this end we provide the tool
`generate-xdmf` in the Python command-line interface. Run it in your build
directory as `spectre generate-xdmf`. It
takes two required arguments which are passed to `--subfile-name`, and
`--output`. The
argument passed to `--subfile-name` is the name of the subfile inside the H5
volume data file where data is stored. In the above example, `--subfile-name`
would be `VolumePsiPiPhiEvery50Slabs`. The argument passed to `--output` is the
desired .xmf file name, also without filename extension. Use `--help` to see a
further description of possible arguments.

Open the .xmf file in ParaView and select the `Xdmf Reader`, *not* the version 3
readers. On the left hand side of the main ParaView window is a section named
`Properties`, here you must click the highlighted `Apply` button. ParaView will
now render your volume data. If you only wish to visualize a few datasets out of
a large set, we recommended unchecking the boxes for the datasets you wish to
ignore under `Point Arrays` before clicking `Apply`. Also, don't forget to check
which dataset is being used for the color under `Coloring`. The default won't
necessarily be the dataset you want.

\note ParaView cannot understand `.xmf` files when subfile names have colons in
them. Please avoid subfile names like `My::Subfile:Name`.

### Helpful ParaView Filters

Here we describe the usage of filters we've found to better visualize our data.
Feel free to contribute to this section!

#### Removing Mesh Imprinting
You may notice what appears to be mesh imprinting on the data. The imprinting
effect can be removed by applying the `Tetrahedralize` filter. To apply the
filter select the `Filters` menu item, then `Alphabetical` and finally
`Tetrahedralize`.

#### Creating Derived Volume Data
New volume data can be created from the existing volume data using the
`Calculator` filter. In the `Calculator`'s text box, input a numerical
expression in terms of existing datasets evaluating to the desired
quantity. For example, a vector-valued velocity dataset can be created
from three scalar velocity component datasets using the expression
`velocity_x * iHat + velocity_y * jHat + velocity_z * kHat` and hitting
the `Apply` button. By default, this will create a new dataset `Result`.
The name of the new dataset can be changed by changing the name provided
to `Result Array Name` above the `Calculator` text box.

#### Visualizing Vector Fields Derived From Scalar Fields
Use the `Calculator` filter described above in "Creating Derived Volume Data"
to create a new vector-valued dataset. Once this is created, use the `Glyph`
filter and set the `Active Attributes` to the vector you wish to visualize.
Make sure that `Scale Mode` is set to `vector`.

### Visualizing and Extracting Dat Files

Global quantities such as error norms are stored in `h5::Dat` subfiles in the
reduction/global HDF5 file. The command-line endpoint `spectre plot-dat`
can be used to plot quantities, and `spectre extract-dat` can be used
to extract them into space-separated text files. These text
files can then be read with other plotting programs or viewed in an editor.

### Reproducibility of Results

Being able to reproduce the results of simulations is important for scientific
integrity and accuracy. To this end, SpECTRE encodes the source tree,
environment variables, and a record of build information (e.g., the third-party
library versions) into the executables. From most executables these can be
retrieved using the flags `--dump-source-as filename_without_extension`,
`--dump-paths`, `--dump-build-info`, and `--dump-environment`. The flag
`--dump-only` will cause the program to exit after the information has been
dumped. You can use the `--help` flag to get more details on what flags are
available. The flag `--dump-paths` will print to screen the environment
variables `PATH`, `CPATH`, `LD_LIBRARY_PATH`, `LIBRARY_PATH`, and
`CMAKE_PREFIX_PATH` at link time. The flag `--dump-build-info` will print to
screen the contents of the `BuildInfo.txt` file generated by CMake. The flag
`--dump-environment` will print to screen the output of `printenv` at link
time. Finally, the flag `--dump-source-as filename_without_extension` will dump
the archive `filename_without_extension.tar.gz`, which contains the entire
source tree at link time. The combined above information should be enough to
reproduce simulations results to roundoff error as long as the third party
libraries used to build the executable have not changed on the system the
simulation was run on.

Often only data from the simulation is retained, not the entire executable. In
order to make simulations reproducible from only the HDF5 output, any HDF5 files
written with SpECTRE's HDF5 wrappers contain the info about the run, output of
`printenv` at link time, the contents of `BuildInfo.txt`, and an archive
of the source tree. These are inside the `header.hdr` group with names
`SimulationInfo`, `EnvironmentVariablesAtCompilation`, `BuildInfo`, and
`src.tar.gz`. The source archive can be extracted by running the command
```
h5dump -d /src.tar.gz -b LE -o src.tar.gz /path/to/hdf5/file.h5
```
You will need to make sure the argument to `-b` is the same endianness as your
computer. In the above example little endian is used.
